import socket
from datetime import datetime

HOST = "0.0.0.0"
PORT = 9000

def ts() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")

def main():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind((HOST, PORT))
        print(f"[{ts()}] [udp-server] listening on {HOST}:{PORT}")
        while True:
            data, addr = s.recvfrom(4096)
            print(f"[{ts()}] [udp-server] received {len(data)} bytes from {addr}: {data!r}")
            s.sendto(data, addr)
            print(f"[{ts()}] [udp-server] echoed {len(data)} bytes to {addr}")

if __name__ == "__main__":
    main()
