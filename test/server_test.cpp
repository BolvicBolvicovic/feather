/*--- Code file for test server ---*/

#include "test_pch.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace feather::core;
using namespace plug;
using namespace test;
using namespace Catch::Matchers;

SCENARIO("Server Request Parsing", "[server]") {
    GIVEN("A raw HTTP request") {
        std::string raw_request = 
            "GET /test?param=value HTTP/1.1\r\n"
            "Host: localhost:4000\r\n"
            "Accept-Language: en-US,en;q=0.5\r\n"
            "Cookie: session=abc123; user_id=42\r\n"
            "\r\n"
            "Hello World";

        WHEN("Parsing the request") {
            auto result = Server::parse_request(raw_request);

            THEN("Request is parsed correctly") {
                REQUIRE(result.first == ResultType::Ok);
                REQUIRE_THAT(result.second.method, Equals("GET"));
                REQUIRE_THAT(result.second.path, Equals("/test"));
                REQUIRE_THAT(result.second.target, Equals("/test?param=value"));
                REQUIRE_THAT(result.second.version, Equals("HTTP/1.1"));
                REQUIRE_THAT(result.second.body, Equals("Hello World"));
            }

            AND_THEN("Headers are properly parsed") {
                REQUIRE_THAT(result.second.headers.find("Host")->second, Equals("localhost:4000"));
                REQUIRE_THAT(result.second.headers.find("Accept-Language")->second, Equals("en-US,en;q=0.5"));
            }
        }

        WHEN("Parsing an invalid request") {
            std::string invalid_request = "INVALID /test HTTP/1.1\r\n";
            auto result = Server::parse_request(invalid_request);

            THEN("Error is returned") {
                REQUIRE(result.first == ResultType::Err);
            }
        }
    }
}

SCENARIO("Server WebSocket Handling", "[server]") {
    GIVEN("A server instance") {
        Server server;

        WHEN("Handling WebSocket connections") {
            auto conn = buildFirstConn();
            conn.state = Unsent::UPGRADED;

            THEN("Connection state is properly set") {
                REQUIRE(std::holds_alternative<Unsent>(conn.state));
                REQUIRE(std::get<Unsent>(conn.state) == Unsent::UPGRADED);
            }
        }
    }
}

SCENARIO("Server Cookie Management", "[server]") {
    GIVEN("A server instance and a connection") {
        Server server;
        auto conn = buildFirstConn();

        WHEN("Processing cookies") {
            conn = Conn::fetch_cookies(conn);

            THEN("Cookies are properly parsed") {
                REQUIRE(conn.req_cookies.has_value());
                auto cookies = conn.req_cookies.value();
                REQUIRE(cookies.find("session") != nullptr);
                REQUIRE_THAT(**cookies.find("session"), Equals("abc123"));
                REQUIRE(cookies.find("user_id") != nullptr);
                REQUIRE_THAT(**cookies.find("user_id"), Equals("42"));
            }
        }

        WHEN("Setting response cookies") {
            conn = Conn::put_resp_cookie(conn, "test", "value")
                pipe unwrap<Conn>;

            THEN("Response cookies are properly set") {
                REQUIRE(conn.resp_cookies.find("test") != nullptr);
                REQUIRE_THAT(*conn.resp_cookies["test"]["value"], ContainsSubstring("value"));
            }
        }
    }
}