#ifndef _3E571332_9DF2_11E3_A450_206A8A22A96A
#define _3E571332_9DF2_11E3_A450_206A8A22A96A

#include <boost/asio/ip/tcp.hpp>

typedef boost::asio::ip::tcp::socket TCPSocket;
typedef boost::asio::ip::tcp::endpoint TCPEndpoint;

void handle_client(TCPSocket&& peer, TCPEndpoint&& peer_endpoint);

#endif
