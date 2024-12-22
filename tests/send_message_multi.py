import asyncio
import random
import time


async def client_session(client_id: int, num_messages: int, host: str = '127.0.0.1', port: int = 8080):
    for i in range(num_messages):
        try:
            reader, writer = await asyncio.open_connection(host, port)
            message = f"Message {i} from client {client_id}\n"
            print(f'Client {client_id} sending: {message}')

            writer.write(message.encode())
            await writer.drain()

            data = await reader.read(1024)
            print(f'Client {client_id} received: {data.decode(errors="replace")}')

            writer.close()
            await writer.wait_closed()

            # Random delay between messages
            await asyncio.sleep(random.uniform(0.1, 0.5))

        except Exception as e:
            print(f"Client {client_id} error: {e}")


async def main():
    num_clients = 100
    messages_per_client = 10

    tasks = [
        client_session(i, messages_per_client)
        for i in range(num_clients)
    ]

    await asyncio.gather(*tasks)


if __name__ == "__main__":
    start = time.time()
    asyncio.run(main())
    print(f"Total time: {time.time() - start:.2f}s")