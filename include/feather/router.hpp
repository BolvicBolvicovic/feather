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

    Router is a singleton. Therefor all its static methods directly modify the router itself.
    The reason behind this design is that they only register anonymous functions when we initialize it with a custom user defined initialize function.

    Usage:

    #include "my_plugs.hpp"
    #include "my_controller.hpp"

    auto MyRouter()
    {
        static auto const my_router =
            Router()
            CHAIN( Router::pipeline, "test1",
                (CALLBACK_PLINE
                {
                    PLUG( Conn::fetch_cookies );
                    PLUG( Conn::fetch_query_params );

                    END_PLINE;
                }))
            CHAIN( Router::pipeline, "test2",
                (CALLBACK_PLINE
                {
                    PLUG( Conn::assign, "key_test", 42 );

                    END_PLINE;
                }))
            CHAIN( Router::scope, "/",
                (CALLBACK_SCOPE
                {
                    PIPE_THROUGH( "test1", "test2" );

                    GET( "/posts/new", nullptr );
                    POST( "/posts", nullptr );

                    END_SCOPE;
                }))
            CHAIN( Router::scope, "/",
                (CALLBACK_SCOPE
                {
                    PIPE_THROUGH( "test1" );

                    GET( "/posts", nullptr );
                    GET( "/users/123", nullptr );

                    END_SCOPE;
                }));

        return my_router;
    }
*/
struct Router
{
    using Pipeline = std::function<RouterVecTransient&(RouterVecTransient&)>;
    using IntoScope = std::function<Scope(Scope&&)>;
    using RouterInstance = std::shared_ptr<Router>;
private:
    static inline RouterInstance instance = nullptr;
    Router() = default;
public:
    RouterMap       pipelines;
    RouterMultiMap  scopes;
    ~Router()             = default;

    Router& operator=(Router const& other) = delete;

    /*- fetch_instance -*/
    /*
        Request the instance of the singleton Router.
        If it is the first request, initialize the instance.
    */
    static RouterInstance fetch_instance()
    {
        if (!instance)
        {
            instance = RouterInstance(new Router());
        }
        return instance;
    }

    /*- pipeline -*/
    /*
        Once a request arrives at the Phoenix router,
        it performs a series of transformations through pipelines
        until the request is dispatched to a desired route.

        Such transformations are defined via plugs, as defined in the core header.
        Once a pipeline is defined, it can be piped through per scope.

    */
    static RouterInstance pipeline(
        RouterInstance      router,
        std::string const&  name,
        Pipeline            pl)
    {
        RouterVecTransient new_pipeline{};

        router->pipelines = router->pipelines.insert({name, pl(new_pipeline).persistent()});

        return router;
    }

    /*- scope -*/
    /*
        Register a scope and its routes to a router.
    */
    static RouterInstance scope(
        RouterInstance      router,
        std::string const&  name,
        IntoScope           handler
    )
    {
        router->scopes = router->scopes.insert({name, handler(Scope())});
        return router;
    }

    /*- CALLBACK_PLINE -*/
    /*
        Macro helper to define a pipeline callback.
    */
#define CALLBACK_PLINE +[](RouterVecTransient& vec) -> RouterVecTransient&

    /*- PLUG -*/
    /*
        Macro helper for pipeline callback. Allows you to pass an optionable argument to a Plug.
    */
#define PLUG(func, ...) vec.push_back([&](feather::core::plug::Conn const& conn, feather::core::plug::PlugOptions={}) { return func(conn __VA_OPT__(,) __VA_ARGS__); })

    /*- END_PLINE -*/
    /*
        Macro helper to significate the end of a pipeline callback.
    */
#define END_PLINE return vec

    /*- CALLBACK_SCOPE -*/
    /*
        Macro helper to define a scope callback.
    */
#define CALLBACK_SCOPE +[](Scope&& scope)

#define END_SCOPE return scope

    /*- PIPE_THROUGH -*/
    /*
        Macro helper to tell to a scope which pipeline(s) should be used before accessing the routes.
        If there are many pipelines, they are called in the order writen.
    */
#define PIPE_THROUGH(p_lines...) scope.pipeline = ([&](feather::core::plug::Conn const& conn, feather::core::plug::PlugOptions={}) \
{ \
    feather::core::plug::Conn new_conn(conn); \
    std::vector<std::string> p_lines_as_vec{p_lines};\
    feather::router::Router::RouterInstance instance = feather::router::Router::fetch_instance();\
\
    for (auto const& p_name : p_lines_as_vec)\
    {\
        for (auto const& pip : instance->pipelines[p_name])\
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
#define GET(path, handler) scope.get = scope.get.insert({path, handler})

    /*- POST -*/
    /*
        Macro helper registering a post callback for a scope.
    */
#define POST(path, handler) scope.post = scope.post.insert({path, handler})

    /*- PUT -*/
    /*
        Macro helper registering a put callback for a scope.
    */
#define PUT(path, handler) scope.put = scope.put.insert({path, handler})

    /*- DEL -*/
    /*
        Macro helper registering a delete callback for a scope.
    */
#define DEL(path, handler) scope.del = scope.del.insert({path, handler})

/*- handler -*/
/*
    Route handler to register to the server.
*/
static plug::Conn const handler(plug::Conn const& conn)
{
    RouterInstance instance = fetch_instance();

    for (auto const& [scope_id, scopes] : instance->scopes)
    {
        if (auto path_start = conn.request_path->find(scope_id); path_start == std::string::npos)
        {
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


}; // struct router


} // namespace router


#endif