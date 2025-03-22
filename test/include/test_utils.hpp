#pragma once

#include <feather/core.hpp>
#include <feather/router.hpp>

namespace test {
    inline feather::core::plug::Conn buildFirstConn() {
        using namespace feather::core::plug;

        http::Request req;
        req.path = "/users/123";
        req.method = "get";
        req.target = "/users/123?test=tested&patate=douce";
        req.params.emplace("format", "json");
        req.headers.emplace("Authorization", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        req.headers.emplace("Accept", "en-US,en;q=0.5");
        req.headers.emplace("Accept-Language", "en-US,en;q=0.5");
        req.headers.emplace("cookie", "session=abc123;");
        req.headers.emplace("cookie", "user_id=42; preferences=\"theme:dark,font:large\"; Path=/; Domain=example.com; Secure; HttpOnly");
        auto session = std::make_shared<CookieSession>(CookieSession());

        return Conn(std::move(req), session);
    }

    inline feather::core::plug::Conn buildFirstErrorConn() {
        using namespace feather::core::plug;

        http::Request req;
        req.path = "/non-existent";
        req.method = "get";
        req.target = "/non-existent";
        req.params.emplace("format", "json");
        req.headers.emplace("Authorization", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        req.headers.emplace("Accept", "en-US,en;q=0.5");
        req.headers.emplace("Accept-Language", "en-US,en;q=0.5");
        req.headers.emplace("cookie", "session=abc123;");
        req.headers.emplace("cookie", "user_id=42; preferences=\"theme:dark,font:large\"; Path=/; Domain=example.com; Secure; HttpOnly");
        auto session = std::make_shared<CookieSession>(CookieSession());

        return Conn(std::move(req), session);
    }
} 