#include <async_simple/coro/SyncAwait.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog-inl.h>
#include <sys/socket.h>

#include "io_context/memory_stream_context.h"

class InMemoryIoContextTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Set up code here.
    }

    void TearDown() override
    {
        // Clean up code here.
        ctx.shutdown();
    }

    MemoryStreamContext ctx;
    int fd = 1;  // Example file descriptor
};

TEST_F(InMemoryIoContextTest, BasicAcceptRW)
{
    sockaddr addr{};
    socklen_t addr_len = sizeof(addr);

    const auto client_fd = syncAwait(ctx.async_accept(fd, &addr, &addr_len));
    EXPECT_GE(client_fd, 0);

    std::string msg = "HelloInMemory";
    const auto wres = syncAwait(ctx.async_write(client_fd, std::span(msg), 0));
    EXPECT_EQ(wres, static_cast<int>(msg.size()));

    char buffer[64];
    auto rres = syncAwait(ctx.async_read(client_fd, std::span(buffer), 0));
    EXPECT_EQ(rres, static_cast<int>(msg.size()));
    EXPECT_EQ(std::string_view(buffer, rres), std::string_view(msg));
}

TEST_F(InMemoryIoContextTest, BasicAcceptRWv)
{
    sockaddr addr{};
    socklen_t addr_len = sizeof(addr);

    const auto client_fd = syncAwait(ctx.async_accept(fd, &addr, &addr_len));
    EXPECT_GE(client_fd, 0);

    // Writev
    auto part1 = "ABC";
    auto part2 = "123";
    iovec writeVec[2] = {{const_cast<char*>(part1), strlen(part1)}, {const_cast<char*>(part2), strlen(part2)}};
    auto written = syncAwait(ctx.async_writev(client_fd, writeVec, 2, 0));
    EXPECT_EQ(written, 6);

    // Readv
    char buf1[4]{}, buf2[4]{};
    iovec readVec[2] = {{buf1, sizeof(buf1) - 1}, {buf2, sizeof(buf2) - 1}};
    auto readLen = syncAwait(ctx.async_readv(client_fd, readVec, 2, 0));
    EXPECT_EQ(readLen, 6);
    EXPECT_STREQ(buf1, "ABC");
    EXPECT_STREQ(buf2, "123");
}

TEST_F(InMemoryIoContextTest, NormalSetRead)
{
    std::vector buffer = {'A', 'B', 'C'};
    syncAwait(ctx.set_fd(1200, std::move(buffer)));

    char read_buf[3];
    auto read_len = syncAwait(ctx.async_read(1200, std::span(read_buf), 0));
    EXPECT_EQ(read_len, 3);
    EXPECT_STREQ(read_buf, "ABC");
}

TEST_F(InMemoryIoContextTest, NormalSetWrite)
{
    std::vector<char> buffer(5);
    syncAwait(ctx.set_fd(1201, std::move(buffer)));

    std::vector write_buffer = {'W', 'o', 'r', 'l', 'd'};
    auto write_len = syncAwait(ctx.async_write(1201, std::span(write_buffer), 0));
    EXPECT_EQ(write_len, 5);
}

TEST_F(InMemoryIoContextTest, ReadClosedSocket)
{
    std::vector buffer = {'A', 'B', 'C'};
    syncAwait(ctx.set_fd(900, std::move(buffer)));

    syncAwait(ctx.set_condition(900, MemoryStreamContext::Condition{.is_closed = true}));

    char read_buf[3];
    auto result = syncAwait(ctx.async_read(900, std::span(read_buf), 0));
    EXPECT_EQ(result, -1);
    EXPECT_EQ(errno, EBADF);
}

TEST_F(InMemoryIoContextTest, WriteClosedSocket)
{
    std::vector buffer = {'A', 'B', 'C'};
    syncAwait(ctx.set_fd(901, std::move(buffer)));

    syncAwait(ctx.set_condition(901, MemoryStreamContext::Condition{.is_closed = true}));

    char read_buf[3];
    auto result = syncAwait(ctx.async_write(901, std::span(read_buf), 0));
    EXPECT_EQ(result, -1);
    EXPECT_EQ(errno, EBADF);
}

TEST_F(InMemoryIoContextTest, ReadWithLatency)
{
    std::vector buffer = {'H', 'e', 'l', 'l', 'o'};
    syncAwait(ctx.set_fd(902, std::move(buffer)));

    syncAwait(ctx.set_condition(902, MemoryStreamContext::Condition{.latency = std::chrono::milliseconds(100)}));

    char read_buf[5];
    auto start = std::chrono::steady_clock::now();
    auto result = syncAwait(ctx.async_read(902, std::span(read_buf), 0));
    auto end = std::chrono::steady_clock::now();
    EXPECT_EQ(result, 5);
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 100);
}

TEST_F(InMemoryIoContextTest, ReadWithRandomLatency)
{
    std::vector buffer = {'H', 'e', 'l', 'l', 'o'};
    syncAwait(ctx.set_fd(905, std::move(buffer)));

    syncAwait(ctx.set_random_latency(905, std::chrono::milliseconds(100), std::chrono::milliseconds(500)));

    char read_buf[5];
    auto start = std::chrono::steady_clock::now();
    auto result = syncAwait(ctx.async_read(905, std::span(read_buf), 0));
    auto end = std::chrono::steady_clock::now();
    EXPECT_EQ(result, 5);
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 100);
    EXPECT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 500);
}

TEST_F(InMemoryIoContextTest, PartialReadWrite)
{
    std::vector buffer = {'H', 'e', 'l', 'l', 'o'};
    syncAwait(ctx.set_fd(906, std::move(buffer)));

    syncAwait(ctx.set_condition(906, MemoryStreamContext::Condition{.partial_read_write = true}));

    char read_buf[5];
    auto result = syncAwait(ctx.async_read(906, std::span(read_buf), 0));
    EXPECT_EQ(result, 2);
}

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::debug);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
