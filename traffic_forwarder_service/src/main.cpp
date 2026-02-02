// UDP-only Port Forwarder in C++ (Linux) with DSCP Mapping
// Build: g++ -std=c++17 -O2 -pthread -o udp_forwarder udp_forwarder.cpp
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <streambuf>
#include <random>
#include <algorithm>

using namespace std::chrono_literals;

static std::atomic<bool> g_running{true};
static std::atomic<bool> g_log_enabled{true};

// ---- null stream + logging shims ----
struct NullBuffer : public std::streambuf {
  int overflow(int c) override { return traits_type::not_eof(c); }
};
static NullBuffer g_nullbuf;
static std::ostream g_nullout(&g_nullbuf);
#define LOG_OUT (g_log_enabled.load() ? std::cout : g_nullout)
#define LOG_ERR (g_log_enabled.load() ? std::cerr : g_nullout)
// ---------------------------------------------

void on_signal(int sig) {
  if (sig == SIGINT) {
    LOG_OUT << "\n[INFO] Caught Ctrl+C (SIGINT). Shutting down..." << std::endl;
  } else if (sig == SIGTERM) {
    LOG_OUT << "\n[INFO] Caught SIGTERM. Shutting down..." << std::endl;
  } else if (sig == SIGQUIT) {
    LOG_OUT << "\n[INFO] Caught SIGQUIT. Shutting down..." << std::endl;
  }
  g_running = false;
}

enum class MappingMethod {
  STATIC,      // Use fixed DSCP for each port
  ROUND_ROBIN, // Cycle through DSCP values for each packet
  RANDOM       // Random DSCP value for each packet
};

struct Target {
  std::string host;
  uint16_t port{};
  uint8_t dscp{};  // DSCP value (0-63)
};

struct UdpMap { uint16_t listen_port{}; Target target; };

struct CliConfig {
  std::string bind_ip = "0.0.0.0";
  std::vector<UdpMap> udp;
  MappingMethod mapping_method = MappingMethod::STATIC;
};

// Global DSCP pool for dynamic mapping methods
struct DscpPool {
  std::vector<uint8_t> dscp_values;
  std::atomic<size_t> round_robin_index{0};
  std::mutex mutex;
  std::random_device rd;
  std::mt19937 gen{std::random_device{}()};

  uint8_t get_dscp(MappingMethod method, uint8_t static_dscp) {
    if (method == MappingMethod::STATIC) return static_dscp;

    std::lock_guard<std::mutex> lock(mutex);
    if (dscp_values.empty()) return 0;

    switch (method) {
      case MappingMethod::ROUND_ROBIN: {
        size_t idx = round_robin_index.fetch_add(1) % dscp_values.size();
        return dscp_values[idx];
      }
      case MappingMethod::RANDOM: {
        std::uniform_int_distribution<size_t> dist(0, dscp_values.size() - 1);
        return dscp_values[dist(gen)];
      }
      default:
        return static_dscp;
    }
  }

  void add_dscp(uint8_t dscp) {
    std::lock_guard<std::mutex> lock(mutex);
    dscp_values.push_back(dscp);
  }

  void finalize() {
    std::lock_guard<std::mutex> lock(mutex);
    std::sort(dscp_values.begin(), dscp_values.end());
    dscp_values.erase(std::unique(dscp_values.begin(), dscp_values.end()), dscp_values.end());
  }
};

static DscpPool g_dscp_pool;

static void die(const std::string &msg) {
  std::cerr << msg << ": " << std::strerror(errno) << std::endl;
  std::exit(1);
}

static int set_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int set_dscp(int fd, uint8_t dscp) {
  // DSCP occupies the upper 6 bits of the TOS/Traffic Class byte => TOS = DSCP << 2
  int tos = (dscp & 0x3F) << 2;
  if (setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
    LOG_ERR << "[WARN] Failed to set DSCP value " << (int)dscp
            << " (TOS=" << tos << "): " << std::strerror(errno) << std::endl;
    return -1;
  }
  return 0;
}

