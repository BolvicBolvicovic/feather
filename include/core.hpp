#ifndef CORE_HPP
#define CORE_HPP

/*--- httplib for server ---*/
#include <httplib.h>

/*--- Boost includes for process ---*/
#include <boost/process/v2/pid.hpp>

/*--- Boost includes for parsing ---*/
#include <boost/tokenizer.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/locale/encoding_utf.hpp>

/*--- Immer includes for immutable data structures ---*/
#include <immer/vector.hpp>
#include <immer/map.hpp>
#include <immer/map_transient.hpp>
#include <immer/set.hpp>

#include <array>
#include <queue>
#include <string>
#include <functional>
#include <optional>
#include <variant>
#include <utility>
#include <any>

namespace process = boost::process::v2; // From <boost/process/v2/pid.hpp>
namespace http = httplib;

/*--- pipe operator ---*/
/*
    The pipe operator is very important to make function chaining in a clean way.
    It is defined outside of the feather::core namespace for simplicity's sake.

    Usage:

    int var = 1;
    auto result =
        func(var)
    // If only one argument is needed by the function.
            pipe some_func    
    // For function with many arguments.
            pipe [](int const& var) { return some_other_func(var, 5); }
    // If type checking is not important for function with many arguments (expands to the previous type).
            pipe bind(unsafe_type_func, 6);
*/
#define pipe <operatorpipe>
#define bind(func, ...) [&](auto const& _pipe_val) { return func(_pipe_val __VA_OPT__(,) __VA_ARGS__); }
#define Mbind(func, ...) [&](auto&& _pipe_val) { return func(std::move(_pipe_val) __VA_OPT__(,) __VA_ARGS__); }

const struct pipe_ {} operatorpipe;

template <typename T>
struct PipeProxy
{
    std::optional<std::reference_wrapper<T const>> t_;
    T m_;
    PipeProxy(T const& t): t_(std::cref(t)) {}
    PipeProxy(T&& m): t_(std::nullopt), m_(std::move(m)) {}
};

template <typename T>
PipeProxy<T> const operator<(T const& lhs, pipe_ const&)
{
    return PipeProxy<T>(lhs);
}

template <typename T>
PipeProxy<T> operator<(T&& lhs, pipe_ const&)
{
    return PipeProxy<T>(std::move(lhs));
}

template <typename V, typename F>
auto operator>(PipeProxy<V> const& value, F func) -> decltype(func((*value.t_).get()))
{
    return func((*value.t_).get());
}

template <typename V, typename F>
auto operator>(PipeProxy<V>&& value, F&& func) -> decltype(func(std::forward<V>(value.m_)))
{
    return func(std::forward<V>(value.m_));
}

/*--- MCHAIN ---*/
/*
    Syntax sugar for pipe Mbind(func, args).
    arg is moved.
    As the argument and the function are moved, the enclosing function cannot be referenced in a lambda body unless it is captured.
    For this reason MCHAIN and Mbind does not work with closures as the macro does not capture any reference.
    However, if there is only one argument to the closure, then you can use pipe.

    Usage:

    auto closure = [](auto&& arg, int c) { return do_something(std::move(arg), c); };
    auto solo_clo = [](auto&& arg) { return do_something(std::move(arg)); };

    auto res = 
        std::move(arg)
            MCHAIN( first_func )
            MCHAIN( second_func, arg2, arg3 )
            pipe solo_clo
            pipe [&closure](auto&& _arg) { return closure(std::move(_arg), 42); };
*/
#define MCHAIN(...) pipe Mbind(__VA_ARGS__)

/*--- CHAIN ---*/
/*
    Syntax sugar for pipe bind(func, args).
    arg is constant.

    Usage:
    
    auto closure = [](auto const& arg, int c) { return do_something(arg, c); };

    auto res = 
        arg
            CHAIN( first_func )
            CHAIN( second_func, arg2, arg3 )
            CHAIN( closure, 42 );
*/
#define CHAIN(...) pipe bind(__VA_ARGS__)

