#include "test_pch.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

using namespace feather::core::plug;
using namespace feather;
using namespace test;
using namespace Catch::Matchers;

SCENARIO("Core Path Info Operations", "[core]") {
    GIVEN("A path with multiple segments") {
        std::string const raw_path = "/app/test/random";

        WHEN("BuildPathInfo is called") {
            core::ImmutVecString const path = BuildPathInfo(raw_path);

            THEN("Each segment should not contain slashes") {
                for (auto const& info : path) {
                    REQUIRE_THAT(*info, !ContainsSubstring("/"));
                }
            }
        }
    }
}

SCENARIO("Host Port Resolution", "[core]") {
    GIVEN("Various host formats") {
        const std::vector<std::pair<std::string, int>> test_cases = {
            {"example.com:8080", 8080},
            {"localhost:3000", 3000},
            {"api.example.com", 443},  // Default HTTPS port
            {"[::1]:9000", 9000},      // IPv6
            {"192.168.1.1:5432", 5432} // IPv4
        };

        WHEN("GetPortFromHost is called") {
            THEN("Each host format should resolve to the correct port") {
                for (const auto& [host, expected_port] : test_cases) {
                    INFO("Testing host: " << host);
                    REQUIRE(GetPortFromHost(host).value_or(-1) == expected_port);
                }
            }
        }
    }
}

SCENARIO("URL Query Extraction", "[core]") {
    GIVEN("URLs with different query formats") {
        const std::vector<std::pair<std::string, std::string>> test_cases = {
            {"https://cplusplus.com/reference/string/string/substr/", ""},
            {"test.com?test=quest", "test=quest"},
            {"test.com?test=quest#dest", "test=quest"},
            {"", ""}
        };

        WHEN("GetQueryFromTarget is called") {
            THEN("Query strings should be correctly extracted") {
                for (const auto& [url, expected_query] : test_cases) {
                    INFO("Testing URL: " << url);
                    REQUIRE(*GetQueryFromTarget(url) == expected_query);
                }
            }
        }
    }
}

SCENARIO("Connection State Management", "[core]") {
    GIVEN("A fresh connection") {
        Conn const initial_conn = buildFirstConn();

        WHEN("Assigning values") {
            Conn const updated_conn = Conn::assign(initial_conn, "test", 5);

            THEN("Original connection remains unchanged") {
                REQUIRE(initial_conn.assigns.empty());
            }

            AND_THEN("Updated connection contains the new value") {
                REQUIRE_FALSE(updated_conn.assigns.empty());
                REQUIRE(std::any_cast<int>(updated_conn.assigns.at("test")) == 5);
            }
        }

        WHEN("Managing request headers") {
            std::string const key = "Accept-Language";
            auto range = Conn::get_req_header(initial_conn, key);

            THEN("Original headers are accessible") {
                REQUIRE(range.first != range.second);
                REQUIRE_THAT(range.first->second, Equals("en-US,en;q=0.5"));
            }

            AND_WHEN("Modifying headers") {
                auto modified_conn = 
                    Conn::delete_req_header(initial_conn, key).second
                    CHAIN(Conn::put_req_header, "test", "tester")
                    CHAIN(core::unwrap<Conn>);

                THEN("Headers are properly updated") {
                    auto deleted_range = Conn::get_req_header(modified_conn, key);
                    REQUIRE(deleted_range.first == deleted_range.second);

                    auto new_range = Conn::get_req_header(modified_conn, "test");
                    REQUIRE(new_range.first != new_range.second);
                    REQUIRE_THAT(new_range.first->second, Equals("tester"));
                }
            }
        }
    }
}

