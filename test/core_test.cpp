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
                MCHAIN(Conn::assign, "test2", 21)
                pipe bind(Conn::assign, "test_assign", 42)
                pipe [](Conn const& c) { return Conn::put_session(c, "test_header", "42"); }
                pipe assert_conn;
        }

        WHEN("Using string pipelines") {
            std::string arg = "42";
            auto result = std::move(arg)
                MCHAIN(std::operator+, " is the answer")
                MCHAIN(std::operator+, " to everything!");

            THEN("String operations are properly chained") {
                REQUIRE_THAT(result, Equals("42 is the answer to everything!"));
                REQUIRE_THAT(arg, Equals("")); // Moved from
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