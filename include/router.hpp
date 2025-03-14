/*--- Header file for router ---*/

#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "core.hpp"


namespace feather::router
{

namespace utils = feather::core;

template <class Body, class Allocator>
struct Router
{
    immer::map<std::string, std::queue<utils::plug::Plug>>    pipelines;
};

}


#endif