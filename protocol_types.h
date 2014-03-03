#ifndef _B7E892D0_9DF1_11E3_A9A8_206A8A22A96A
#define _B7E892D0_9DF1_11E3_A9A8_206A8A22A96A

#include <cstdint>

#include <vector>

#include <boost/asio/ip/address.hpp>

#include <arpa/inet.h>

#define PACKED_STRUCT __attribute__((packed))

enum class AuthMethod : uint8_t
{
    NoAuth              = 0,
    GSSAPI              = 1,
    UserPass            = 2,
    NoSuitableMethod    = 0xff
};

enum class AuthStatus : uint8_t
{
    Success = 0,
    Failure = 1                         // All non-zero numbers mean failure
};

enum class Command : uint8_t
{
    TcpConnect  = 1,
    TcpBind     = 2,
    UdpBind     = 3
};

enum class AddressType : uint8_t
{
    NoAddress   = 0,
    IPv4        = 1,
    HostName    = 3,
    IPv6        = 4
};

enum class ConnectionStatus : uint8_t
{
    Granted                     = 0,
    GeneralFailure              = 1,
    BannedByRuleset             = 2,
    NetworkUnreachable          = 3,
    HostUnreachable             = 4,
    ConnectionRefused           = 5,
    TtlExpired                  = 6,
    CommandNotSupported         = 7,
    AddressTypeNotSupported     = 8
};

struct NetU16
{
    NetU16(): repr() {}
    explicit NetU16(uint16_t hostshort): repr(htons(hostshort)) {}
    uint16_t toHost() const
    { return ntohs(repr); }
    
    uint16_t repr;
};

struct PACKED_STRUCT ClientGreeting
{
    struct
    {
        uint8_t socksVer;                   // Must be 5
        uint8_t numAuthMethods;
    } header;
    uint8_t authMethods[255];
};

struct PACKED_STRUCT ServerGreeting
{
    uint8_t socksVer;                   // Must be 5
    AuthMethod chosenAuthMethod;
};

struct PACKED_STRUCT SocksString
{
    uint8_t length;
    uint8_t contents[255];
};

struct PACKED_STRUCT UserPassRequest
{
    uint8_t version;                    // Must be 1
                                        // Two socks strings
};

struct PACKED_STRUCT UserPassResponse
{
    uint8_t version;                    // Must be 1
    AuthStatus status;
};

union AddressData
{
    boost::asio::ip::address_v4::bytes_type v4Addr;
    boost::asio::ip::address_v6::bytes_type v6Addr;
    SocksString hostName;
};

struct ConnectionRequest
{
    ConnectionRequest(): header(), destAddress(), destPort() {}
    struct PACKED_STRUCT
    {
        uint8_t socksVer;                   // Must be 5
        Command command;
        uint8_t reserved;                   // Must be 0
        AddressType addressType;
    } header;
    AddressData destAddress;
    NetU16 destPort;
};

void makeConnectionResponse(ConnectionStatus status, std::vector<char> &buffer, boost::asio::ip::address const& addr, NetU16 const& port);

inline void makeConnectionResponse(ConnectionStatus status, std::vector<char> &buffer)
{
    makeConnectionResponse(status, buffer, boost::asio::ip::address_v4(), NetU16(0));
}

template <typename Endpoint>
inline void makeConnectionResponse(ConnectionStatus status, std::vector<char> &buffer, Endpoint const& endpoint)
{
    makeConnectionResponse(status, buffer, endpoint.address(), NetU16(endpoint.port()));
}

#endif
