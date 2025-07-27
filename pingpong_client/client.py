import socket
import struct
import argparse
import random
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

random.seed(1337)

PING_TYPE = 0
PONG_TYPE = 1
PING_PONG_ERR = 2

TOTAL_FMT = "!BHB5s"
HEADER_FMT = "!BH"
PINGPONG_FMT = "!B5s"


def make_ping(volley: int) -> bytes:
    header: bytes = struct.pack(HEADER_FMT, PING_TYPE, struct.calcsize(PINGPONG_FMT))
    payload: bytes = struct.pack(PINGPONG_FMT, volley, b"ping")

    return header + payload


def parse_pong(data: bytes):
    if len(data) < struct.calcsize(TOTAL_FMT):
        print("Not enough data to parse")
        return None

    msg_type, msg_len, msg_volley, msg_payload = struct.unpack(TOTAL_FMT, data)
    return msg_type, msg_len, msg_volley, msg_payload


def main(id: int, host: str, port: int, max_volleys: int, jitter: bool = False):
    if max_volleys >= 128:
        raise ValueError("Maximum volleys must be less than 128")

    bad_msg_count = 0
    sent_msg_count = 0
    with socket.create_connection((host, port)) as sock:
        sock.settimeout(500)

        volley = 0
        while volley < max_volleys:
            if jitter:
                time.sleep(random.uniform(0.0, 0.05))
            ping_msg = make_ping(volley)

            sock.sendall(ping_msg)
            sent_msg_count += 1

            try:
                data = sock.recv(struct.calcsize(TOTAL_FMT))
                msg_type, msg_len, msg_volley, msg_payload = parse_pong(data)
                if msg_type != PONG_TYPE or msg_payload != b'pong\0' or msg_volley != volley + 1:
                    print(f"Client {id} failed message")
                    bad_msg_count += 1
                    continue

                volley = msg_volley + 1

            except socket.timeout:
                print("Socket timed out")
                break

    return id, bad_msg_count


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Ping Pong Client")
    parser.add_argument("--host", type=str, default="localhost", help="Host to connect to")
    parser.add_argument("--port", type=int, default=1337, help="Port to connect to")
    parser.add_argument("--clients", type=int, default=1, help="Number of clients to run concurrently")
    parser.add_argument("--client-jitter", action="store_true", help="Add random jitter to client comms")
    args = parser.parse_args()

    failed_msgs = 0
    futures = []

    now = time.time()
    with ThreadPoolExecutor(max_workers=args.clients) as executor:
        for i in range(args.clients):
            max_volleys = random.randint(1, 127)
            future = executor.submit(main, i, args.host, args.port, max_volleys, args.client_jitter)
            futures.append(future)
            time.sleep(random.uniform(0.0, 1.0))

        for i, future in enumerate(as_completed(futures)):
            try:
                c_id, failed_msgs = future.result()
                print(f"Client {c_id} failed {failed_msgs} messages")
            except Exception as e:
                print(f"Client {i} failed with exception: {e}")
                failed_msgs += 1
                continue

    end = time.time()
    print(f"Total time: {end - now}")
