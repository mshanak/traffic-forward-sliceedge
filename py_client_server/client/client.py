import os
import socket
import time
from datetime import datetime

FORWARDER_HOST = os.getenv("FORWARDER_HOST", "port_forwarder")
FORWARDER_PORT = int(os.getenv("FORWARDER_PORT", "5000"))
MESSAGE = os.getenv("MESSAGE", "hello from UDP client via forwarder\n").encode()
INTERVAL = float(os.getenv("INTERVAL", "1.0"))  # seconds

def ts() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")

def main():
    print(f"[{ts()}] [udp-client] sending to {FORWARDER_HOST}:{FORWARDER_PORT} every {INTERVAL}s")
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.settimeout(2.0)
        while True:
            try:
                s.sendto(MESSAGE, (FORWARDER_HOST, FORWARDER_PORT))
                print(f"[{ts()}] [udp-client] sent: {MESSAGE!r}")
                data, addr = s.recvfrom(4096)
                print(f"[{ts()}] [udp-client] received {len(data)} bytes from {addr}: {data!r}")
            except socket.timeout:
                print(f"[{ts()}] [udp-client] no reply (timeout)")
            time.sleep(INTERVAL)

if __name__ == "__main__":
    main()