namespace feather::core
{

namespace functional
{
/*--- merge ---*/
// Merges two immer::map into a single one.
template<typename K, typename V>
inline immer::map<K, V> merge(immer::map<K, V> const& map1, immer::map<K, V> const& map2)
{
    auto result = map1.transient();

    for (auto const& [key, value] : map2)
    {
        result.set(key, value);
    }

    return result.persistent();
}

/*--- reduce ---*/
// Reduces two containers into a single one based on a lambda criteria
template<class Container1, class Container2, class Func>
inline Container2 reduce(
    Container1 const& container,
    Container2 const& acc,
    Func func)
{
    auto result = acc;

    for (auto const& value_t : container)
    {
        result = func(value_t, result);
    }

    return result;
}

}
/*--- SharedString ---*/
// A shortcut for a const string in a shared pointer
using SharedString = std::shared_ptr<std::string const>;

// Share helper
inline SharedString    ShareStr(std::string str)
{
    return std::make_shared<std::string const>(std::move(str));
}

/*--- ImmutVecString ---*/
// A shortcut for a vector of shared pointers containing a string
using ImmutVecString = immer::vector<SharedString>;

/*--- ImmutMapString ---*/
// A shortcut for the most used map
using ImmutMapString = immer::map<std::string, SharedString>;

/*--- Result ---*/
// Similar to Go Result.
// Container for a result that can be correct or that can be an error.
enum struct ResultType
{
    Ok,
    Err,
};

template <typename Value>
using Result = std::pair<ResultType, Value const>;

/*--- unwrap ---*/
/*
    Helper to retrun the value of a result.

    Throw an error if the ResultType is Err.
*/
template <typename Value>
inline Value const    unwrap(Result<Value> const& res)
{
    if (res.first == ResultType::Err)
    {
        throw std::runtime_error("unwrap a result on error");
    }
    return res.second;
}

/*--- to_string ---*/
// Helper to create string from anything needed
inline std::string const    to_string(immer::map<std::string, SharedString> const& map)
{
    std::string str = "";

    for (auto const& value : map)
    {
        str += value.first + "=" + *(value.second) + "; ";
    }
    return str.substr(0, str.length() - 2);
}

namespace plug
{

/*--- BuildPathInfo ---*/
// A parser for path_info in Conn.
inline ImmutVecString BuildPathInfo(std::string const& target)
{
    boost::char_separator<char> const sep("/");
    boost::tokenizer<boost::char_separator<char>> tok(target, sep);
    ImmutVecString vec;

    for (auto beg = tok.begin(); beg != tok.end(); ++beg)
    {
        vec = vec.push_back(std::make_shared<std::string const>(*beg));
    }
    return vec;
}

/*--- GetPortFromHost ---*/
// A host parser to get the port as an optional int
inline std::optional<int const> GetPortFromHost(std::string const& host)
{
    static boost::regex const expr(R"(^([\w\.-]+|\[?[a-fA-F0-9:\.]+\]?)(?::(\d+))?$)", boost::regex::optimize);
    boost::smatch   matches;
    if (boost::regex_match(host, matches, expr))
    {
        if (matches.size() == 3 && !matches[2].str().empty())
        {
            return std::stoi(matches[2]);
        }
        return matches[1].str() == "localhost" ? 80 : 443;
    }
    return std::nullopt;
}

/*--- GetQueryFromTarget ---*/
// A target parser to extract the query
inline SharedString GetQueryFromTarget(std::string const& target)
{
    size_t const      start = target.find('?');
    size_t const        end = target.find('#');
    if (start == std::string::npos)
    {
        return ShareStr("");
    }
    if (end == std::string::npos)
    {
        return ShareStr(target.substr(start + 1));
    }
    return ShareStr(target.substr(start + 1, end - start - 1));
}

/*--- ParseCookie ---*/
// A cookie parser
inline ImmutMapString ParseCookie(std::string const& cookie)
{
    boost::char_separator<char> const sep(";");
    boost::tokenizer<boost::char_separator<char>> tok(cookie, sep);
    ImmutMapString map;

    for (auto it = tok.begin(); it != tok.end(); ++it)
    {
        if (it->empty())
        {
            continue;
        }
        if (auto equal_pos = it->find("="); equal_pos == std::string::npos)
        {
            continue;
        }
        else
        {
            std::string key = 
                it->substr(0, equal_pos)
                CHAIN( boost::trim_copy );
            std::string value =
                it->substr(equal_pos + 1)
                CHAIN( boost::trim_copy );

            if (std::isupper(key[0]))
            {
                continue;
            }
            
            //std::cout << "key: " << key << " value: " << value << std::endl;
            map = map.set(key, ShareStr(value));
        }
    }

    return map;
}

/*--- ErrorType ---*/
// Enum containing all the errors related to the plug namespace

enum struct ErrorType
{
    INCORRECT_CONN_STATE,
};


/*--- Session ---*/
// Interface for any type of session.
// A session is stored in a cookie by default.
// You can implement a session mechanism based on a database
// or redis and configue it in the configuration file.
struct Session
{
    public:
        virtual ~Session() = default;