static std::optional<sockaddr_in> parse_sockaddr(const std::string &ip, uint16_t port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
    LOG_ERR << "Invalid IPv4 address: " << ip << std::endl;
    return std::nullopt;
  }
  return addr;
}

static void handle_udp_port(const std::string &bind_ip, const UdpMap &m, MappingMethod method) {
  int ufd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (ufd < 0) die("socket(UDP)");
  set_nonblock(ufd);
  int yes = 1;
  ::setsockopt(ufd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  auto laddr = parse_sockaddr(bind_ip, m.listen_port);
  if (!laddr) std::exit(2);
  if (::bind(ufd, reinterpret_cast<const sockaddr*>(&*laddr), sizeof(sockaddr_in)) < 0)
    die("bind(UDP)");

  auto taddrOpt = parse_sockaddr(m.target.host, m.target.port);
  if (!taddrOpt) std::exit(2);
  const sockaddr_in taddr = *taddrOpt;

  if (method == MappingMethod::STATIC) {
    LOG_OUT << "[UDP] One-way forwarding (preserve src port) on "
            << bind_ip << ":" << m.listen_port
            << " -> " << m.target.host << ":" << m.target.port
            << " (DSCP=" << (int)m.target.dscp << ")" << std::endl;
  } else {
    const char* method_name = (method == MappingMethod::ROUND_ROBIN) ? "Round-Robin" : "Random";
    LOG_OUT << "[UDP] " << method_name << " DSCP (preserve src port) on "
            << bind_ip << ":" << m.listen_port
            << " -> " << m.target.host << ":" << m.target.port
            << " (DSCP pool: " << g_dscp_pool.dscp_values.size() << " values)" << std::endl;
  }

  struct Sender { int fd; std::chrono::steady_clock::time_point last; };
  std::unordered_map<uint16_t, Sender> senders;
  std::mutex senders_mx;
  const auto idle_timeout = 60s;

  std::vector<char> buf(64 * 1024);
  while (g_running) {
    sockaddr_in caddr{}; socklen_t clen = sizeof(caddr);
    ssize_t n = ::recvfrom(ufd, buf.data(), buf.size(), 0,
                           reinterpret_cast<sockaddr*>(&caddr), &clen);
    if (n < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK) { std::this_thread::sleep_for(2ms); continue; }
      LOG_ERR << "[UDP] recvfrom(client) error: " << std::strerror(errno) << "\n";
      continue;
    }

    // Choose DSCP for this packet
    uint8_t selected_dscp = g_dscp_pool.get_dscp(method, m.target.dscp);

    char addrbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &caddr.sin_addr, addrbuf, sizeof(addrbuf));
    uint16_t sport = ntohs(caddr.sin_port);

    const char* method_name = method == MappingMethod::STATIC ? "Static" :
                             (method == MappingMethod::ROUND_ROBIN ? "RR" : "Random");
    LOG_OUT << "[UDP] " << method_name << ": " << n << " bytes from "
            << addrbuf << ":" << sport << " -> target "
            << m.target.host << ":" << m.target.port
            << " (DSCP=" << (int)selected_dscp << ", preserve src port)" << std::endl;

    int sfd = -1;
    auto now = std::chrono::steady_clock::now();

    {
      std::lock_guard<std::mutex> lk(senders_mx);

      // GC old senders
      for (auto it = senders.begin(); it != senders.end(); ) {
        if (now - it->second.last > idle_timeout) {
          ::close(it->second.fd);
          it = senders.erase(it);
        } else {
          ++it;
        }
      }

      // Find or create a sender bound to bind_ip:sport
      auto it = senders.find(sport);
      if (it == senders.end()) {
        int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
          LOG_ERR << "[UDP] socket(sender) error: " << std::strerror(errno) << "\n";
        } else {
          set_nonblock(fd);
          int yes2 = 1;
          ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes2, sizeof(yes2));

          // Set DSCP on the sender socket
          set_dscp(fd, selected_dscp);

          auto saddrOpt = parse_sockaddr(bind_ip, sport);
          if (!saddrOpt) {
            LOG_ERR << "[UDP] invalid bind_ip for sender\n";
            ::close(fd);
          } else if (::bind(fd, reinterpret_cast<const sockaddr*>(&*saddrOpt), sizeof(sockaddr_in)) < 0) {
            LOG_ERR << "[UDP] bind(sender " << bind_ip << ":" << sport << ") failed: "
                    << std::strerror(errno)
                    << " (cannot preserve port; already in use?)\n";
            ::close(fd);
          } else if (::connect(fd, reinterpret_cast<const sockaddr*>(&taddr), sizeof(sockaddr_in)) < 0) {
            LOG_ERR << "[UDP] connect(sender->target) failed: " << std::strerror(errno) << "\n";
            ::close(fd);
          } else {
            senders.emplace(sport, Sender{fd, now});
            sfd = fd;
          }
        }
      } else {
        sfd = it->second.fd;
        it->second.last = now;

        // For dynamic DSCP modes, update DSCP on existing socket
        if (method != MappingMethod::STATIC) {
          set_dscp(sfd, selected_dscp);
        }
      }
    } // lock released

    if (sfd >= 0) {
      ssize_t sent = ::send(sfd, buf.data(), n, 0);
      if (sent < 0) {
        LOG_ERR << "[UDP] send(target) error: " << std::strerror(errno) << "\n";
      }
    }
  }

  {
    std::lock_guard<std::mutex> lk(senders_mx);
    for (auto &kv : senders) ::close(kv.second.fd);
    senders.clear();
  }
  ::close(ufd);
}

