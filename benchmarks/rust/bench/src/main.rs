use std::error::Error;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    // Bind the server to localhost on port 8080
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("Echo server listening on 127.0.0.1:8080");

    // Accept incoming connections in a loop
    loop {
        // Wait for a new connection
        let (mut socket, addr) = listener.accept().await?;
        println!("New connection: {}", addr);

        // Spawn a new task to handle the connection
        tokio::spawn(async move {
            let mut buf = [0; 1024];

            // Read data from the client and echo it back
            loop {
                match socket.read(&mut buf).await {
                    Ok(0) => {
                        // Connection was closed by the client
                        println!("Client disconnected: {}", addr);
                        break;
                    }
                    Ok(n) => {
                        // Echo the data back to the client
                        if let Err(e) = socket.write_all(&buf[..n]).await {
                            eprintln!("Failed to write to socket: {}", e);
                            break;
                        }
                    }
                    Err(e) => {
                        eprintln!("Failed to read from socket: {}", e);
                        break;
                    }
                }
            }
        });
    }
}