        virtual std::any const&             get_session(std::string const&) const = 0;
        virtual std::unique_ptr<Session>    put_session(std::string const&, std::any const&) const = 0;
        virtual std::unique_ptr<Session>    delete_session(std::string const&) const = 0;
        virtual std::unique_ptr<Session>    reset_session() const = 0;
        virtual std::unique_ptr<Session>    clone() const = 0;
        virtual std::shared_ptr<Session>    shared_clone() const = 0;
};

/*--- SessionOpt ---*/
// Options for what the connection should do with the current session once it went through the pipeline.
enum SessionOpt
{
    WRITE,
    RENEW,
    DROP,
    IGNORE,
};

/*--- CookieSessions ---*/
// Implementation for the cookie sessions. Default mechanism for handling sessions.
struct CookieSession : public Session
{
    private:
        immer::map<std::string, std::any>     storage;
    public:

        using Session::Session;

        CookieSession()                     = default;
        CookieSession(CookieSession&&)      = default;
        CookieSession(CookieSession const&) = default;
        CookieSession(immer::map<std::string, std::any> const& s) : storage(s) {}
        CookieSession(immer::map<std::string, std::any>&& s) : storage(std::move(s)) {}
        ~CookieSession()                    = default;

        std::unique_ptr<Session> clone() const override
        {
            return std::make_unique<CookieSession>(*this);
        }

        std::shared_ptr<Session> shared_clone() const override
        {
            return std::make_shared<CookieSession>(*this);
        }

        std::any const& get_session(std::string const& key) const override
        {
            static std::any const empty_any;
            if (auto search = storage.find(key); search != nullptr)
            {
                return *search;
            }
            return empty_any;
        }

        std::unique_ptr<Session> put_session(std::string const& key, std::any const& value) const override
        {
            CookieSession updated_sessions(*this);

            updated_sessions.storage = updated_sessions.storage.set(key, value);

            return std::make_unique<CookieSession>(updated_sessions);
        }

        std::unique_ptr<Session> delete_session(std::string const& key) const override
        {
            CookieSession updated_sessions(*this);

            updated_sessions.storage = updated_sessions.storage.erase(key);

            return std::make_unique<CookieSession>(updated_sessions);
        }

        std::unique_ptr<Session> reset_session() const override
        {
            return std::make_unique<CookieSession>();
        }
};

/*--- ConnState ---*/
// Connection status for the Connection structure.
struct Sent {};
enum struct Unsent
{
    UNSET,
    SET,
    SET_CHUNKED,
    SET_FILE,
    FILE,
    CHUNKED,
    SENT,
    UPGRADED,
};

using ConnState = std::variant<Sent, Unsent>;

/*--- HeaderRange---*/
// Helper for the return type of the getters for Headers.
using HeaderRange = std::pair<http::Headers::const_iterator, http::Headers::const_iterator>;

/*--- Connection ---*/
// A Connection represents the equivalent of a Plug.Conn in the Phoenix Framework.
// Templating is necessary to work with sessions. The default case is provided for CookieSessions.
struct Conn
{
private:
    // Should add adapter for socket upgrade such as WebSocket
    // Should add private for private methods. Though I think it is not the best idea.
    std::shared_ptr<Session>    session;
    std::shared_ptr<Session>    session_copy;
    SessionOpt                  session_info;

    /*- private put_session -*/
    // Helper for session related interaction
    static Conn const  put_session(Conn&& conn, std::function<std::unique_ptr<Session>(std::unique_ptr<Session>)> func)
    {
        if (auto ptr = func(conn.session_copy->clone()); ptr == nullptr)
        {
            conn.session_copy = std::move(conn.session_copy->reset_session());
        } else
        {
            conn.session_copy = std::move(ptr);
        }
        conn.session_info = SessionOpt::WRITE;
        return conn;
    }

public:
    // Request fields
    SharedString    host;
    SharedString    method;
    ImmutVecString  path_info;
    ImmutVecString  script_name;
    SharedString    request_path;
    std::optional<int const>        port;
    std::array<int const, 4> const  remote_ip;
    http::Headers                   req_headers;
    SharedString    scheme;
    SharedString    query_string;

