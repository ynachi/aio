//
// Created by vscode on 8/24/25.
//


#include <ylt/coro_http/coro_http_server.hpp>
#include <ylt/coro_http/coro_http_client.hpp>
using namespace coro_http;

async_simple::coro::Lazy<void> basic_usage()
{
    coro_http_server server(1, 9001);
    server.set_http_handler<GET>(
            "/get", [](coro_http_request& req, coro_http_response& resp)
            {
                resp.set_status_and_content(status_type::ok, "ok");
            });

    server.set_http_handler<GET>(
            "/coro",
            [](coro_http_request& req,
               coro_http_response& resp) -> async_simple::coro::Lazy<void>
            {
                resp.set_status_and_content(status_type::ok, "ok");
                co_return;
            });
    server.async_start(); // aync_start() don't block, sync_start() will block.
    // std::this_thread::sleep_for(300ms);  // wait for server start

    coro_http_client client{};
    auto result = co_await client.async_get("http://127.0.0.1:9001/get");
    assert(result.status == 200);
    assert(result.resp_body == "ok");
    for (auto [key, val]: result.resp_headers)
    {
        std::cout << key << ": " << val << "\n";
    }
}

async_simple::coro::Lazy<void> get_post(coro_http_client& client)
{
    std::string uri = "http://www.example.com";
    auto result = co_await client.async_get(uri);
    std::cout << result.status << "\n";

    result = co_await client.async_post(uri, "hello", req_content_type::string);
    std::cout << result.status << "\n";
}

int main()
{
    easylog::init_log(easylog::Severity::INFO);
    coro_http_server server(1, 8092);
    server.set_http_handler<GET>(
            "/", [](coro_http_request& req, coro_http_response& resp)
            {
                resp.set_status_and_content(status_type::ok, "ok");
            });
    server.sync_start();
}
