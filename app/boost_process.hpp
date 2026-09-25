#pragma once

#if __has_include(<boost/process/v1.hpp>)
#    include <boost/process/v1.hpp>
namespace rauch
{
namespace process = boost::process::v1;
}
#else
#    define BOOST_PROCESS_V2_HEADER_ONLY
#    include <boost/process.hpp>
namespace rauch
{
namespace process = boost::process;
}
#endif
