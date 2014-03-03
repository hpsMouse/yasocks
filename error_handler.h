#ifndef _48B808D6_9E01_11E3_A67B_206A8A22A96A
#define _48B808D6_9E01_11E3_A67B_206A8A22A96A

#include <utility>

#include <boost/system/error_code.hpp>

#include "logging.h"

template <typename Func>
class ErrorHandler
{
public:
    ErrorHandler(Func const& success, char const* call):
    func(success),
    call(call)
    {}
    
    template <typename ... Args>
    void operator() (boost::system::error_code const& error, Args&& ... args)
    {
        if(error)
            logging::error("%1%: %2%", call, error.message());
        else
            func(std::forward<Args>(args)...);
    }
    
    ErrorHandler<Func>(ErrorHandler<Func> const&) = default;
    ErrorHandler<Func>& operator = (ErrorHandler<Func> const&) = default;
    
private:
    Func func;
    char const* call;
};

template <typename Func>
inline ErrorHandler<Func> error_handler(char const* call, Func const& success)
{
    return ErrorHandler<Func>(success, call);
}

template <typename Func>
class IgnoreSize
{
public:
    IgnoreSize(Func const& func):
    func(func)
    {}
    
    template <typename ... Args>
    void operator() (std::size_t, Args&& ... args)
    {
        func(std::forward<Args>(args)...);
    }
    
private:
    Func func;
};

template <typename Func>
inline ErrorHandler<IgnoreSize<Func>> nosize(char const* call, Func const& success)
{
    return error_handler(call, IgnoreSize<Func>(success));
}

template <typename FuncFail, typename FuncSucc>
class ErrorBranch
{
public:
    ErrorBranch(FuncFail const& fail, FuncSucc const& succ):
    fail(fail),
    succ(succ)
    {}
    
    template <typename ... Args>
    void operator() (boost::system::error_code const& error, Args&& ... args)
    {
        if(error)
            fail(error);
        else
            succ(std::forward<Args>(args)...);
    }
    
private:
    FuncFail fail;
    FuncSucc succ;
};

template <typename FuncFail, typename FuncSucc>
inline ErrorBranch<FuncFail, FuncSucc> error_branch(FuncFail const& fail, FuncSucc const& succ)
{
    return ErrorBranch<FuncFail, FuncSucc>(fail, succ);
}

#endif
