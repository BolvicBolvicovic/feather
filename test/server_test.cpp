/*--- Code file for test server ---*/

#include <server_test.hpp>

// Router is initialized in Router_init_test

using namespace feather::core;

void Server_parse_request_test(void)
{

    std::string raw_request =
    "GET /api/products/1234 HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n"
    "Accept: application/json\r\n"
    "Accept-Language: en-US,en;q=0.9\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Connection: keep-alive\r\n"
    "Cookie: session_id=abc123; user_preferences=dark_mode\r\n"
    "Cache-Control: no-cache\r\n\r\n"
    "This is the body";

    auto result = Server::parse_request(raw_request);
    assert(result.first == ResultType::Ok);
    
    auto& request = result.second;
    assert(request.body == "This is the body");
    assert(request.get_header_value("Host") == "example.com");
    assert(request.version == "HTTP/1.1");
    assert(request.method == "GET");
    assert(request.path == "/api/products/1234");

}

void run_server_test(void)
{
    std::function<void(void)> const tests[] = {
        Server_parse_request_test,
    };
    size_t counter = 0;

    for (auto const& test : tests)
    {
        test();
        counter++;
    }

    std::cout << "server tests: " << counter << " success." << std::endl;
}