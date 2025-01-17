# aio

Let's experiment io_uring and async network programming in C++

This is a work in progress. The goal is to create a simple and efficient async io library in C++ using io_uring.
The library supports files operations and network sockets. We use alibaba async_simple to take care of the coroutine
machinery.

# How to build

## First, install liburing 2.8 and pkg-config

```shell
sudo apt install pkg-config
wget https://github.com/axboe/liburing/archive/refs/tags/liburing-2.8.tar.gz -O - | tar -xvz
cd liburing-liburing-2.8
./configure --prefix=/usr/local
make -C src -j $(nprocs)
sudo make install
```

## Clone the repo with submodules

```shell
git clone --recursive  https://github.com/ynachi/aio.git
```

## build

```shell
# example, build debug
cmake --workflow --preset debug
# after that, you will find binaries in build/debug/bin folder
```

# Demos/examples

This minimal version has been demoed with a simple echo server and a simple file read program.

1. [echo server](./demos/low_level_tcp_server): demonstrates the usage of the low level io_uring wrapper to create an
   async TCP server.
2. [file read](./demos/char_count_low.cpp): demonstrates the usage of the low level io_uring wrapper to read a file.
3. [echo_server_202](./demos/tcp_server_202.cpp): demonstrates the usage of higher level classes to build a TCP server.
   In this example, the user still need to run the event loop himself.

Bemchmarks:

Example with apache benchmark on a simple echo server.:  
The server running on a single core on AMD Ryzen 7 PRO 7840U with 64GB of DDR5 ram

```shell
➜  ~ ulimit -n 65535
➜  ~ ab -n 10000000 -c 1000 -k http://127.0.0.1:8080/
This is ApacheBench, Version 2.3 <$Revision: 1903618 $>
Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/
Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking 127.0.0.1 (be patient)
Completed 1000000 requests
Completed 2000000 requests
Completed 3000000 requests
Completed 4000000 requests
Completed 5000000 requests
Completed 6000000 requests
Completed 7000000 requests
Completed 8000000 requests
Completed 9000000 requests
Completed 10000000 requests
Finished 10000000 requests


Server Software:        
Server Hostname:        127.0.0.1
Server Port:            8080

Document Path:          /
Document Length:        0 bytes

Concurrency Level:      1000
Time taken for tests:   63.208 seconds
Complete requests:      10000000
Failed requests:        0
Non-2xx responses:      10000000
Keep-Alive requests:    10000000
Total transferred:      1060000000 bytes
HTML transferred:       0 bytes
Requests per second:    158206.65 [#/sec] (mean)
Time per request:       6.321 [ms] (mean)
Time per request:       0.006 [ms] (mean, across all concurrent requests)
Transfer rate:          16376.86 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.3      0      41
Processing:     2    6   1.2      6      40
Waiting:        0    6   1.2      6      23
Total:          2    6   1.3      6      63

Percentage of the requests served within a certain time (ms)
  50%      6
  66%      6
  75%      6
  80%      6
  90%      7
  95%      8
  98%      9
  99%     15
 100%     63 (longest request)
```
