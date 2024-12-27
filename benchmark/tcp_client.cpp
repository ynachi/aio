#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <atomic>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

// Constants
constexpr size_t DATASIZE = 1500; // Size of data to send per message

// Global variables
std::atomic<long> packageProcessed(0);
std::atomic recvDataLen(DATASIZE);

void reportProcessedPackages() {
    using namespace std::chrono_literals;

    long previousSec = 0;

    while (true) {
        std::this_thread::sleep_for(1s); // Sleep for 1 second
        auto currentSec = packageProcessed.load();
        auto processedInLastSec = currentSec - previousSec;

        // Calculate bandwidth in Mbps
        auto bandwidthMbps = (processedInLastSec * recvDataLen.load() * 8) / (1024 * 1024);

        std::cout << "Processed in Last Second: " << processedInLastSec
                  << " packages, Bandwidth: " << bandwidthMbps << " Mbps"
                  << ", Total Packages: " << currentSec << std::endl;

        previousSec = currentSec;
    }
}

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <IPv4 or IPv6 address:port>" << std::endl;
            return EXIT_FAILURE;
        }

        // Parse address and port
        std::string input = argv[1];
        auto colonPos = input.find_last_of(':');
        if (colonPos == std::string::npos) {
            throw std::runtime_error("Invalid format. Expected <address:port>");
        }

        std::string address = input.substr(0, colonPos);
        int port = std::stoi(input.substr(colonPos + 1));

        // Set up server address (IPv6 with potential IPv4-mapping)
        sockaddr_in6 server_addr{};
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin6_family = AF_INET6;
        server_addr.sin6_port = htons(port);

        if (inet_pton(AF_INET, address.c_str(), &server_addr.sin6_addr.s6_addr[12]) == 1) {
            std::cout << "Assuming IPv4 address" << std::endl;
            memset(&server_addr.sin6_addr.s6_addr[0], 0, 10);
            memset(&server_addr.sin6_addr.s6_addr[10], 0xff, 2);
        } else {
            std::cout << "Assuming IPv6 address" << std::endl;
            if (inet_pton(AF_INET6, address.c_str(), &server_addr.sin6_addr) != 1) {
                throw std::runtime_error("Invalid address");
            }
        }

        // Create TCP socket
        int sock_fd = socket(AF_INET6, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            perror("Socket creation failed");
            return EXIT_FAILURE;
        }

        // Connect to the server
        if (connect(sock_fd, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
            perror("Connection to server failed");
            close(sock_fd);
            return EXIT_FAILURE;
        }
        std::cout << "Connected to server " << address << ":" << port << std::endl;

        // Prepare a random buffer of data to send
        std::vector<char> sendBuffer(DATASIZE);
        for (char &c : sendBuffer) {
            c = static_cast<char>(std::rand());
        }

        // Launch reporting thread
        std::jthread reportThread(reportProcessedPackages);

        // Main sending/receiving loop
        while (true) {
            // Send the data
            ssize_t bytes_sent = send(sock_fd, sendBuffer.data(), sendBuffer.size(), 0);
            if (bytes_sent < 0) {
                perror("Send failed");
                close(sock_fd);
                return EXIT_FAILURE;
            }

            // Receive response
            std::vector<char> recvBuffer(DATASIZE); // Assuming the server echoes data of the same size
            ssize_t bytes_received = recv(sock_fd, recvBuffer.data(), recvBuffer.size(), 0);
            if (bytes_received < 0) {
                perror("Receive failed");
                close(sock_fd);
                return EXIT_FAILURE;
            }

            // Update statistics
            recvDataLen = bytes_received;
            ++packageProcessed;

            // Optional: Print received data (for debugging)
            // std::cout << "Received: " << std::string(recvBuffer.begin(), recvBuffer.begin() + bytes_received) << std::endl;
        }

        // Cleanup (unreachable in this example, but good practice)
        reportThread.join();
        close(sock_fd);

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}