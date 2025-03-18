/*--- Code file for test router ---*/

#include <router_test.hpp>

using namespace feather::router;
using namespace feather::core::plug;


void Router_pipeline_test(void)
{
    router =
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

                GET( s, "/posts/new", nullptr );
                POST( s, "/posts", nullptr );

                return s;
            }))
        CHAIN( Router::scope, "/",
            (+[](Scope&& s, Router const& r)
            {
                PIPE_THROUGH( s, r, "test1" );

                GET( s, "/posts", nullptr );
                GET( s, "/users/123", nullptr );

                return s;
            }));
    
    Conn conn = buildFirstConn() pipe router_handler;
    
    assert(conn.cookies.has_value());
}

void run_router_test(void)
{
    std::function<void(void)> const tests[] = {
        Router_pipeline_test,
    };
    size_t counter = 0;

    for (auto const& test : tests)
    {
        test();
        counter++;
    }

    std::cout << "router tests: " << counter << " success." << std::endl;
}