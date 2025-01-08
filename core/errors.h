#pragma once

#include <cerrno>
#include <cstdint>
#include <string_view>


enum class AioError : int16_t
{
    Success = 0,
    WouldBlock,
    InvalidFileDescriptor,
    ConnectionAborted,
    InvalidAddress,
    SocketNotListening,
    TooManyOppendFDs,
    NotEnoughMemory,
    InvalidProtocol,
    ProtocolFailure,
    OperationRefused,
    IOUringSQFull,
    EmptyBuffer,
    Unknown
};

inline AioError from_errno(int err)
{
    switch (err)
    {
        case 0:
            return AioError::Success;
        case EAGAIN:
            return AioError::WouldBlock;
        case EBADF:
            return AioError::InvalidFileDescriptor;
        case ECONNABORTED:
            return AioError::ConnectionAborted;
        case EFAULT:
            return AioError::InvalidAddress;
        case EINVAL:
            return AioError::SocketNotListening;
        case EMFILE:
            return AioError::TooManyOppendFDs;
        case ENOMEM:
        case ENOBUFS:
            return AioError::NotEnoughMemory;
        case EOPNOTSUPP:
            return AioError::InvalidProtocol;
        case EPROTO:
            return AioError::ProtocolFailure;
        case EPERM:
            return AioError::OperationRefused;
        case 1000:
            return AioError::IOUringSQFull;
        default:
            return AioError::Unknown;
    }
}

inline std::string_view to_string(AioError err)
{
    switch (err)
    {
        case AioError::Success:
            return "Success";
        case AioError::WouldBlock:
            return "Operation would block on a non-blocking socket or file descriptor";
        case AioError::InvalidFileDescriptor:
            return "This file descriptor is not valid, maybe it was closed or not opened";
        case AioError::ConnectionAborted:
            return "Connection was aborted by the peer";
        case AioError::InvalidAddress:
            return "The  addr  argument  is not in a writable part of the user address space";
        case AioError::SocketNotListening:
            return "Socket is not listening for connections, or addrlen is invalid (e.g., is negative) or Socket is not listening for connections, or addrlen is invalid (e.g., is negative)";
        case AioError::TooManyOppendFDs:
            return "Limit on the number of open file  descriptors has been reached, per process or system-wide";
        case AioError::NotEnoughMemory:
            return "Not enough system memory available or buffer limit reached";
        case AioError::InvalidProtocol:
            return "Operation not supported by the protocol, example accept on a non-SOCK_STREAM socket";
        case AioError::ProtocolFailure:
            return "Protocol error";
        case AioError::OperationRefused:
            return "Operation refused probably due to a firewall rule";
        case AioError::IOUringSQFull:
            return "IOUring submission queue is full";
        case AioError::EmptyBuffer:
            return "EmptyBuffer";
        case AioError::Unknown:
            return "Unknown";
    }
    return "Unknown";
}
