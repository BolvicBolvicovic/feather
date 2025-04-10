/*--- Header file for server ---*/

#ifndef SERVER_HPP
#define SERVER_HPP

#include <feather/router.hpp>


namespace feather::core
{

struct Server
{
    using WebSocketServer = websocketpp::server<websocketpp::config::asio>;
    using ConnectionHdl = websocketpp::connection_hdl;

    struct User
    {
        std::shared_ptr<plug::Session>  session;
        ConnectionHdl                   hdl;
    };

    public:
        WebSocketServer                                     server;
        boost::asio::io_service                            io_service;
    private:
        std::mutex                                          conn_lock;
        boost::uuids::random_generator                      uuid_generator;
        std::unordered_map<std::string, User>               connections;
    public:
        Server()
        {
            server.clear_access_channels(websocketpp::log::alevel::all);
            server.init_asio(&io_service);

            server.set_http_handler([this](ConnectionHdl hdl)
            {
                using namespace feather::core::plug;
                using namespace websocketpp::http;


                auto req = 
                    server.get_con_from_hdl(hdl)->get_request().raw()
                    pipe Server::parse_request
                    pipe unwrap<http::Request>;

                auto req_cpy = req;
                auto session = std::make_shared<CookieSession>();
                
                Conn conn = [&](Conn const& c) {
                    auto req_with_cookies = Conn::fetch_cookies(c);
                    if (auto const& _id = req_with_cookies.req_cookies.value().find("id"); _id != nullptr)
                    {
                        std::lock_guard<std::mutex> lock(conn_lock);
                        return Conn(std::move(req), connections[**_id].session);
                    }
                    else
                    {
                        std::string id = uuid_generator() pipe boost::uuids::to_string;

                        std::lock_guard<std::mutex> lock(conn_lock);
                        connections[id] = {session, hdl};
                        return Conn::put_resp_cookie(c, "id", id).second;
                    }

                }(Conn(std::move(req_cpy), session));
                
                auto ready_for_resp = router::Router::handler(conn);

                auto response = server.get_con_from_hdl(hdl)->get_response();
                
                auto status = ready_for_resp.status.value_or(200);
                response.set_status(static_cast<status_code::value>(status));

                response.set_max_body_size(8000000);

                if (ready_for_resp.resp_body.use_count() != 0)
                {
                    response.set_body(*ready_for_resp.resp_body);
                }

                for (auto const& [key, value] : ready_for_resp.resp_headers)
                {
                    response.append_header(key, value);
                }

                for (auto [key, cookie] : ready_for_resp.resp_cookies)
                {
                    std::string set_cookie = "";
                    if (auto const& value = cookie.find("value"); value != nullptr)
                    {
                        set_cookie += **value;
                    }
                    else
                    {
                        continue;
                    }

                    if (auto const& path = cookie.find("path"); path != nullptr)
                    {
                        set_cookie += "; Path=" + **path;
                    }
                    else
                    {
                        set_cookie += "; Path=/";
                    }

                    if (auto const& domain = cookie.find("domain"); domain != nullptr)
                    {
                        set_cookie += "; Domain=" + **domain;
                    }

                    if (auto const& max_age = cookie.find("max_age"); max_age != nullptr)
                    {
                        set_cookie += "; Max-Age=" + **max_age;
                    }

                    if (auto const& expires = cookie.find("expires"); expires != nullptr)
                    {
                        set_cookie += "; Expires=" + **expires;
                    }

                    if (cookie.find("secure") != nullptr)
                    {
                        set_cookie += "; Secure";
                    }

                    if (cookie.find("httponly") != nullptr)
                    {
                        set_cookie += "; HttpOnly";
                    }

                    if (auto const& same_site = cookie.find("same_site"); same_site != nullptr)
                    {
                        set_cookie += "; SameSite=" + **same_site;
                    }


                    response.append_header("Set-Cookie", set_cookie);
                }
            });

            server.set_open_handler([this](ConnectionHdl hdl)
            {
                std::string id = uuid_generator() pipe boost::uuids::to_string;

                std::lock_guard<std::mutex> lock(conn_lock);
                connections[id] = {std::make_shared<plug::CookieSession>(), hdl};
            });

            server.set_message_handler([this](ConnectionHdl hdl, WebSocketServer::message_ptr)
            {
                /*TODO: change all of this to Phoenix Channel*/
                using namespace feather::core::plug;

                auto user = [&]()
                {
                    for (auto const& conn : connections)
                    {
                        if (server.get_con_from_hdl(conn.second.hdl) == server.get_con_from_hdl(hdl))
                        {
                            return conn.second;
                        }
                    }
                    throw std::runtime_error("Conn is not recorded");
                }();
                
                auto req = 
                    server.get_con_from_hdl(hdl)->get_request().raw()
                    pipe Server::parse_request
                    pipe unwrap<http::Request>;

                Conn conn(std::move(req), user.session);
                conn.state = Unsent::UPGRADED;
                router::Router::handler(conn);
            });
        }
        ~Server() = default;

