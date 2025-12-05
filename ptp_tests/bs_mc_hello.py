import json
import socket
import struct
from time import sleep

MCAST_GRP = "224.0.126.20"
MCAST_PORT = 4242  # change to 320 if you want the other PTP port

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

# Bind to all interfaces on that port
sock.bind(("", MCAST_PORT))

# Tell the kernel to join the multicast group on all interfaces
mreq = struct.pack("=4sl", socket.inet_aton(MCAST_GRP), socket.INADDR_ANY)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

print(f"Listening on {MCAST_GRP}:{MCAST_PORT} ...")
lastPing = 0
while True:
    sock.sendto(b"Hello from BS MC!", (MCAST_GRP, MCAST_PORT))
    sleep(1)
    sock.sendto(
        json.dumps({"command": "GET SAMPLES", "client_id": "led-0"}).encode(),
        (MCAST_GRP, MCAST_PORT),
    )

    data, addr = sock.recvfrom(2048)
    print("from", addr, "len", len(data))
    sleep(1)
