/*--- Code file for test router ---*/

#include "test_pch.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace feather::router;
using namespace feather::core::plug;
using namespace test;
using namespace Catch::Matchers;

namespace {
    // Helper to create a basic router configuration
    Router setupBasicRouter() {
        return Router()
            CHAIN(Router::pipeline, "test1",
                (CALLBACK_PLINE {
                    PLUG(Conn::fetch_cookies);
                    PLUG(Conn::fetch_query_params);
                    END_PLINE;
                }))
            CHAIN(Router::pipeline, "test2",
                (CALLBACK_PLINE {
                    PLUG(Conn::assign, "key_test", 42);
                    END_PLINE;
                }));
    }
}

SCENARIO("Router Pipeline Configuration", "[router]") {
    GIVEN("A router with multiple pipelines") {
        WHEN("Configuring pipelines") {
            router = setupBasicRouter();
            
            THEN("Pipelines are properly configured") {
                REQUIRE(router.pipelines.find("test1") != nullptr);
                REQUIRE(router.pipelines.find("test2") != nullptr);
            }
        }

        WHEN("Configuring scopes with pipelines") {
            router = setupBasicRouter()
                CHAIN(Router::scope, "/",
                    (CALLBACK_SCOPE {
                        PIPE_THROUGH("test1", "test2");
                        GET("/posts/new", nullptr);
                        POST("/posts", nullptr);
                        END_SCOPE;
                    }))
                CHAIN(Router::scope, "/api",
                    (CALLBACK_SCOPE {
                        PIPE_THROUGH("test1");
                        GET("/posts", nullptr);
                        GET("/users/123", nullptr);
                        END_SCOPE;
                    }));

            THEN("Router scopes are properly configured") {
                REQUIRE(router.scopes["/"][0].pipeline != nullptr);
                REQUIRE(router.scopes["/api"][0].pipeline != nullptr);
                REQUIRE(router.scopes["/"][0].get.find("/posts/new") != nullptr);
                REQUIRE(router.scopes["/api"][0].get.find("/posts") != nullptr);
            }
        }
    }
}

SCENARIO("Router Request Handling", "[router]") {
    GIVEN("A configured router and a connection") {
        router = setupBasicRouter()
            CHAIN(Router::scope, "/",
                (CALLBACK_SCOPE {
                    PIPE_THROUGH("test1");
                    GET("/users/123", nullptr);
                    END_SCOPE;
                }));

        WHEN("Handling a GET request") {
            Conn conn = buildFirstConn() pipe router_handler;

            THEN("Request method is preserved") {
                REQUIRE_THAT(*conn.method, Equals("get"));
            }

            AND_THEN("Pipeline transformations are applied") {
                REQUIRE(conn.cookies.has_value());
                REQUIRE(conn.query_params.has_value());
            }
        }

        WHEN("Handling a request with multiple pipeline transformations") {
            router = setupBasicRouter()
                CHAIN(Router::scope, "/",
                    (CALLBACK_SCOPE {
                        PIPE_THROUGH("test1", "test2");
                        GET("/users/123", nullptr);
                        END_SCOPE;
                    }));
            
            Conn conn = buildFirstConn() pipe router_handler;

            THEN("All pipeline transformations are applied in order") {
                REQUIRE(conn.cookies.has_value());
                REQUIRE(conn.query_params.has_value());
                REQUIRE(conn.assigns.find("key_test") != nullptr);
                REQUIRE(std::any_cast<int>(conn.assigns.at("key_test")) == 42);
            }
        }

        WHEN("Handling a request for non-existent route") {
            Conn conn = buildFirstErrorConn() pipe router_handler;

            THEN("Connection is returned unchanged") {
                REQUIRE_FALSE(conn.cookies.has_value());
                REQUIRE_FALSE(conn.query_params.has_value());
            }
        }
    }
}