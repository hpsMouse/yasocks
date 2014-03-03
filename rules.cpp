#include "rules.h"

bool checkClient(const boost::asio::ip::tcp::endpoint&, AuthMethod)
{
    return true;
}

bool checkTarget(const boost::asio::ip::tcp::endpoint& target, Command command)
{
    if(command == Command::TcpConnect && !target.address().is_loopback())
        return true;
    return false;
}