SCENARIO("Cookie and Query Parameter Handling", "[core]") {
    GIVEN("A connection with cookies and query parameters") {
        Conn const conn = buildFirstConn()
            CHAIN(Conn::fetch_cookies)
            CHAIN(Conn::fetch_query_params);

        WHEN("Accessing cookies") {
            THEN("Standard cookies are accessible") {
                REQUIRE(conn.cookies.value().find("session") != nullptr);
                REQUIRE(conn.cookies.value().find("user_id") != nullptr);
            }

            AND_THEN("Special cookie attributes are not treated as cookies") {
                REQUIRE(conn.cookies.value().find("Path") == nullptr);
            }
        }

        WHEN("Accessing query parameters") {
            THEN("Parameters are properly parsed") {
                REQUIRE(conn.query_params.value().find("test") != nullptr);
                REQUIRE(conn.query_params.value().find("patate") != nullptr);
                REQUIRE_THAT(**conn.query_params.value().find("test"), Equals("tested"));
                REQUIRE_THAT(**conn.query_params.value().find("patate"), Equals("douce"));
            }
        }
    }
}

SCENARIO("Response Cookie Management", "[core]") {
    GIVEN("A connection") {
        std::string const key = "test";
        Conn const initial_conn = buildFirstConn();

        WHEN("Setting a response cookie") {
            core::Result<Conn const> res = Conn::put_resp_cookie(initial_conn, key, "test");

            THEN("Cookie is properly set") {
                REQUIRE(res.first == core::ResultType::Ok);
                REQUIRE(res.second.resp_cookies.find(key) != nullptr);
                REQUIRE_THAT(*res.second.resp_cookies[key]["value"], Equals("test_cookie=test"));
            }

            AND_WHEN("Deleting the cookie") {
                Conn const deleted_cookie_conn = Conn::delete_resp_cookie(res.second, key);

                THEN("Cookie is marked as expired") {
                    REQUIRE(deleted_cookie_conn.resp_cookies.find(key) != nullptr);
                    REQUIRE(deleted_cookie_conn.resp_cookies[key].find("value") == nullptr);
                    REQUIRE_THAT(*deleted_cookie_conn.resp_cookies[key]["max_age"], Equals("0"));
                }
            }
        }
    }
}

SCENARIO("Session Management", "[core]") {
    GIVEN("A connection with session management") {
        Conn const initial_conn = buildFirstConn();

        WHEN("Setting a session value") {
            Conn const session_conn = Conn::put_session(initial_conn, "test", 42);

            THEN("Session value is properly set") {
                REQUIRE_FALSE(Conn::get_session(initial_conn, "test").has_value());
                REQUIRE(std::any_cast<int>(Conn::get_session(session_conn, "test")) == 42);

                AND_WHEN("Deleting the session value") {
                    Conn const deleted_session_conn = Conn::delete_session(session_conn, "test");

                    THEN("Value is removed") {
                        REQUIRE_FALSE(Conn::get_session(deleted_session_conn, "test").has_value());
                    }
                }

                AND_WHEN("Clearing the entire session") {
                    Conn const cleared_session_conn = Conn::clear_session(session_conn);

                    THEN("All values are removed") {
                        REQUIRE_FALSE(Conn::get_session(cleared_session_conn, "test").has_value());
                    }
                }
            }
        }
    }
}

SCENARIO("Response Header Management", "[core]") {
    GIVEN("A connection for header manipulation") {
        std::string const key = "test";
        Conn const initial_conn = buildFirstConn();

        WHEN("Setting invalid headers") {
            THEN("Newline characters are rejected") {
                REQUIRE(Conn::put_resp_header(initial_conn, key, "\n").first == core::ResultType::Err);
                REQUIRE(Conn::put_resp_header(initial_conn, key, "\r").first == core::ResultType::Err);
            }
        }

        WHEN("Setting valid headers") {
            Conn const header_conn = Conn::put_resp_header(initial_conn, key, "test").second;

            THEN("Headers are properly set") {
                auto range = Conn::get_resp_header(header_conn, key);
                REQUIRE(header_conn.resp_headers.find(key) != header_conn.resp_headers.end());
                REQUIRE_THAT(range.first->second, Equals("test"));

                AND_WHEN("Deleting headers") {
                    Conn const deleted_header_conn = Conn::delete_resp_header(header_conn, key).second;

                    THEN("Headers are removed") {
                        auto deleted_range = Conn::get_resp_header(deleted_header_conn, key);
                        REQUIRE(deleted_range.first == deleted_range.second);
                    }
                }
            }
        }
    }
}

