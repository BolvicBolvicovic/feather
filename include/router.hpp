/*--- Header file for router ---*/

#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "core.hpp"


namespace feather::router
{

namespace core = feather::core;
namespace plug = core::plug;

/*--- Router ---*/
/*
    Helper struct for the user to define a pipeline and routes.
    Not mendatory but strongly recommended.
*/
struct Router
{
    using RouterVec = immer::vector<plug::Plug>;
    using RouterVecTransient = immer::vector_transient<plug::Plug>;
    using RouterMap = immer::map<std::string, RouterVec>;
    using Pipeline = std::function<RouterVecTransient&(RouterVecTransient&)>;
private:
    RouterMap   pipelines;
    RouterMap   scopes;
public:
    Router()              = default;
    Router(Router&&)      = default;
    Router(Router const&) = default;
    ~Router()             = default;

    /*- pipeline -*/
    /*
        Once a request arrives at the Phoenix router,
        it performs a series of transformations through pipelines
        until the request is dispatched to a desired route.

        Such transformations are defined via plugs, as defined in the core header.
        Once a pipeline is defined, it can be piped through per scope.

        Usage:

        #include "my_plugs.hpp"
        #include "my_controller.hpp"

        static auto const my_router =
            Router()
            CHAIN( Router::pipeline, "browser", [](RouterVecTransient& vec)
            {

                PLUG( vec, Conn::fetch_session );
                PLUG( vec, my_plugs::accepts, {"html"} );

                return vec;
            })
            CHAIN( Router::pipeline, "auth", [](RouterVecTransient& vec)
            {

                PLUG( vec, my_plugs::ensure_authenticated );

                return vec;
            })
            CHAIN( Router::scope, "/", [](Router const& router, RouterVecTransient& vec)
            {
                PIPE_THROUGH( vec, router, {"browser", "auth"} );

                GET( vec, "/posts/new", my_controller::new );
                POST( vec, "/posts", my_controller::create );

                return vec;
            })
            CHAIN( Router::scope, "/", [](Router const& router, RouterVecTransient& vec)
            {
                PIPE_THROUGH( vec, router, {"browser"} );

                GET( vec, "/posts", my_controller::index );
                GET( vec, "/posts/:id", my_controller::show );

            });
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

    /*- PLUG -*/
    /*
        Macro helper for pipeline callback. Allows you to pass an optionable argument to a Plug.
    */
#define PLUG(vec, func, ...) vec.push_back([&](feather::core::plug::Conn const& conn, feather::core::plug::PlugOptions) { return func(conn __VA_OPT__(,) __VA_ARGS__) })

    /*- PIPE_THROUGH -*/
    /*
        Macro helper to tell to a scope which pipeline(s) should be used before accessing the routes.
        If there are many pipelines, they are called in the order writen.
    */
#define PIPE_THROUGH(vec, router, pipelines) vec.push_back([&](feather::core::plug::Conn const& conn, feather::core::plug::PlugOptions={}) \
{ \
    feather::core::plug::Conn new_conn(conn); \
\
    for (auto const& p_name : pipelines)\
    {\
        new_conn = router.pipelines[p_name](new_conn, {});\
    }\
    return new_conn;\
})

};

} // namespace router


#endif