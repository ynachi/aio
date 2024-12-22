# aio
Let's experiment io_uring and async network programming in C++

This is a work in progress. The goal is to create a simple and efficient async io library in C++ using io_uring.
The library supports files operations and network sockets. We use alibaba async_simple to take care of the coroutine
machinery.  

This minimal version has been demoed with a simple echo server and a simple file read program.
1. [echo server](./demo/echo_server.cpp)
2. [file read](./demo/file_read.cpp)