    // Fetchable fields
    std::optional<ImmutMapString> cookies;
    std::optional<ImmutMapString> req_cookies;
    std::optional<ImmutMapString> body_params;
    std::optional<ImmutMapString> query_params;
    std::optional<ImmutMapString> path_params;
    std::optional<ImmutMapString> params;

    // Response fields
    SharedString                            resp_body;
    immer::map<std::string, ImmutMapString> resp_cookies;
    http::Headers                           resp_headers;
    std::optional<int>                      status;

    // Connection fields
    immer::map<std::string, std::any>       assigns;
    process::pid_type const                 owner;
    bool                                    halted;
    SharedString                            secret_key_base;
    ConnState                               state;

    /*- Constructors -*/ 
    Conn(http::Request&& req, std::shared_ptr<Session> s)
    :
    session(s),
    session_copy(s->shared_clone()),
    host(ShareStr(req.get_header_value("Host"))),
    method(ShareStr(req.method)),
    path_info(BuildPathInfo(req.path)),
    script_name(/*TODO: extract path from route in application*/),
    request_path(ShareStr(req.path)),
    port(GetPortFromHost(*host)),
    remote_ip({127, 0, 0, 1}/*TODO: default to peer's IP*/),
    req_headers(req.headers),
    scheme(ShareStr(req.version)/*TODO: find a way to choose between ws, wss and https*/),
    query_string(GetQueryFromTarget(req.target)),
    owner(process::current_pid()),
    halted(false),
    secret_key_base(/*TODO: generate key from a crypto lib*/),
    state(Unsent::UNSET)
    {}

    Conn()                       = default;
    Conn(Conn const&)            = default;
    Conn(Conn&&)                 = default;
    Conn& operator=(Conn&&)      = default;
    Conn& operator=(Conn const&) = default;
    ~Conn()                      = default;

    /*- assign -*/
    /*
        Assigns a value to a key in the connection.

        The assigns storage is meant to be used to store values in the connection
        so that other plugs in your plug pipeline can access them.
        The assigns storage is a map.
    */
    static Conn const   assign(Conn const& conn, std::string const& key, std::any const& value)
    {
        Conn    new_conn(conn);
        new_conn.assigns = new_conn.assigns.set(key, value);
        return new_conn;
    }

    /*- merge_assigns -*/
    /*
        Assigns multiple values to keys in the connection.

        More performant than multiple call to assign/3
    */
    static Conn const merge_assigns(Conn const& conn, immer::map<std::string, std::any> const& new_assigns)
    {
        Conn new_conn(conn);
        auto mut_assigns = new_conn.assigns.transient();

        for (auto const& [key, value]: new_assigns)
        {
            mut_assigns.set(key, value);
        }
        new_conn.assigns = mut_assigns.persistent();
        return new_conn;
    }

    /*- chunk -*/
    /*
        Sends a chunk as part of a chunked response.

        It expects a connection with the state Unsent::CHUNKED.
        It returns a std::expected<Conn, ErrorType>
        ErrorType is defined in the namespace core::plug
    */
    static Result<Conn const> const   chunk(Conn const& conn, std::string const& chk)
    {
        if (chk.empty())
        {
            return { ResultType::Ok, conn };
        }

        if (std::holds_alternative<Unsent>(conn.state) 
            && std::get<Unsent>(conn.state) == Unsent::CHUNKED)
        {
            // TODO: write a send chunk through an adapter
            return { ResultType::Ok, conn };
        }

        return { ResultType::Err, conn };
    }

    /*- get_session -*/
    /*
        If called with a single argument, returns the whole session.
        Else return the value for the given key.
    */
    static Session const&   get_session(Conn const& conn)
    {
        return *conn.session_copy;
    }

    static std::any const& get_session(Conn const& conn, std::string const& key)
    {
        return conn.session_copy->get_session(key);
    }

    /*- put_session -*/
    /*
        Put the specified value in the session for the given key.
    */
    static Conn const put_session(Conn const& conn, std::string const& key, std::any const& value)
    {
        Conn new_conn(conn);

        new_conn.session_copy = std::move(new_conn.session_copy->put_session(key, value));
        return new_conn;
    }

    /*- delete_session -*/
    /*
        Deletes key from session.
    */
    static Conn const   delete_session(Conn const& conn, std::string const& key)
    {
        Conn new_conn(conn);
        new_conn.session_copy = std::move(new_conn.session_copy->delete_session(key));

        return new_conn;
    }

