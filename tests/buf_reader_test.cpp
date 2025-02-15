//
// Created by ynachi on 2/15/25.
//
#include <async_simple/coro/SyncAwait.h>
#include <gtest/gtest.h>

#include "io/stream.h"
#include "io_context/memory_stream_context.h"

class ReaderTest : public ::testing::Test
{
protected:
    MemoryStreamContext memory_stream_context_;
    int server_fd_ = 5;
    ReaderTest() : tcp_stream_(server_fd_, "127.0.0.1:8081", "127.0.0.1:8082", memory_stream_context_), rd_(tcp_stream_) { syncAwait(memory_stream_context_.set_fd(5, {})); }

    void SetUp() override {}

    void TearDown() override {}

    aio::Stream tcp_stream_;
    aio::Reader rd_;
};

TEST_F(ReaderTest, BasicPeek)
{
    std::string data = "Hello, World! I am a TCP stream!";
    auto write_res = syncAwait(tcp_stream_.write(data));

    ASSERT_TRUE(write_res.has_value());

    auto res = syncAwait(rd_.peek(5));
    ASSERT_TRUE(res.has_value());
    auto res_value = res.value();
    ASSERT_EQ(std::string(res_value.begin(), res_value.end()), "Hello");
    ASSERT_EQ(rd_.available_in_buffer(), 32) << "Peek should not advance read cursor or consume the buffer";
    ASSERT_EQ(memory_stream_context_.get_stats(5).read_count, 1) << "Peek should call underlined read when not enough data in the buffer";

    auto res2 = syncAwait(rd_.peek(5));
    ASSERT_TRUE(res2.has_value());
    auto res2_value = res2.value();
    ASSERT_EQ(std::string(res2_value.begin(), res2_value.end()), "Hello");
    ASSERT_EQ(rd_.available_in_buffer(), 32) << "Peek should not advance read cursor or consume the buffer";
    ASSERT_EQ(memory_stream_context_.get_stats(5).read_count, 1) << "Peek should not call underlined read when there is enough data in the buffer";

    // std::vector<char> buffer(5);
    // auto result = syncAwait(tcp_stream_.read(buffer));
    // // we were served from the internal buffer
    // ASSERT_TRUE(result.has_value());
    // ASSERT_EQ(result.value(), 5);
    // ASSERT_EQ(std::string(buffer.data(), 5), "Hello");
    // memory_stream_context_.reset_stats(5);
}

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::debug);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
