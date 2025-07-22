import socket
import struct
import argparse

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


def main(host: str, port: int, max_volleys: int):
    if max_volleys >= 128:
        raise ValueError("Maximum volleys must be less than 128")

    bad_msg_count = 0
    sent_msg_count = 0
    with socket.create_connection((host, port)) as sock:
        sock.settimeout(500)

        volley = 0
        for v in range(max_volleys):
            ping_msg = make_ping(volley)
            ping_msg_type, ping_msg_len, ping_msg_volley, ping_msg_payload = parse_pong(ping_msg)
            # print(
            #     f"Sending message: type={ping_msg_type}, len={ping_msg_len}, volley={ping_msg_volley}, payload={ping_msg_payload}")
            sock.sendall(ping_msg)
            sent_msg_count += 1

            try:
                data = sock.recv(struct.calcsize(TOTAL_FMT))
                msg_type, msg_len, msg_volley, msg_payload = parse_pong(data)
                if msg_type != PONG_TYPE or msg_payload != b'pong\0' or msg_volley != volley + 1:
                    print(
                        f"Unexpected response: type={msg_type}, buf={msg_payload}, volley={msg_volley}, expected volley={volley + 1}")
                    bad_msg_count += 1
                    continue

                print(f"Received message: type={msg_type}, len={msg_len}, volley={msg_volley}, payload={msg_payload}")
                volley = msg_volley + 1

            except socket.timeout:
                print("Socket timed out")
                break

    print(f"Completed sending {sent_msg_count} messages. Bad message count: {bad_msg_count}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Ping Pong Client")
    parser.add_argument("--host", type=str, default="localhost", help="Host to connect to")
    parser.add_argument("--port", type=int, default=1337, help="Port to connect to")
    parser.add_argument("--max-volleys", type=int, default=50, help="Maximum number of ping/pong volleys to send")
    args = parser.parse_args()

    main(args.host, args.port, args.max_volleys)
