#include <iostream>
#include <utility>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_service.hpp>

#include "error_handler.h"
#include "handle_client.h"

class Acceptor
{
public:
    Acceptor(boost::asio::io_service &io_service, TCPEndpoint const& endpoint):
    acceptor(io_service, endpoint),
    peer(io_service),
    peer_endpoint()
    {}
    
    void exec()
    {
        acceptor.async_accept(peer, peer_endpoint, error_handler("async_accept", [this]{
            handle_client(std::move(peer), std::move(peer_endpoint));
            exec();
        }));
    }
    
private:
    Acceptor(Acceptor const&) = delete;
    Acceptor& operator = (Acceptor const&) = delete;
    
    boost::asio::ip::tcp::acceptor acceptor;
    TCPSocket peer;
    TCPEndpoint peer_endpoint;
};

int main(int argc, char **argv) {
    if(argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " ip addr" << std::endl;
        return 1;
    }
    boost::asio::io_service io_service;
    typedef boost::asio::ip::tcp::resolver resolver_type;
    resolver_type resolver(io_service);
    resolver_type::query query(argv[1], argv[2]);
    auto addr = resolver.resolve(query);
    Acceptor acceptor(io_service, addr->endpoint());
    acceptor.exec();
    io_service.run();
}
