#include <iostream>
#include <boost/asio.hpp>
#include <chrono>
#include "Block.hpp"
#include "blockchain.hpp"
#include "network/Server.hpp"
#include "network/RpcServer.hpp"
#include "network/PeerServer.hpp"
#include "network/MockSessionHandler.hpp"

using boost::asio::ip::tcp;

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path to blockchain directory>" << std::endl;
        return 1;
    }

    blockchain<Chunk> bc(argv[1]);
    bc.loadChunk(0);
    bc.loadKeys();
    bc.dumpBlocks();

    unsigned short port = 12345;
    std::string cert_file = "../ssl-cert-snakeoil.pem";
    std::string key_file = "../ssl-cert-snakeoil.key";

    boost::asio::io_context io_context;
    boost::asio::ssl::context ssl_context(ssl::context::sslv23);
    
    ssl_context.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::single_dh_use);
    ssl_context.use_certificate_chain_file(cert_file);
    ssl_context.use_private_key_file(key_file, ssl::context::pem);

    tcp::acceptor rpc_acceptor(io_context);
    tcp::resolver resolver(io_context);
    tcp::endpoint endpoint = *resolver.resolve({tcp::v6(), std::to_string(port)}).begin();
    rpc_acceptor.open(endpoint.protocol());
    rpc_acceptor.set_option(tcp::acceptor::reuse_address(true));
    rpc_acceptor.set_option(boost::asio::ip::v6_only(false));
    rpc_acceptor.bind(endpoint);
    rpc_acceptor.listen();

    Server<RpcServer, tcp::acceptor> rpc(io_context, ssl_context, rpc_acceptor, bc);
    rpc.start_accept();

    tcp::acceptor p2p_acceptor(io_context);
    endpoint = *resolver.resolve({tcp::v6(), std::to_string(port + 1)}).begin();
    p2p_acceptor.open(endpoint.protocol());
    p2p_acceptor.set_option(tcp::acceptor::reuse_address(true));
    p2p_acceptor.set_option(boost::asio::ip::v6_only(false));
    p2p_acceptor.bind(endpoint);
    p2p_acceptor.listen();

    Server<MockSessionHandler, tcp::acceptor> node_server(io_context, ssl_context, p2p_acceptor, bc);
    node_server.start_accept();

    io_context.run();
  
    return 0;
}
