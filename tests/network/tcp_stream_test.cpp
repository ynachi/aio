#include "network/tcp_stream.h"

#include <async_simple/coro/SyncAwait.h>
#include <gtest/gtest.h>
#include <spdlog/common.h>

#include "io_context/memory_stream_context.h"

class TcpStreamTest : public ::testing::Test
{
protected:
    TcpStreamTest() : tcp_stream_(5, std::make_shared<MemoryStreamContext>(), "", "") {}

    void SetUp() override {}

    void TearDown() override {}

    TcpStream tcp_stream_;
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
    std::string data = "Hello, World!";
    auto write_res = async_simple::coro::syncAwait(tcp_stream_.write(data));

    ASSERT_TRUE(write_res.has_value());

    std::vector<char> buffer(5);
    auto result = async_simple::coro::syncAwait(tcp_stream_.read(buffer));
    // we were served from the internal buffer
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), 5);

    // ASSERT_TRUE(result);
    // ASSERT_EQ(result.value(), 5);
    // ASSERT_EQ(std::string(buffer.data(), 5), "Hello");
}

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::debug);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
