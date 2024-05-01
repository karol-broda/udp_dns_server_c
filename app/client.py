import socket
import struct


def create_dns_query(domain):
    transaction_id = 0x0A0B
    flags = 0x0100
    questions = 1
    answer_rrs = 0
    authority_rrs = 0
    additional_rrs = 0

    header = struct.pack(
        ">HHHHHH",
        transaction_id,
        flags,
        questions,
        answer_rrs,
        authority_rrs,
        additional_rrs,
    )

    qname = b""
    for part in domain.split("."):
        qname += struct.pack("B", len(part)) + part.encode()
    qname += b"\x00"
    qtype = struct.pack(">H", 1)
    qclass = struct.pack(">H", 1)
    print("Encoded qname:", qname)

    question = qname + qtype + qclass

    return header + question


def send_dns_query(server_ip, server_port, domain):
    query = create_dns_query(domain)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(query, (server_ip, server_port))

    response, _ = sock.recvfrom(512)
    sock.close()

    return response


def main():
    server_ip = "127.0.0.1"
    server_port = 2053
    domain = "google.com"
    response = send_dns_query(server_ip, server_port, domain)
    print(f"res: {response}")


if __name__ == "__main__":
    main()
