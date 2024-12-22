import socket
import time


def send_message(message: str, host: str = '127.0.0.1', port: int = 8080) -> str:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    print(f'Sending: {message}')

    sock.send(message.encode())
    data = sock.recv(1024)
    print(f'Received: {data.decode()}')

    sock.close()
    return data.decode()


def main():
    messages = [
        "hello",
        "world",
        "test message",
    ]

    for i, msg in enumerate(messages):
        data = send_message(msg)
        assert data == messages[i]


if __name__ == "__main__":
    start = time.time()
    main()
    print(f"Total time: {time.time() - start:.2f}s")