SCENARIO("Pipeline Operations", "[core]") {
    GIVEN("A connection for pipeline testing") {
        WHEN("Using connection pipelines") {
            auto assert_conn = [](Conn const& c) {
                REQUIRE_THAT(std::string(std::any_cast<char const*>(Conn::get_session(c, "test_header"))), Equals("42"));
                REQUIRE(std::any_cast<int>(c.assigns["test_assign"]) == 42);
                return c;
            };

            buildFirstConn()
                CHAIN(Conn::assign, "test2", 21)
                CHAIN(bind, Conn::assign, "test_assign", 42)
                CHAIN(bind, [](Conn const& c) { return Conn::put_session(c, "test_header", "42"); })
                CHAIN(bind, assert_conn);
        }

        WHEN("Using string pipelines") {
            std::string arg = "42";
            auto result =
                arg
                CHAIN(std::operator+, " is the answer")
                CHAIN(std::operator+, " to everything!");

            THEN("String operations are properly chained") {
                REQUIRE_THAT(result, Equals("42 is the answer to everything!"));
                REQUIRE_THAT(arg, Equals("42"));
            }
        }
    }
}

SCENARIO("Cookie Parsing", "[core]") {
    GIVEN("A complex cookie string") {
        std::string const cookie_str = "session=abc123; user_id=42; preferences=\"theme:dark,font:large\"; Path=/; Domain=example.com; Secure; HttpOnly";

        WHEN("Parsing the cookie string") {
            auto parsed_cookie = ParseCookie(cookie_str);

            THEN("Cookie values are correctly parsed") {
                REQUIRE(parsed_cookie["session"] != nullptr);
                REQUIRE(parsed_cookie["user_id"] != nullptr);
                REQUIRE(parsed_cookie["preferences"] != nullptr);
                REQUIRE_THAT(*parsed_cookie["session"], Equals("abc123"));
                REQUIRE_THAT(*parsed_cookie["user_id"], Equals("42"));
                REQUIRE_THAT(*parsed_cookie["preferences"], Equals("\"theme:dark,font:large\""));
            }

            AND_THEN("Cookie attributes are not treated as values") {
                REQUIRE(parsed_cookie["Path"] == nullptr);
                REQUIRE(parsed_cookie["Domain"] == nullptr);
            }
        }
    }
}

SCENARIO("Connection State Transitions", "[core]") {
    GIVEN("A fresh connection") {
        Conn const initial_conn = buildFirstConn();

        WHEN("Setting response status") {
            auto result = Conn::put_status(initial_conn, 200);

            THEN("Status is properly set") {
                REQUIRE(result.first == core::ResultType::Ok);
                REQUIRE(result.second.status.value() == 200);
            }

            AND_WHEN("Attempting to set status after sending") {
                Conn sent_conn = result.second;
                sent_conn.state = Sent{};
                auto error_result = Conn::put_status(sent_conn, 404);

                THEN("Error is returned") {
                    REQUIRE(error_result.first == core::ResultType::Err);
                }
            }
        }

        WHEN("Upgrading connection") {
            Conn const upgraded_conn = Conn::upgrade_conn(initial_conn, "websocket");

            THEN("Connection is properly upgraded") {
                REQUIRE(std::holds_alternative<Unsent>(upgraded_conn.state));
                REQUIRE(std::get<Unsent>(upgraded_conn.state) == Unsent::UPGRADED);
                REQUIRE(upgraded_conn.status.value() == 426);
                REQUIRE(upgraded_conn.resp_headers.find("Upgrade")->second == "websocket");
                REQUIRE(upgraded_conn.resp_headers.find("Connection")->second == "Upgrade");
            }
        }
    }
}

