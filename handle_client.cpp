#include <algorithm>
#include <chrono>
#include <type_traits>

#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/steady_timer.hpp>

#include <boost/iterator/filter_iterator.hpp>

#include <boost/lexical_cast.hpp>

#include "handle_client.h"

#include "error_handler.h"
#include "logging.h"
#include "protocol_types.h"
#include "rules.h"

static unsigned activeCount = 0;
static unsigned maxActive = 0;

struct ControlBlock
{
    ControlBlock(TCPSocket&& peer, TCPEndpoint&& peer_endpoint):
    peer(std::move(peer)),
    peer_endpoint(std::move(peer_endpoint)),
    target(this->peer.get_io_service()),
    tcp_resolver(this->peer.get_io_service()),
    udp_resolver(this->peer.get_io_service()),
    clientGreeting(),
    serverGreeting(),
    connectionRequest(),
    connectionResponse()
    {
        ++activeCount;
        logging::debug("Construct ControlBlock, %1%/%2% active.", activeCount, maxActive = std::max(activeCount, maxActive));
    }
    
    ~ControlBlock()
    {
        logging::debug("Deconstruct ControlBlock, %1%/%2% active.", --activeCount, maxActive);
    }
    
    TCPSocket peer;
    TCPEndpoint peer_endpoint;
    
    TCPSocket target;
    
    boost::asio::ip::tcp::resolver tcp_resolver;
    boost::asio::ip::udp::resolver udp_resolver;
    
    ClientGreeting clientGreeting;
    ServerGreeting serverGreeting;
    ConnectionRequest connectionRequest;
    std::vector<char> connectionResponse;
    
    ControlBlock(ControlBlock const&) = delete;
    ControlBlock& operator = (ControlBlock const&) = delete;
};

void forwardSingle(std::shared_ptr<TCPSocket> from, std::shared_ptr<TCPSocket> to, std::shared_ptr<std::vector<char>> buf)
{
    using boost::asio::buffer;
    using boost::asio::async_write;
    using boost::asio::socket_base;
    using boost::system::error_code;
    from->async_receive(buffer(*buf), error_branch([from, to, buf](error_code const&){
        error_code error;
        from->shutdown(socket_base::shutdown_receive, error);
        to->shutdown(socket_base::shutdown_send, error);
    }, [from, to, buf](std::size_t bytes){
        async_write(*to, buffer(*buf, bytes), error_branch([from, to, buf](error_code const&){
            error_code error;
            from->shutdown(socket_base::shutdown_receive, error);
            to->shutdown(socket_base::shutdown_send, error);
        }, [from, to, buf](std::size_t){
            forwardSingle(std::move(from), std::move(to), std::move(buf));
        }));
    }));
}

void forwardBoth(TCPSocket&& peer, TCPSocket&& target, std::size_t bufSize)
{
    std::shared_ptr<TCPSocket> ptrPeer(new TCPSocket(std::move(peer))), ptrTarget(new TCPSocket(std::move(target)));
    forwardSingle(ptrPeer, ptrTarget, std::shared_ptr<std::vector<char>>(new std::vector<char>(bufSize)));
    forwardSingle(ptrTarget, ptrPeer, std::shared_ptr<std::vector<char>>(new std::vector<char>(bufSize)));
}

template <typename T>
inline static typename std::enable_if<std::is_pod<T>::value && !std::is_pointer<T>::value, boost::asio::const_buffers_1>::type
makeBuffer(T const& obj)
{
    return boost::asio::buffer(static_cast<void const*>(&obj), sizeof(obj));
}

template <typename T>
inline static typename std::enable_if<std::is_pod<T>::value && !std::is_pointer<T>::value, boost::asio::mutable_buffers_1>::type
makeBuffer(T& obj)
{
    return boost::asio::buffer(static_cast<void*>(&obj), sizeof(obj));
}