    /*- clear_session -*/
    /*
        Clears the entire session.

        Note that, even if `clear_session/1` is used, the session is still sent to the
        client. If the session should be effectively *dropped*, `configure_session/2`
        should be used with the `:drop` option set to `true`.
    */
    static Conn const   clear_session(Conn const& conn)
    {
        Conn new_conn(conn);
        return Conn::put_session(std::move(new_conn), [](std::unique_ptr<Session> s) { return s->reset_session(); });
    }

    /*- configure_session -*/
    /*
        Configures the session.

        Options:
            - "renew" -> generates a new session id for the cookie
            - "drop"  -> drops the session, a session cookie will not be included in the response
            - "ignore"-> ignore all changes made to the session in this request cycle        
    */
    static Result<Conn const>   configure_session(Conn const& conn, SessionOpt const& opt)
    {
        if (std::holds_alternative<Sent>(conn.state))
        {
            return { ResultType::Err, conn };
        }

        Conn new_conn(conn);

        if (opt != SessionOpt::WRITE)
        {
            new_conn.session_info = opt;
        }

        return { ResultType::Ok, new_conn };
    }

    /*- get_req_header -*/
    /*
        Returns the values of the request header specified by the key.
    
        Example:

        auto range =
        Conn()
            CHAIN( Conn::put_req_header, "test", "42")
            CHAIN( Conn::put_req_header, "test", "is the answer")
            CHAIN( Conn::get_req_header, "test");
        for (auto it = range.first; it != range.second; ++it)
        {
            std::cout << it->first << " " << it->second << std::endl;
        }

        Output:
        test 42
        test is the answer
    */
    static HeaderRange  get_req_header(Conn const& conn, std::string const& key)
    {
        return conn.req_headers.equal_range(key);
    }

    /*- put_req_header -*/
    /*
        Adds a new request header (key) if not present,
        otherwise replaces the previous value of that header with value.
        If key is "host", the host parameter of the connection is updated instead.

        Returns an error if the connection has already been sent, chunked or upgraded. 
    */
    static Result<Conn const>   put_req_header(Conn const& conn, std::string const& key, std::string const& value)
    {
        if (std::holds_alternative<Sent>(conn.state)
            || std::get<Unsent>(conn.state) == Unsent::CHUNKED
            || std::get<Unsent>(conn.state) == Unsent::UPGRADED)
        {
            return { ResultType::Err, conn };
        }
        Conn new_conn(conn);
        if (key == "host")
        {
            new_conn.host = ShareStr(value);
        }
        else
        {
            new_conn.req_headers.insert({key, value});
        }
        return { ResultType::Ok, new_conn };
    }

    /*- merge_req_headers -*/
    /*
        Merges a series of request headers into the connection.
        If key is "host", the host parameter of the connection is updated instead.

        Returns an error if the connection has already been sent, chunked or upgraded. 
    */
    static Result<Conn const> merge_req_headers(Conn const& conn, ImmutMapString const& headers)
    {
        if (std::holds_alternative<Sent>(conn.state)
            || std::get<Unsent>(conn.state) == Unsent::CHUNKED
            || std::get<Unsent>(conn.state) == Unsent::UPGRADED)
        {
            return { ResultType::Err, conn };
        }
        Conn new_conn(conn);

        for (auto const& [key, value] : headers)
        {
            if (key == "host")
            {
                new_conn.host = value;
            }
            else
            {
                new_conn.req_headers.insert({key, *value});
            }
        }
        return { ResultType::Ok, new_conn };
    }

    /*- delete_req_header -*/
    /*
        Deletes a request header if present.
        Return an error if the response is already sent.
    */
    static Result<Conn const> const delete_req_header(Conn const& conn, std::string const& key)
    {
        if (std::holds_alternative<Sent>(conn.state))
        {
            return { ResultType::Err, conn };
        }

        Conn new_con(conn);
        new_con.req_headers.erase(key);
        return { ResultType::Ok, new_con };
    }

