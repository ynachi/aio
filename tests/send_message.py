import socket
import time


def send_message(message: str, host: str = '127.0.0.1', port: int = 8080):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    print(f'Sending: {message}')

    sock.send(message.encode())
    data = sock.recv(1024)
    print(f'Received: {data.decode()}')

    sock.close()


def main():
    messages = [
        "+hello\r\n",
        "$5\r\nworld\r\n",
        "test message\n",
    ]

    for msg in messages:
        send_message(msg)


if __name__ == "__main__":
    start = time.time()
    main()
    print(f"Total time: {time.time() - start:.2f}s")