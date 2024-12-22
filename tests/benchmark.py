import asyncio
import time
import random

async def client_session(client_id: int, num_messages: int, host: str = '127.0.0.1', port: int = 8080):
    total_bytes_sent = 0
    total_bytes_received = 0
    for i in range(num_messages):
        try:
            reader, writer = await asyncio.open_connection(host, port)
            message = f"Message {i} from client {client_id}\n"
            writer.write(message.encode())
            await writer.drain()
            total_bytes_sent += len(message)

            data = await reader.read(1024)
            total_bytes_received += len(data)
            writer.close()
            await writer.wait_closed()

            await asyncio.sleep(random.uniform(0.1, 0.5))

        except Exception as e:
            print(f"Client {client_id} error: {e}")

    return total_bytes_sent, total_bytes_received

async def benchmark(num_clients: int, messages_per_client: int):
    tasks = [client_session(i, messages_per_client) for i in range(num_clients)]
    start_time = time.time()
    results = await asyncio.gather(*tasks)
    end_time = time.time()

    total_messages = num_clients * messages_per_client
    total_time = end_time - start_time
    qps = total_messages / total_time

    total_bytes_sent = sum(result[0] for result in results)
    total_bytes_received = sum(result[1] for result in results)
    total_bytes = total_bytes_sent + total_bytes_received
    bandwidth = total_bytes / total_time / (1024 * 1024)  # Convert to MB/s

    print(f"Total messages: {total_messages}")
    print(f"Total time: {total_time:.2f} seconds")
    print(f"QPS: {qps:.2f}")
    print(f"Total bytes sent: {total_bytes_sent}")
    print(f"Total bytes received: {total_bytes_received}")
    print(f"Bandwidth: {bandwidth:.2f} MB/s")

if __name__ == "__main__":
    num_clients = 100
    messages_per_client = 100
    asyncio.run(benchmark(num_clients, messages_per_client))