    /*- fetch_query_params-*/
    /*
    Fetches query parameters from the query string.

    Params are decoded as "x-www-form-urlencoded" in which key/value pairs are separated by & and keys are separated from values by =.

    This function does not fetch parameters from the body.
    Options
        - "length" - the maximum query string length. Defaults to 1_000_000 bytes. Keep in mind the webserver you are using may have a more strict limit. For example, for the Cowboy webserver, please read.

        - "validate_utf8" - boolean that tells whether or not to validate the keys and values of the decoded query string are UTF-8 encoded. Defaults to true.
    */
    static Conn const fetch_query_params(Conn const& conn, ImmutMapString&& opts = {})
    {
        Conn new_conn(conn);

        if (!new_conn.query_params.has_value())
        {
            auto opts_len = opts.find("length");
            auto max_length = opts_len ? std::stof(**opts_len) : 1000000;

            if (new_conn.query_string->length() > max_length)
            {
                new_conn.status = std::make_optional(414);
                return new_conn;
            }

            auto query_params = ImmutMapString().transient();
            boost::char_separator sep("&");
            boost::tokenizer<boost::char_separator<char>> tok(*new_conn.query_string, sep);

            for (auto const& param : tok)
            {
                auto opts_validate_utf8 = opts.find("length");
                std::string validate_ut8 = opts_validate_utf8 && **opts_validate_utf8 == "false" ? "false" : "true";

                if (auto equal_sign = param.find("="); equal_sign == std::string::npos)
                {
                    new_conn.status = std::make_optional(414);
                    return new_conn;
                }
                else
                {
                    std::string key = param.substr(0, equal_sign);
                    std::string value = param.substr(equal_sign + 1);


                    if (validate_ut8 == "true")
                    {
                        char const* ptr = param.c_str();
                        char const* end = ptr + param.length();

                        while (ptr < end)
                        {
                            auto c = boost::locale::utf::utf_traits<char>::decode(ptr, end);   
                            if (c == boost::locale::utf::illegal || c == boost::locale::utf::incomplete)
                            {
                                new_conn.status = std::make_optional(414);
                                return new_conn;
                            }
                        }
                    }
                    query_params.set(key, ShareStr(value));
                }
            }

            new_conn.query_params = std::make_optional(query_params.persistent());
        }

        return new_conn;
    }

    /*- fetch_cookies -*/
    /*
        Fetch the cookies from req_headers and resp_cookies.
        You need to fetch the cookies in order to access them.
    */
    static Conn const fetch_cookies(Conn const& conn, immer::set<std::string>&& opts = {})
    {
        Conn new_conn(conn);

        if (!new_conn.req_cookies.has_value())
        {
            ImmutMapString req_cookies;
        
            for (auto const& [tag, cookie] : new_conn.req_headers)
            {
                if (tag != "cookie")
                {
                    continue;
                }
                req_cookies = functional::merge(req_cookies, ParseCookie(cookie));
            }
            
            ImmutMapString cookies = functional::reduce(
                new_conn.resp_cookies,
                req_cookies,
                [](std::pair<std::string const, ImmutMapString> const& cookie, ImmutMapString acc)
                {
                    if (auto const& val = cookie.second.find("value"))
                    {
                        return acc.set(cookie.first, *val);
                    }
                    return acc.erase(cookie.first);
                }
            );

            new_conn.req_cookies = std::make_optional(req_cookies);
            new_conn.cookies = std::make_optional(cookies);
        }

        if (auto const& signed_ = opts.find("signed"); signed_ != nullptr)
        {
            //TODO: verify cookies
        }
        if (auto const& encrypted = opts.find("encrypted"); encrypted != nullptr)
        {
            //TODO: decrypt cookies
        }

        return new_conn;
    }

    /*- delete_resp_cookie -*/
    /*
        Deletes a response cookie
        Deleting a cookie requires the same options as to when the cookie was put.
    */
    static Conn const delete_resp_cookie(Conn const& conn, std::string const& key, ImmutMapString&& opts = {})
    {
        Conn new_conn(conn);

        opts = opts.set("universal_time", ShareStr("Thu, 01 Jan 1970 00:00:00 GMT")) ;
        opts = opts.set("max_age", ShareStr("0"));
        if (*new_conn.scheme == "https")
        {
            opts = opts.set("secure", ShareStr("true"));
        }

        new_conn.resp_cookies = 
            new_conn.resp_cookies.update_if_exists(
                key, 
                [&opts](auto v) { (void)v; return opts; }
            );

        return new_conn;
    }

