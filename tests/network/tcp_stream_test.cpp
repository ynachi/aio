#include <async_simple/coro/SyncAwait.h>
#include <gtest/gtest.h>
#include <spdlog/common.h>

#include "core/buffer.h"
#include "io/stream.h"
#include "io_context/memory_stream_context.h"

class TcpStreamTest : public ::testing::Test
{
protected:
    MemoryStreamContext memory_stream_context_;
    int server_fd_ = 5;
    TcpStreamTest() : tcp_stream_(server_fd_, "127.0.0.1:8081", "127.0.0.1:8082", memory_stream_context_) { syncAwait(memory_stream_context_.set_fd(5, {})); }

    void SetUp() override {}

    void TearDown() override {}

    aio::Stream tcp_stream_;
};

TEST(BufferTest, SizeReturnsActualDataSize)
{
    Buffer buf(100);
    EXPECT_EQ(buf.size(), 0);
    EXPECT_EQ(buf.capacity(), 100);

    buf.write("hello", 5);
    EXPECT_EQ(buf.size(), 5);
    EXPECT_EQ(buf.capacity(), 100);

    buf.consume(3);
    EXPECT_EQ(buf.size(), 2);
    EXPECT_EQ(buf.capacity(), 100);
}

TEST_F(TcpStreamTest, BasicReadWriteSuccess)
{
    std::string data = "Hello, World! I am a TCP stream!";
    auto write_res = syncAwait(tcp_stream_.write(data));

    ASSERT_TRUE(write_res.has_value());

    std::vector<char> buffer(5);
    auto result = syncAwait(tcp_stream_.read(buffer));
    // we were served from the internal buffer
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), 5);
    ASSERT_EQ(std::string(buffer.data(), 5), "Hello");
    memory_stream_context_.reset_stats(5);
}

TEST_F(TcpStreamTest, BasicReadAllSuccess)
{
    std::string data = "Hello, World! I am a TCP stream!";
    auto write_res = syncAwait(tcp_stream_.write(data));
    ASSERT_TRUE(write_res.has_value());

    std::vector<char> buffer(13);
    syncAwait(memory_stream_context_.set_condition(5, MemoryStreamContext::Condition{.partial_read_write = true}));
    auto result = syncAwait(tcp_stream_.read_all(buffer));
    // we were served from the internal buffer
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), 13);
    ASSERT_EQ(std::string(buffer.data(), 13), "Hello, World!");
    // based on our partial read simulation algo, the underlining read would be called 5 times
    // 13 // 2 = 6, 7 // 2 = 3, 4 // 2 = 2, 2 // 2 = 1, 1
    ASSERT_EQ(memory_stream_context_.get_stats(5).read_count, 5);
    memory_stream_context_.reset_stats(5);
}

TEST_F(TcpStreamTest, BasicReadAllBufferBiggerThanData)
{
    std::string data = "Hello, World! I am a TCP stream!";
    auto write_res = syncAwait(tcp_stream_.write(data));
    ASSERT_TRUE(write_res.has_value());

    std::vector<char> buffer(33);
    syncAwait(memory_stream_context_.set_condition(5, MemoryStreamContext::Condition{.partial_read_write = true}));
    auto result = syncAwait(tcp_stream_.read_all(buffer));
    // we were served from the internal buffer
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), 32);
    ASSERT_EQ(std::string(buffer.data(), 32), "Hello, World! I am a TCP stream!");
    // based on our partial read simulation algo, the underlining read would be called 6 times
    ASSERT_EQ(memory_stream_context_.get_stats(5).read_count, 6);
    memory_stream_context_.reset_stats(5);
}

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::debug);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