template <typename Stream, typename F>
inline static void readString(Stream& s, SocksString& str, F const& continuation)
{
    using boost::asio::async_read;
    using boost::asio::buffer;
    async_read(s, makeBuffer(str.length), nosize("async_read", [continuation, &s, &str]{
        if(str.length != 0)
            async_read(s, buffer(str.contents, str.length), nosize("async_read", continuation));
        else
            continuation();
    }));
}

template <typename Stream, typename F>
inline static bool readAddress(Stream& s, AddressType type, AddressData &addressData, F const& continuation)
{
    using boost::asio::async_read;
    using boost::asio::buffer;
    switch(type)
    {
        case AddressType::IPv4:
            async_read(s, buffer(addressData.v4Addr), nosize("async_read", continuation));
            break;
        case AddressType::IPv6:
            async_read(s, buffer(addressData.v6Addr), nosize("async_read", continuation));
            break;
        case AddressType::HostName:
            readString(s, addressData.hostName, continuation);
            break;
        default:
            return false;
            break;
    }
    return true;
}

inline static std::string formatAddress(AddressType type, AddressData const& addressData)
{
    using boost::asio::ip::address_v4;
    using boost::asio::ip::address_v6;
    switch(type)
    {
        case AddressType::IPv4:
            return address_v4(addressData.v4Addr).to_string();
            break;
        case AddressType::IPv6:
            return address_v6(addressData.v6Addr).to_string();
            break;
        case AddressType::HostName:
        {
            char const* contents = reinterpret_cast<char const*>(addressData.hostName.contents);
            return std::string(contents, contents + addressData.hostName.length);
            break;
        }
        default:
            return std::string();
            break;
    }
}

static void sendConnError(ConnectionStatus status, std::shared_ptr<ControlBlock> cb)
{
    using boost::asio::buffer;
    using boost::asio::async_read;
    using boost::asio::async_write;
    makeConnectionResponse(status, cb->connectionResponse);
    async_write(cb->peer, buffer(cb->connectionResponse), nosize("async_write", [cb]{}));
}

static void serve_connect(std::shared_ptr<ControlBlock> cb)
{
    using boost::asio::buffer;
    using boost::asio::async_read;
    using boost::asio::async_write;
    using boost::asio::ip::tcp;
    
    using boost::system::error_code;
    
    auto const& filter = [cb](tcp::endpoint const& e){
        return checkTarget(e, cb->connectionRequest.header.command);
    };
    
    auto const& request = cb->connectionRequest;
    auto const& header = request.header;
    auto const& query = tcp::resolver::query(formatAddress(header.addressType, request.destAddress),
                                             boost::lexical_cast<std::string>(request.destPort.toHost())
    );
    logging::debug("%1%:%2%", query.host_name(), query.service_name());
    cb->tcp_resolver
    .async_resolve(query, error_branch([cb](error_code const& e){
        logging::debug("%1%", e.message());
        sendConnError(ConnectionStatus::HostUnreachable, std::move(cb));
    }, [cb, filter](tcp::resolver::iterator i){
        logging::debug("Resolving finished, trying to connect.");
        auto filtered = boost::make_filter_iterator(filter, i);
        auto end = boost::make_filter_iterator(filter, tcp::resolver::iterator());
        if(filtered == end)
            sendConnError(ConnectionStatus::BannedByRuleset, std::move(cb));
        else
        {
            boost::asio::async_connect(cb->target, filtered, end, error_branch([cb](error_code const& e){
                auto ecode = e.value();
                switch(ecode)
                {
                    case boost::asio::error::network_unreachable:
                        sendConnError(ConnectionStatus::NetworkUnreachable, std::move(cb));
                        break;
                    case boost::asio::error::host_unreachable:
                        sendConnError(ConnectionStatus::HostUnreachable, std::move(cb));
                        break;
                    case boost::asio::error::connection_refused:
                        sendConnError(ConnectionStatus::ConnectionRefused, std::move(cb));
                        break;
                    case boost::asio::error::timed_out:
                        sendConnError(ConnectionStatus::TtlExpired, std::move(cb));
                        break;
                    default:
                        sendConnError(ConnectionStatus::GeneralFailure, std::move(cb));
                        break;
                }
            }, [cb](decltype(filtered)){
                logging::debug("Connected.");
                makeConnectionResponse(ConnectionStatus::Granted, cb->connectionResponse, cb->target.local_endpoint());
                async_write(cb->peer, buffer(cb->connectionResponse), nosize("async_write", [cb]{
                    forwardBoth(std::move(cb->peer), std::move(cb->target), 64 * 1024);
                }));
            }));
        }
    }));
}