SCENARIO("Response Content Type Management", "[core]") {
    GIVEN("A fresh connection") {
        Conn const initial_conn = buildFirstConn();

        WHEN("Setting content type with charset") {
            Conn const content_conn = Conn::put_resp_content_type(initial_conn, "application/json", "utf-8");

            THEN("Content type is properly set") {
                auto range = Conn::get_resp_header(content_conn, "content-type");
                REQUIRE(range.first != range.second);
                REQUIRE_THAT(range.first->second, Equals("application/json; charset=utf-8"));
            }
        }

        WHEN("Setting content type without charset") {
            Conn const content_conn = Conn::put_resp_content_type(initial_conn, "image/png", "none");

            THEN("Content type is set without charset") {
                auto range = Conn::get_resp_header(content_conn, "content-type");
                REQUIRE(range.first != range.second);
                REQUIRE_THAT(range.first->second, Equals("image/png"));
            }
        }
    }
}

SCENARIO("Response Body Management", "[core]") {
    GIVEN("A fresh connection") {
        Conn const initial_conn = buildFirstConn();

        WHEN("Setting response body") {
            Conn const resp_conn = Conn::resp(initial_conn, 200, "Hello World");

            THEN("Response is properly set") {
                REQUIRE(resp_conn.status.value() == 200);
                REQUIRE_THAT(*resp_conn.resp_body, Equals("Hello World"));
                REQUIRE(std::holds_alternative<Unsent>(resp_conn.state));
                REQUIRE(std::get<Unsent>(resp_conn.state) == Unsent::SET);
            }

            AND_WHEN("Attempting to set response after sending") {
                Conn sent_conn = resp_conn;
                sent_conn.state = Sent{};
                REQUIRE_THROWS_AS(Conn::resp(sent_conn, 404, "Error"), std::logic_error);
            }
        }
    }
}

SCENARIO("Before Send Callbacks", "[core]") {
    GIVEN("A fresh connection") {
        Conn const initial_conn = buildFirstConn();

        WHEN("Registering before send callbacks") {
            bool callback_called = false;
            auto callback = [&callback_called](Conn const& c) {
                callback_called = true;
                return c;
            };

            Conn const callback_conn = Conn::register_before_send(initial_conn, callback);

            THEN("Callback is registered") {
                REQUIRE(callback_conn.callbacks_before_send.size() == 1);
            }

            AND_WHEN("Attempting to register callback after sending") {
                Conn sent_conn = callback_conn;
                sent_conn.state = Sent{};
                REQUIRE_THROWS_AS(Conn::register_before_send(sent_conn, callback), std::logic_error);
            }
        }
    }
}

SCENARIO("Connection Halt", "[core]") {
    GIVEN("A fresh connection") {
        Conn const initial_conn = buildFirstConn();

        WHEN("Halting the connection") {
            Conn const halted_conn = Conn::halt(initial_conn);

            THEN("Connection is marked as halted") {
                REQUIRE(halted_conn.halted);
            }
        }
    }
}

SCENARIO("Query Parameter Validation", "[core]") {
    GIVEN("A connection with query parameters") {
        Conn const initial_conn = buildFirstConn();

        WHEN("Fetching query parameters with length limit") {
            auto opts = core::ImmutMapString().transient();
            opts.set("length", core::ShareStr("10"));
            Conn const param_conn = Conn::fetch_query_params(initial_conn, opts.persistent());

            THEN("Parameters are validated against length limit") {
                REQUIRE(param_conn.status.value() == 414);
            }
        }

        WHEN("Fetching query parameters with UTF-8 validation") {
            auto opts = core::ImmutMapString().transient();
            opts.set("validate_utf8", core::ShareStr("true"));
            Conn const param_conn = Conn::fetch_query_params(initial_conn, opts.persistent());

            THEN("Parameters are validated for UTF-8") {
                REQUIRE(param_conn.query_params.has_value());
            }
        }
    }
}