    /*- put_resp_cookie -*/
    /*
        Puts a response cookie in the connection.
        If the sign or encrypt flag are given, then the cookie value can be any term.
        If the two flags are given, returns an error and does not put the cookie in the connection.

        If the cookie is not signed nor encrypted, then the value must be a binary.
        Note the value is not automatically escaped.
        Therefore if you want to store values with non-alphanumeric characters,
        you must either sign or encrypt the cookie 
        or consider explicitly escaping the cookie value 
        by using a function such as Base.encode64(value, padding: false)
        when writing and Base.decode64(encoded, padding: false)
        when reading the cookie. 
        It is important for padding to be disabled since = is not a valid character in cookie values.

        *Signing and encrypting cookies*
        This function allows you to automatically sign and encrypt cookies.
        When signing or encryption is enabled, then any value can be stored in the cookie 
        (except anonymous functions for security reasons).
        Once a value is signed or encrypted, you must also call fetch_cookies/2
        with the name of the cookies that are either signed or encrypted.
    */
    static Result<Conn const> const put_resp_cookie(
        Conn const& conn,
        std::string const& key,
        std::string const& value,
        ImmutMapString&& opts = {}
    )
    {
        bool const sign    = opts.find("sign") != nullptr;
        bool const encrypt = opts.find("encrypt") != nullptr;
        opts = 
            opts
            .erase("sign")
            .erase("encrypt");

        auto const search  = opts.find("max_age");
        int  const max_age = search != nullptr ? std::stoi(**search) : 86400;

        if (sign && encrypt)
        {
            return { ResultType::Err, conn };
        }
        (void)max_age; // Needed for the sign/encrypt part that is to be done.

        Conn new_conn(conn);

        opts = opts.insert({"value", ShareStr(key + "_cookie=" + value)});
        new_conn.resp_cookies = new_conn.resp_cookies.set(key, opts);

        return { ResultType::Ok, new_conn };
    }

    /*- put_resp_header -*/
    /*
        Adds a new response header (key) if not present,
        otherwise replaces the previous value of that header with value.
        
        Because header keys are case-insensitive in both HTTP/1.1 and HTTP/2,
        it is recommended for header keys to be in lowercase,
        to avoid sending duplicate keys in a request.

        Returns an error if the connection is sent, chunked or upgraded.
        Returns an error if the header value contains '\r' or '\n' characters.
    */
    static Result<Conn const>   put_resp_header(Conn const& conn, std::string const& key, std::string const& value)
    {
        if (std::holds_alternative<Sent>(conn.state)
            || std::get<Unsent>(conn.state) == Unsent::CHUNKED
            || std::get<Unsent>(conn.state) == Unsent::UPGRADED
            || value.rfind("\n") != std::string::npos
            || value.rfind("\r") != std::string::npos)
        {
            return { ResultType::Err, conn };
        }

        Conn new_conn(conn);
        new_conn.resp_headers.insert({key, value});
        return { ResultType::Ok, new_conn };
    }

    /*- delete_resp_header -*/
    /*
        Deletes a response header if present.

        Returns an error if the connection is sent, chunked or upgraded.
    */
    static Result<Conn const>   delete_resp_header(Conn const& conn, std::string const& key)
    {
        if (std::holds_alternative<Sent>(conn.state)
            || std::get<Unsent>(conn.state) == Unsent::CHUNKED
            || std::get<Unsent>(conn.state) == Unsent::UPGRADED)
        {
            return { ResultType::Err, conn };
        }

        Conn new_conn(conn);
        new_conn.resp_headers.erase(key);
        return { ResultType::Ok, new_conn };
    }

    /*- get_resp_header -*/
    /*
        Returns the values of the response header specified by key.

        Example:

        auto range =
        Conn()
            CHAIN( Conn::put_resp_header, "test", "42")
            CHAIN( Conn::put_resp_header, "test", "is the answer")
            CHAIN( Conn::get_resp_header, "test");
        for (auto it = range.first; it != range.second; ++it)
        {
            std::cout << it->first << " " << it->second << std::endl;
        }

        Output:
        test 42
        test is the answer
    */
    static HeaderRange const get_resp_header(Conn const& conn, std::string const& key)
    {
        return conn.resp_headers.equal_range(key);
    }

    /*- halt -*/
    /*
        Halts the plug pipeline (not yet implemented)
    */
    static Conn const halt(Conn const& conn)
    {
        Conn new_conn(conn);

        new_conn.halted = true;
        return new_conn;
    }
};

typedef std::optional<std::vector<SharedString>> PlugOptions;

typedef std::function<Conn(Conn, PlugOptions)> Plug;

}
}

#endif