static void serve_bind(std::shared_ptr<ControlBlock> cb)
{
    sendConnError(ConnectionStatus::CommandNotSupported, std::move(cb));
}

static void serve_udpbind(std::shared_ptr<ControlBlock> cb)
{
    sendConnError(ConnectionStatus::CommandNotSupported, std::move(cb));
}

void handle_client(TCPSocket&& peer, TCPEndpoint&& peer_endpoint)
{
    using boost::asio::buffer;
    using boost::asio::async_read;
    using boost::asio::async_write;
    std::shared_ptr<ControlBlock> cb(new ControlBlock(std::move(peer), std::move(peer_endpoint)));
    logging::info("Connection from %1%:%2%", cb->peer_endpoint.address().to_string(), cb->peer_endpoint.port());
    async_read(cb->peer, makeBuffer(cb->clientGreeting.header), nosize("async_read", [cb]{
        auto& greeting = cb->clientGreeting;
        auto const& header = greeting.header;
        
        if(header.socksVer != 5 || header.numAuthMethods == 0)
            return;
        else
        {
            async_read(cb->peer, buffer(greeting.authMethods, header.numAuthMethods), nosize("async_read", [cb]{
                auto const& greeting = cb->clientGreeting;
                auto const& header = greeting.header;
                auto const& firstMethod = greeting.authMethods;
                auto const& lastMethod  = greeting.authMethods + header.numAuthMethods;
                
                cb->serverGreeting.socksVer = 5;
                if(std::find(firstMethod, lastMethod, uint8_t(AuthMethod::NoAuth)) == lastMethod)
                {
                    cb->serverGreeting.chosenAuthMethod = AuthMethod::NoSuitableMethod;
                    async_write(cb->peer, makeBuffer(cb->serverGreeting), nosize("async_write", [cb]{}));
                }
                else
                {
                    cb->serverGreeting.chosenAuthMethod = AuthMethod::NoAuth;
                    async_write(cb->peer, makeBuffer(cb->serverGreeting), nosize("async_write", [cb]{
                        async_read(cb->peer, makeBuffer(cb->connectionRequest.header), nosize("async_read", [cb]{
                            auto& request = cb->connectionRequest;
                            auto const& header = request.header;
                            bool addressValid = readAddress(cb->peer, header.addressType, request.destAddress, [cb]{
                                auto& request = cb->connectionRequest;
                                async_read(cb->peer, makeBuffer(request.destPort.repr), nosize("async_read", [cb]{
                                    if(!checkClient(cb->peer_endpoint, cb->serverGreeting.chosenAuthMethod))
                                        sendConnError(ConnectionStatus::BannedByRuleset, std::move(cb));
                                    else
                                    {
                                        auto const& request = cb->connectionRequest;
                                        auto const& header = request.header;
                                        switch(header.command)
                                        {
                                            case Command::TcpConnect:
                                                serve_connect(std::move(cb));
                                                break;
                                            case Command::TcpBind:
                                                serve_bind(std::move(cb));
                                                break;
                                            case Command::UdpBind:
                                                serve_udpbind(std::move(cb));
                                                break;
                                            default:
                                                sendConnError(ConnectionStatus::CommandNotSupported, std::move(cb));
                                                break;
                                        }
                                    }
                                }));
                            });
                            if(!addressValid)
                            {
                                sendConnError(ConnectionStatus::AddressTypeNotSupported, std::move(cb));
                            }
                        }));
                    }));
                }
            }));
        }
    }));
}