static std::string mapping_method_to_string(MappingMethod method) {
  switch (method) {
    case MappingMethod::STATIC: return "static";
    case MappingMethod::ROUND_ROBIN: return "round-robin";
    case MappingMethod::RANDOM: return "random";
    default: return "unknown";
  }
}

static std::optional<MappingMethod> string_to_mapping_method(const std::string& str) {
  std::string lower_str = str;
  std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
  if (lower_str == "static") return MappingMethod::STATIC;
  if (lower_str == "round-robin" || lower_str == "roundrobin" || lower_str == "rr")
    return MappingMethod::ROUND_ROBIN;
  if (lower_str == "random" || lower_str == "rand") return MappingMethod::RANDOM;
  return std::nullopt;
}

static void print_usage_and_exit(const std::string& bad_arg = "") {
  if (!bad_arg.empty()) LOG_ERR << "Unknown/invalid argument: " << bad_arg << "\n";
  LOG_ERR
    << "Usage: ./udp_forwarder --bind <ip> [--quiet | --log on|off] "
       "[--mapping-method static|round-robin|random] "
       "udp:<lport>=<host>:<rport>[:<DSCP>] ...\n\n"
    << "Mapping methods:\n"
    << "  static      : Use the specified DSCP for each port mapping (default)\n"
    << "  round-robin : Cycle through all DSCP values from all mappings\n"
    << "  random      : Randomly select DSCP from all values in all mappings\n\n"
    << "Notes:\n"
    << "  * DSCP value: 0-63 (optional, default=0)\n"
    << "  * Only UDP mappings are supported (e.g., udp:5000=1.2.3.4:5000:46)\n";
  std::exit(1);
}