SCENARIO("Cookie Security Options", "[core]") {
    GIVEN("A connection with cookies") {
        Conn const initial_conn = buildFirstConn();

        WHEN("Setting signed cookies") {
            auto opts = core::ImmutSetString().transient();
            opts.insert("sign");
            Conn const cookie_conn = Conn::fetch_cookies(initial_conn, opts.persistent());

            THEN("Cookies are processed with signing") {
                REQUIRE(cookie_conn.cookies.has_value());
            }
        }

        WHEN("Setting encrypted cookies") {
            auto opts = core::ImmutSetString().transient();
            opts.insert("encrypt");
            Conn const cookie_conn = Conn::fetch_cookies(initial_conn, opts.persistent());

            THEN("Cookies are processed with encryption") {
                REQUIRE(cookie_conn.cookies.has_value());
            }
        }

        WHEN("Setting both signed and encrypted cookies") {
            auto opts = core::ImmutMapString().transient();
            opts.set("sign", core::ShareStr("true"));
            opts.set("encrypt", core::ShareStr("true"));
            auto result = Conn::put_resp_cookie(initial_conn, "test", "value", opts.persistent());

            THEN("Error is returned") {
                REQUIRE(result.first == core::ResultType::Err);
            }
        }
    }
}

SCENARIO("Response Header Management with Multiple Headers", "[core]") {
    GIVEN("A fresh connection") {
        Conn const initial_conn = buildFirstConn();

        WHEN("Merging multiple response headers") {
            http::Headers headers = {
                {"content-type", "application/json"},
                {"cache-control", "no-cache"},
                {"x-custom", "value"}
            };
            auto result = Conn::merge_resp_headers(initial_conn, std::move(headers));

            THEN("All headers are properly merged") {
                REQUIRE(result.first == core::ResultType::Ok);
                auto const& resp_headers = result.second.resp_headers;
                REQUIRE(resp_headers.find("content-type")->second == "application/json");
                REQUIRE(resp_headers.find("cache-control")->second == "no-cache");
                REQUIRE(resp_headers.find("x-custom")->second == "value");
            }
        }

        WHEN("Prepending multiple response headers") {
            http::Headers headers = {
                {"content-type", "application/json"},
                {"cache-control", "no-cache"}
            };
            auto result = Conn::prepend_resp_header(initial_conn, headers);

            THEN("Headers are properly prepended") {
                REQUIRE(result.first == core::ResultType::Ok);
                auto const& resp_headers = result.second.resp_headers;
                REQUIRE(resp_headers.find("content-type")->second == "application/json");
                REQUIRE(resp_headers.find("cache-control")->second == "no-cache");
            }
        }
    }
}

SCENARIO("Request Header Management with Multiple Headers", "[core]") {
    GIVEN("A fresh connection") {
        Conn const initial_conn = buildFirstConn();

        WHEN("Merging multiple request headers") {
            http::Headers headers = {
                {"x-custom", "value"},
                {"x-api-key", "secret"}
            };
            auto result = Conn::merge_req_headers(initial_conn, std::move(headers));

            THEN("All headers are properly merged") {
                REQUIRE(result.first == core::ResultType::Ok);
                auto const& req_headers = result.second.req_headers;
                REQUIRE(req_headers.find("x-custom")->second == "value");
                REQUIRE(req_headers.find("x-api-key")->second == "secret");
            }
        }

        WHEN("Prepending multiple request headers") {
            http::Headers headers = {
                {"x-custom", "value"},
                {"x-api-key", "secret"}
            };
            auto result = Conn::prepend_req_headers(initial_conn, std::move(headers));

            THEN("Headers are properly prepended") {
                REQUIRE(result.first == core::ResultType::Ok);
                auto const& req_headers = result.second.req_headers;
                REQUIRE(req_headers.find("x-custom")->second == "value");
                REQUIRE(req_headers.find("x-api-key")->second == "secret");
            }
        }

        WHEN("Updating host header") {
            auto result = Conn::put_req_header(initial_conn, "host", "newhost.com");

            THEN("Host is properly updated") {
                REQUIRE(result.first == core::ResultType::Ok);
                REQUIRE_THAT(*result.second.host, Equals("newhost.com"));
            }
        }
    }
}