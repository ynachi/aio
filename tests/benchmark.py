import asyncio
import time
import random

async def client_session(client_id: int, num_messages: int, host: str = '127.0.0.1', port: int = 8080):
    for i in range(num_messages):
        try:
            reader, writer = await asyncio.open_connection(host, port)
            message = f"Message {i} from client {client_id}\n"
            writer.write(message.encode())
            await writer.drain()

            data = await reader.read(1024)
            writer.close()
            await writer.wait_closed()

            await asyncio.sleep(random.uniform(0.1, 0.5))

        except Exception as e:
            print(f"Client {client_id} error: {e}")

async def benchmark(num_clients: int, messages_per_client: int):
    tasks = [client_session(i, messages_per_client) for i in range(num_clients)]
    start_time = time.time()
    await asyncio.gather(*tasks)
    end_time = time.time()

    total_messages = num_clients * messages_per_client
    total_time = end_time - start_time
    qps = total_messages / total_time

    print(f"Total messages: {total_messages}")
    print(f"Total time: {total_time:.2f} seconds")
    print(f"QPS: {qps:.2f}")

if __name__ == "__main__":
    num_clients = 100
    messages_per_client = 100
    asyncio.run(benchmark(num_clients, messages_per_client))