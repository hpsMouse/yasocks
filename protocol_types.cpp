#include "protocol_types.h"

template <typename T>
static void push_back_obj(std::vector<char>& buffer, T const& object)
{
    char const* p = reinterpret_cast<char const*>(&object);
    buffer.insert(buffer.end(), p, p + sizeof(object));
}

void makeConnectionResponse(ConnectionStatus status, std::vector< char >& buffer, const boost::asio::ip::address& addr, const NetU16& port)
{
    buffer.clear();
    buffer.push_back(5);                                // version
    buffer.push_back(static_cast<uint8_t>(status));     // status
    buffer.push_back(0);                                // reserved
    if(addr.is_v4())
    {
        buffer.push_back(static_cast<uint8_t>(AddressType::IPv4));
        push_back_obj(buffer, addr.to_v4().to_bytes());
        push_back_obj(buffer, port);
    }
    else if(addr.is_v6())
    {
        buffer.push_back(static_cast<uint8_t>(AddressType::IPv6));
        push_back_obj(buffer, addr.to_v6().to_bytes());
        push_back_obj(buffer, port);
    }
    else
    {
        buffer.push_back(static_cast<uint8_t>(AddressType::HostName));
        auto const& str = addr.to_string();
        buffer.push_back(static_cast<uint8_t>(str.length()));
        buffer.insert(buffer.end(), str.begin(), str.end());
        push_back_obj(buffer, port);
    }
}
