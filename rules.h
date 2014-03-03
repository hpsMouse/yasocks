#ifndef _73B0A634_9F8B_11E3_9917_206A8A22A96A
#define _73B0A634_9F8B_11E3_9917_206A8A22A96A

#include <boost/asio/ip/tcp.hpp>

#include "protocol_types.h"

bool checkClient(boost::asio::ip::tcp::endpoint const& client, AuthMethod method);
bool checkTarget(boost::asio::ip::tcp::endpoint const& target, Command command);

struct TargetChecker
{
    TargetChecker(Command command): command(command) {}
    
    bool operator() (boost::asio::ip::tcp::endpoint const& target)
    {
        return checkTarget(target, command);
    }
    
    Command command;
};

#endif
