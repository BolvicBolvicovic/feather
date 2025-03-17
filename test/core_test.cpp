#include <core_test.hpp>
#include <iostream>

static core::plug::Conn const  buildFirstConn(void)
{
    using namespace core::plug;

    http::Request req;
    req.path = "/users/123";
    req.target = "/users/123?test=tested&patate=douce";
    req.params.emplace("format", "json");
    req.headers.emplace("Authorization", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    req.headers.emplace("Accept", "en-US,en;q=0.5");
    req.headers.emplace("Accept-Language", "en-US,en;q=0.5");
    req.headers.emplace("cookie", "session=abc123;");
    req.headers.emplace("cookie", "user_id=42; preferences=\"theme:dark,font:large\"; Path=/; Domain=example.com; Secure; HttpOnly");
    auto session = std::make_shared<CookieSession>(CookieSession());

    Conn const first_conn(std::move(req), session);
    
    return first_conn;
}

static void    BuildPathInfo_test(void)
{
    using namespace core::plug;

    std::string const           raw_path = "/app/test/random";
    core::ImmutVecString const  path = BuildPathInfo(raw_path);
    for (auto const& info : path)
    {
        assert(info->find("/") == std::string::npos);
    }
}

static void    GetPortFromHost_test(void)
{
    using namespace core::plug;

    std::vector<std::tuple<std::string const, int const>> const hosts =
    {
        {"example.com:8080", 8080},
        {"localhost:3000", 3000}, 
        {"api.example.com", 443}, 
        {"[::1]:9000", 9000},
        {"192.168.1.1:5432", 5432},
    };
    for (auto const& host : hosts)
    {
        assert(GetPortFromHost(std::get<0>(host)).value_or(-1) == std::get<1>(host));
    }
}

static void    GetQueryFromTarget_test(void)
{
    using namespace core::plug;

    std::vector<std::tuple<std::string const, std::string const>> const queries =
    {
        {"https://cplusplus.com/reference/string/string/substr/", ""},
        {"test.com?test=quest", "test=quest"},
        {"test.com?test=quest#dest", "test=quest"},
        {"", ""},
    };
    for (auto const& query : queries)
    {
        assert(*GetQueryFromTarget(std::get<0>(query)) == std::get<1>(query));
    }
}

static void Conn_assign_test(void)
{
    using namespace core::plug;

    Conn const first_conn = buildFirstConn();
    assert(first_conn.assigns.empty());

    Conn const updated_conn = Conn::assign(first_conn, "test", 5);
    assert(first_conn.assigns.empty());
    assert(!updated_conn.assigns.empty());
    assert(std::any_cast<int const>(updated_conn.assigns["test"]) == 5);
}

static void Conn_req_header_test(void)
{
    using namespace core::plug;

    Conn const first_conn = buildFirstConn();
    std::string const key = "Accept-Language";

    auto range = Conn::get_req_header(first_conn, key);
    assert(range.first != range.second);
    for (auto it = range.first; it != range.second; ++it)
    {
        assert(it->second == "en-US,en;q=0.5");
    }

    auto sec_conn = 
        Conn::delete_req_header(first_conn, key).second
        CHAIN( Conn::put_req_header, "test", "tester" )
        CHAIN( core::unwrap<Conn> );

    range = Conn::get_req_header(sec_conn, key);
    assert(range.first == range.second);

    range = Conn::get_req_header(sec_conn, "test");
    assert(range.first != range.second);
    for (auto it = range.first; it != range.second; ++it)
    {
        assert(it->second == "tester");
    }

}

static void Conn_fetch_test(void)
{
    using namespace core::plug;

    Conn const conn =
        buildFirstConn()
        CHAIN( Conn::fetch_cookies )
        CHAIN( Conn::fetch_query_params );
    
    assert(conn.cookies.value().find("session") != nullptr);
    assert(conn.cookies.value().find("user_id") != nullptr);
    assert(conn.cookies.value().find("Path") == nullptr);

    assert(!conn.status.has_value());
    assert(conn.query_params.value().find("test") != nullptr);
    assert(conn.query_params.value().find("patate") != nullptr);
    assert(**conn.query_params.value().find("test") == "tested");
    assert(**conn.query_params.value().find("patate") == "douce");
}

static void Conn_put_resp_cookie_test(void)
{
    using namespace core::plug;

    std::string key = "test";
    Conn const first_conn = buildFirstConn();
    assert(first_conn.resp_cookies.find(key) == nullptr);

    core::Result<Conn const> res = Conn::put_resp_cookie(first_conn, key, "test");
    assert(res.first == core::ResultType::Ok);
    assert(res.second.resp_cookies.find(key) != nullptr);
    assert(*res.second.resp_cookies[key]["value"] == "test_cookie=test");
}

static void Conn_delete_resp_cookie_test(void)
{
    using namespace core::plug;

    std::string key = "test";
    Conn const first_conn = buildFirstConn();

    Conn const second_conn = Conn::put_resp_cookie(first_conn, key, "test").second;
    assert(second_conn.resp_cookies.find(key) != nullptr);
    assert(*second_conn.resp_cookies[key]["value"] == "test_cookie=test");
    Conn const third_conn = Conn::delete_resp_cookie(second_conn, key);
    assert(third_conn.resp_cookies.find(key) != nullptr);
    assert(third_conn.resp_cookies[key].find("value") == nullptr);
    assert(*third_conn.resp_cookies[key]["max_age"] == "0");
}

static void Conn_session_test(void)
{
    using namespace core::plug;
    Conn const first_conn = buildFirstConn();

    Conn const second_conn = Conn::put_session(first_conn, "test", 42);
    assert(!Conn::get_session(first_conn, "test").has_value());
    assert(std::any_cast<int>(Conn::get_session(second_conn, "test")) == 42);
    
    Conn const third_conn = Conn::delete_session(second_conn, "test");
    assert(std::any_cast<int>(Conn::get_session(second_conn, "test")) == 42);
    assert(!Conn::get_session(third_conn, "test").has_value());

    Conn const fourth_conn = Conn::clear_session(second_conn);
    assert(std::any_cast<int>(Conn::get_session(second_conn, "test")) == 42);
    assert(!Conn::get_session(fourth_conn, "test").has_value());
}

static void Conn_resp_header_test(void)
{
    using namespace core::plug;

    std::string key = "test";
    Conn const first_conn = buildFirstConn();
    assert(Conn::put_resp_header(first_conn, key, "\n").first == core::ResultType::Err);
    assert(Conn::put_resp_header(first_conn, key, "\r").first == core::ResultType::Err);

    Conn const second_conn = Conn::put_resp_header(first_conn, key, "test").second;
    auto range = Conn::get_resp_header(second_conn, "test");

    assert(second_conn.resp_headers.find(key) != second_conn.resp_headers.end());
    for (auto it = range.first; it != range.second; ++it)
    {
        assert(it->second == "test");
    }

    Conn const third_conn = Conn::delete_resp_header(second_conn, key).second;
    range = Conn::get_resp_header(third_conn, "test");
    assert(range.first == range.second);
}

static void pipe_test(void)
{
    using namespace core:: plug;

    auto assert_conn = [](Conn const& c)
    {
        assert(std::string(std::any_cast<char const*>(Conn::get_session(c, "test_header"))) == "42");
        assert(std::any_cast<int>(c.assigns["test_assign"]) == 42);
    };

    buildFirstConn()
        MCHAIN(Conn::assign, "test2", 21)
        pipe bind(Conn::assign, "test_assign", 42)
        pipe [](Conn const& c) { return Conn::put_session(c, "test_header", "42"); }
        pipe assert_conn;

    auto assert_move = [](std::string const& s)
    {
        assert(s == "42 is the answer to everything!");
        return s;
    };

    std::string arg = "42";
    std::move(arg)
        MCHAIN( std::operator+, " is the answer"  )
        MCHAIN( std::operator+, " to everything!" )
        MCHAIN( assert_move );
    
    assert(arg != "42");
}

static void ParseCookie_test(void)
{
    using namespace core::plug;

    auto parsed_cookie = ParseCookie("session=abc123; user_id=42; preferences=\"theme:dark,font:large\"; Path=/; Domain=example.com; Secure; HttpOnly");
    assert(parsed_cookie["session"] != nullptr);
    assert(parsed_cookie["user_id"] != nullptr);
    assert(parsed_cookie["preferences"] != nullptr);
    assert(*parsed_cookie["session"] == "abc123");
    assert(*parsed_cookie["user_id"] == "42");
    assert(*parsed_cookie["preferences"] == "\"theme:dark,font:large\"");
    assert(parsed_cookie["Path"] == nullptr);
    assert(parsed_cookie["Domain"] == nullptr);
    assert(parsed_cookie["Secure"] == nullptr);
    assert(parsed_cookie["HttpOnly"] == nullptr);
}

static void run_core_plug_test(void)
{
    size_t test_count = 0;
    std::function<void(void)> const plug_tests[] = 
    {
        ParseCookie_test,
        BuildPathInfo_test,
        GetPortFromHost_test,
        GetQueryFromTarget_test,
        Conn_assign_test,
        Conn_req_header_test,
        Conn_fetch_test,
        Conn_put_resp_cookie_test,
        Conn_delete_resp_cookie_test,
        Conn_session_test,
        Conn_resp_header_test,
    };
    for (auto const& test : plug_tests)
    {
        test_count++;
        test();
    }
    std::cout << "core::plug tests: " << test_count << " success." << std::endl;
}

void run_core_test(void)
{
    std::function<void(void)> const core_tests[] =
    {
        pipe_test,
        run_core_plug_test,
    };
    for (auto const& test: core_tests)
    {
        test();
    }
    std::cout << "All core tests pass!" << std::endl;
}