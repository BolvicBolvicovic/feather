#include <server_test.hpp>
#include <controller_test.hpp>
#include <core_test.hpp>
#include <router_test.hpp>

int main()
{
    run_core_test();
    run_router_test();
    return 0;
}