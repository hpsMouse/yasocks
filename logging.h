#ifndef _1B3A1D9E_9DF8_11E3_BE47_206A8A22A96A
#define _1B3A1D9E_9DF8_11E3_BE47_206A8A22A96A

#include <boost/format.hpp>

namespace logging
{
    void raw_log(std::string const& msg);

    inline std::string log_format(boost::format& format)
    {
        return format.str();
    }

    template <typename Arg, typename ... Args>
    inline std::string log_format(boost::format& format, Arg const& arg, Args const& ... args)
    {
        format % arg;
        return log_format(format, args...);
    }

    template <typename ... Args>
    inline void prefixed_log(std::string const& prefix, char const* format, Args const& ... args)
    {
        boost::format fmt(format);
        raw_log(prefix + log_format(fmt, args...));
    }
    
#define YASOCKS_LOGGING_DEFINE(fun, prefix)\
    template <typename ... Args>\
    inline void fun(char const* format, Args const& ... args)\
    {\
        prefixed_log(#prefix ": ", format, args...);\
    }\
    
    YASOCKS_LOGGING_DEFINE(info, INFO)
    YASOCKS_LOGGING_DEFINE(debug, DEBUG)
    YASOCKS_LOGGING_DEFINE(error, ERROR)
}

#endif
