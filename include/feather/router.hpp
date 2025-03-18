/*--- Header file for router ---*/

#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <feather/core.hpp>


namespace feather::router
{

namespace core = feather::core;
namespace plug = core::plug;


using RouterVec = immer::vector<plug::Plug>;
using RouterVecTransient = immer::vector_transient<plug::Plug>;
using RouterMap = immer::map<std::string, RouterVec>;

using HttpHandler = std::function<plug::Conn const(plug::Conn const&)>;

struct Scope
{
    plug::Plug  pipeline;
    immer::map<std::string, HttpHandler> get;
    immer::map<std::string, HttpHandler> post;
    immer::map<std::string, HttpHandler> put;
    immer::map<std::string, HttpHandler> del;
};

using RouterMultiMap = core::functional::multimap<std::string, Scope>;

/*--- Router ---*/
/*
    Helper struct for the user to define a pipeline and routes.
    Not mendatory but strongly recommended.

    Usage:

    #include "my_plugs.hpp"
    #include "my_controller.hpp"

    auto MyRouter()
    {
        static auto const my_router =
            Router()
            CHAIN( Router::pipeline, "test1",
                (+[](RouterVecTransient& vec) -> RouterVecTransient&
                {
                    PLUG( vec, Conn::fetch_cookies );
                    PLUG( vec, Conn::fetch_query_params );

                    return vec;
                }))
            CHAIN( Router::pipeline, "test2",
                (+[](RouterVecTransient& vec) -> RouterVecTransient&
                {
                    PLUG( vec, Conn::assign, "key_test", 42);

                    return vec;
                }))
            CHAIN( Router::scope, "/",
                (+[](Scope&& s, Router const& r)
                {
                    PIPE_THROUGH( s, r, "test1", "test2" );

                    GET( s, "/posts/new", my_controller::new );
                    POST( s, "/posts", my_constroller::post );

                    return s;
                }))
            CHAIN( Router::scope, "/",
                (+[](Scope&& s, Router const& r)
                {
                    PIPE_THROUGH( s, r, "test1" );

                    GET( s, "/posts", my_controller::index );
                    GET( s, "/posts/:id", my_controller::show );

                    return s;
                }));

        return my_router;
    }
*/
struct Router
{
    using Pipeline = std::function<RouterVecTransient&(RouterVecTransient&)>;
    using IntoScope = std::function<Scope(Scope&&, Router const&)>;
public:
    RouterMap       pipelines;
    RouterMultiMap  scopes;
    Router()              = default;
    Router(Router&&)      = default;
    Router(Router const&) = default;
    ~Router()             = default;

    Router operator=(Router const& other)
    {
        return Router(other);
    }

    /*- pipeline -*/
    /*
        Once a request arrives at the Phoenix router,
        it performs a series of transformations through pipelines
        until the request is dispatched to a desired route.

        Such transformations are defined via plugs, as defined in the core header.
        Once a pipeline is defined, it can be piped through per scope.

    */
    static Router const pipeline(
        Router const&       router,
        std::string const&  name,
        Pipeline            pl)
    {
        Router new_router(router);
        RouterVecTransient new_pipeline{};

        new_router.pipelines = new_router.pipelines.insert({name, pl(new_pipeline).persistent()});

        return new_router;
    }

    /*- scope -*/
    /*
        Register a scope and its routes to a router.
    */
    static Router const scope(
        Router const&       router,
        std::string const&  name,
        IntoScope           handler
    )
    {
        Router new_router(router);
        new_router.scopes = new_router.scopes.insert({name, handler(Scope(), router)});
        return new_router;
    }

    /*- PLUG -*/
    /*
        Macro helper for pipeline callback. Allows you to pass an optionable argument to a Plug.
    */
#define PLUG(vec, func, ...) vec.push_back([&](feather::core::plug::Conn const& conn, feather::core::plug::PlugOptions={}) { return func(conn __VA_OPT__(,) __VA_ARGS__); })


    /*- PIPE_THROUGH -*/
    /*
        Macro helper to tell to a scope which pipeline(s) should be used before accessing the routes.
        If there are many pipelines, they are called in the order writen.
    */
#define PIPE_THROUGH(scope, router, p_lines...) scope.pipeline = ([&](feather::core::plug::Conn const& conn, feather::core::plug::PlugOptions={}) \
{ \
    feather::core::plug::Conn new_conn(conn); \
    std::vector<std::string> p_lines_as_vec{p_lines};\
\
    for (auto const& p_name : p_lines_as_vec)\
    {\
        std::cout << "Pipeline name: " << p_name << std::endl;\
        for (auto const& pip : router.pipelines[p_name])\
        {\
            new_conn = pip(new_conn, {});\
        }\
    }\
    return new_conn;\
})

    /*- GET -*/
    /*
        Macro helper registering a get callback for a scope.
    */
#define GET(scope, path, handler) scope.get = scope.get.insert({path, handler})

    /*- POST -*/
    /*
        Macro helper registering a post callback for a scope.
    */
#define POST(scope, path, handler) scope.post = scope.post.insert({path, handler})

    /*- PUT -*/
    /*
        Macro helper registering a put callback for a scope.
    */
#define PUT(scope, path, handler) scope.put = scope.put.insert({path, handler})

    /*- DEL -*/
    /*
        Macro helper registering a delete callback for a scope.
    */
#define DEL(scope, path, handler) scope.del = scope.del.insert({path, handler})


}; // struct router

/* router */
/*
    Static variable that holds the router of the application.
*/
static Router router;

/*- router_handler -*/
/*
    Route handler to register to the server.
*/
inline plug::Conn const router_handler(plug::Conn const& conn)
{
    std::cout << "scopes size: " << router.scopes.size() << std::endl;
    for (auto const& [scope_id, scopes] : router.scopes)
    {
        if (auto path_start = conn.request_path->find(scope_id); path_start == std::string::npos)
        {
            std::cout << "scope " << scope_id << " not found" << std::endl;
            continue;
        }
        else
        {
            std::string path = conn.request_path->substr(path_start);

            for (auto const& scope : scopes)
            {
                if (*conn.method == "get" && scope.get.find(path) != nullptr) // TODO: Need to handle /:id type of path
                {
                    if (scope.get[path] == nullptr) return scope.pipeline(conn, {});
                    return scope.pipeline(conn, {}) pipe scope.get[path];
                }
                else if (*conn.method == "post" && scope.post.find(path) != nullptr)
                {
                    if (scope.post[path] == nullptr) return scope.pipeline(conn, {});
                    return scope.pipeline(conn, {}) pipe scope.post[path];
                }
                else if (*conn.method == "put" && scope.put.find(path) != nullptr)
                {
                    if (scope.put[path] == nullptr) return scope.pipeline(conn, {});
                    return scope.pipeline(conn, {}) pipe scope.put[path];
                }
                else if (*conn.method == "delete" && scope.del.find(path) != nullptr)
                {
                    if (scope.del[path] == nullptr) return scope.pipeline(conn, {});
                    return scope.pipeline(conn, {}) pipe scope.del[path];
                }
            }

        }

    }

    return conn;
}


} // namespace router


#endif