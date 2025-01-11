package main

import (
	"fmt"
	"io"
	"math/rand"
	"net"
	"sync"
	"time"
)

func clientSession(clientID, numMessages int, host string, port int, wg *sync.WaitGroup, results chan<- [2]int) {
	defer wg.Done()
	totalBytesSent := 0
	totalBytesReceived := 0

	for i := 0; i < numMessages; i++ {
		conn, err := net.Dial("tcp", fmt.Sprintf("%s:%d", host, port))
		if err != nil {
			fmt.Printf("Client %d error: %v\n", clientID, err)
			return
		}

		message := fmt.Sprintf("Message %d from client %d\n", i, clientID)
		fmt.Printf("Client %d sending: %s", clientID, message)

		n, err := conn.Write([]byte(message))
		if err != nil {
			fmt.Printf("Client %d error: %v\n", clientID, err)
			conn.Close()
			return
		}
		totalBytesSent += n

		buf := make([]byte, 1024)
		n, err = conn.Read(buf)
		if err != nil && err != io.EOF {
			fmt.Printf("Client %d error: %v\n", clientID, err)
			conn.Close()
			return
		}
		totalBytesReceived += n

		fmt.Printf("Client %d received: %s", clientID, string(buf[:n]))

		conn.Close()

		// Random delay between messages
		time.Sleep(time.Duration(rand.Float64()*0.4+0.1) * time.Second)
	}

	results <- [2]int{totalBytesSent, totalBytesReceived}
}

func main() {
	numClients := 1000
	messagesPerClient := 100
	host := "127.0.0.1"
	port := 8080

	var wg sync.WaitGroup
	results := make(chan [2]int, numClients)

	startTime := time.Now()

	for i := 0; i < numClients; i++ {
		wg.Add(1)
		go clientSession(i, messagesPerClient, host, port, &wg, results)
	}

	wg.Wait()
	close(results)

	totalBytesSent := 0
	totalBytesReceived := 0
	for result := range results {
		totalBytesSent += result[0]
		totalBytesReceived += result[1]
	}

	totalMessages := numClients * messagesPerClient
	totalTime := time.Since(startTime).Seconds()
	qps := float64(totalMessages) / totalTime
	totalBytes := totalBytesSent + totalBytesReceived
	bandwidth := float64(totalBytes) / totalTime / (1024 * 1024) // Convert to MB/s

	fmt.Printf("Total messages: %d\n", totalMessages)
	fmt.Printf("Total time: %.2f seconds\n", totalTime)
	fmt.Printf("QPS: %.2f\n", qps)
	fmt.Printf("Total bytes sent: %d\n", totalBytesSent)
	fmt.Printf("Total bytes received: %d\n", totalBytesReceived)
	fmt.Printf("Bandwidth: %.2f MB/s\n", bandwidth)
}