        /*- parse_request-*/
        /*
            Parse a raw request into an http::Request
        */
        static Result<http::Request> parse_request(std::string const& raw)
        {
            http::Request req;
            boost::char_separator sep("\n");
            boost::char_separator line_sep(" ");
            bool first_line = true;
            bool is_body = false;
            boost::tokenizer<boost::char_separator<char>> tok(raw, sep);

            for (auto const& line : tok)
            {
                auto len = line.length();

                if (first_line)
                {
                    if (len == 1)
                    {
                        return { ResultType::Err, req };
                    }
                    boost::tokenizer<boost::char_separator<char>> t_line(line, line_sep);
                    size_t count = 0;
                    for (auto const& word : t_line)
                    {
                        switch (count)
                        {
                            case 0: req.method = word; break;
                            case 1: req.target = word; break;
                            case 2: req.version = word.substr(0, word.length() - 1); break;
                            default: break;
                        }

                        ++count;
                    }

                    // Skip URL fragment
                    if (auto const& pos = req.target.find('#'); pos != std::string::npos)
                    {
                        req.target.erase(pos);
                    }

                    auto slash_mark = req.target.find("/");
                    if (auto const& question_mark = req.target.find("?"); question_mark != std::string::npos)
                    {
                        req.path = req.target.substr(slash_mark, question_mark);
                    }
                    else
                    {
                        req.path = req.target.substr(slash_mark);
                    }
                    
                    static const std::set<std::string> methods
                    {
                        "GET", "HEAD", "POST", "PUT", "DELETE",
                        "CONNECT", "OPTION", "TRACE", "PATCH", "PRI"
                    };

                    if (methods.find(req.method) == methods.end()
                        || (req.version != "HTTP/1.1"
                            && req.version != "HTTP/1.0"))
                    {
                        std::cout << "Error version" << std::endl;
                        return { ResultType::Err, req };
                    }
                }
                else if (len == 1)
                {
                    is_body = true;
                }
                else if (is_body)
                {
                    req.body.append(line + "\n");
                }
                else
                {
                    auto double_dot = line.find(":");
                    std::string key = line.substr(0, double_dot);
                    std::string value =
                        line.substr(double_dot + 1, len - 1)
                        CHAIN( boost::trim_copy );
                    req.headers.emplace(key, value);
                }

                first_line = false;
            }

            req.body.erase(req.body.rfind("\n"));

            return {ResultType::Ok, req};
        }

        /*- start -*/
        /*
            Start the server.
            Server cannot be const because of the server.listen()
        */
        static void start(Server& server, std::string const& host, uint16_t const& port)
        {
            try {
                // Configure server endpoint
                server.server.set_reuse_addr(true);
                
                // Set up the address
                boost::asio::ip::address addr;
                if (host.empty()) {
                    addr = boost::asio::ip::address_v4::any();
                } else {
                    addr = boost::asio::ip::make_address(host == "localhost" ? "127.0.0.1" : host);
                }
                
                // Configure server
                server.server.listen(addr, port);
                server.server.start_accept();
            } catch (const std::exception& e) {
                std::cerr << "Server configuration error: " << e.what() << std::endl;
                throw;
            }
        }

        /*- stop -*/
        /*
            Stop the server.
            Server cannot be const because of the server.stop()
        */
        static void stop(Server& server)
        {
            try {
                server.server.stop();
                server.io_service.stop();
            } catch (const std::exception& e) {
                std::cerr << "Server stop error: " << e.what() << std::endl;
                throw;
            }
        }
}; // struct Server

} // namespace core
#endif