static std::optional<CliConfig> parse_args(int argc, char **argv) {
  CliConfig cfg;
  // Only accept udp:<lport>=<host>:<rport>[:<dscp>]
  std::regex re_udp(R"(^([uU][dD][pP]):(\d{1,5})=([^:]+):(\d{1,5})(?::(\d{1,2}))?$)");
  // Explicitly detect tcp:* to give a clear error
  std::regex re_tcp_prefix(R"(^([tT][cC][pP]):)");

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];

    // logging
    if (a == "--quiet") { g_log_enabled = false; continue; }
    if (a == "--log" && i + 1 < argc) {
      std::string v = argv[++i];
      for (auto &c : v) c = std::tolower(c);
      if (v == "0" || v == "off" || v == "false" || v == "no") g_log_enabled = false;
      else g_log_enabled = true;
      continue;
    }

    // mapping method
    if (a == "--mapping-method" && i + 1 < argc) {
      std::string method_str = argv[++i];
      auto method_opt = string_to_mapping_method(method_str);
      if (!method_opt) {
        LOG_ERR << "Invalid mapping method: " << method_str
                << ". Valid: static, round-robin, random" << std::endl;
        return std::nullopt;
      }
      cfg.mapping_method = *method_opt;
      continue;
    }

    if (a == "--bind" && i + 1 < argc) {
      cfg.bind_ip = argv[++i];
      continue;
    }

    // mappings
    if (std::regex_search(a, re_tcp_prefix)) {
      LOG_ERR << "TCP mappings are not supported. Remove '" << a << "' or convert to UDP.\n";
      return std::nullopt;
    }

    std::smatch m;
    if (std::regex_match(a, m, re_udp)) {
      int lport = std::stoi(m[2]);
      std::string host = m[3];
      int rport = std::stoi(m[4]);

      int dscp = 0;
      if (m[5].matched) {
        dscp = std::stoi(m[5]);
        if (dscp < 0 || dscp > 63) {
          LOG_ERR << "Invalid DSCP value in mapping (must be 0-63): " << a << std::endl;
          return std::nullopt;
        }
      }

      if (lport <= 0 || lport > 65535 || rport <= 0 || rport > 65535) {
        LOG_ERR << "Invalid port in mapping: " << a << std::endl;
        return std::nullopt;
      }

      Target t{host, static_cast<uint16_t>(rport), static_cast<uint8_t>(dscp)};
      if (cfg.mapping_method != MappingMethod::STATIC) {
        g_dscp_pool.add_dscp(static_cast<uint8_t>(dscp));
      }
      cfg.udp.push_back(UdpMap{static_cast<uint16_t>(lport), t});
    } else {
      print_usage_and_exit(a);
      return std::nullopt;
    }
  }

  if (cfg.udp.empty()) {
    LOG_ERR << "No UDP mappings provided.\n";
    print_usage_and_exit();
    return std::nullopt;
  }

  // Finalize DSCP pool (sort and deduplicate)
  if (cfg.mapping_method != MappingMethod::STATIC) {
    g_dscp_pool.finalize();
    if (!g_dscp_pool.dscp_values.empty()) {
      LOG_OUT << "[INFO] DSCP mapping method: " << mapping_method_to_string(cfg.mapping_method)
              << "\n[INFO] DSCP pool (" << g_dscp_pool.dscp_values.size() << " values): [";
      for (size_t i = 0; i < g_dscp_pool.dscp_values.size(); ++i) {
        if (i > 0) LOG_OUT << ", ";
        LOG_OUT << (int)g_dscp_pool.dscp_values[i];
      }
      LOG_OUT << "]" << std::endl;
    }
  }

  return cfg;
}

int main(int argc, char **argv) {
  ::signal(SIGINT, on_signal);
  ::signal(SIGTERM, on_signal);
  ::signal(SIGQUIT, on_signal);
  ::signal(SIGPIPE, SIG_IGN);

  auto cfgOpt = parse_args(argc, argv);
  if (!cfgOpt) return 1;
  auto cfg = *cfgOpt;

  std::vector<std::thread> threads;
  threads.reserve(cfg.udp.size());

  for (const auto &m : cfg.udp) {
    threads.emplace_back(handle_udp_port, cfg.bind_ip, m, cfg.mapping_method);
  }

  for (auto &t : threads) t.join();
  LOG_OUT << "[INFO] Program exited cleanly." << std::endl;
  return 0;
}
