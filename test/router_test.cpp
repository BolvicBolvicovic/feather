/*--- Code file for test router ---*/

#include <router_test.hpp>

using namespace feather::router;
using namespace feather::core::plug;

void Router_init_test(void)
{
    router =
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
    
    assert(router.scopes["/"][1].pipeline != nullptr);
}

void Router_router_handler_test(void)
{
    
    Conn conn = buildFirstConn() pipe router_handler;
    
    assert(*conn.method == "get");
    assert(conn.cookies.has_value());
}

void run_router_test(void)
{
    std::function<void(void)> const tests[] = {
        Router_init_test,
        Router_router_handler_test,
    };
    size_t counter = 0;

    for (auto const& test : tests)
    {
        test();
        counter++;
    }

    std::cout << "router tests: " << counter << " success." << std::endl;
}