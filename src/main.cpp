#include <iostream>
#include "block.hpp"
#include "blockchain.hpp"
#include "server.hpp"
#include "client_session.hpp"
#include "node_session.hpp"
#include <chrono>

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path to blockchain directory>" << std::endl;
        return 1;
    }

    boost::asio::io_context io_context;

    unsigned short port = 12345;
    std::string cert_file = "../ssl-cert-snakeoil.pem";
    std::string key_file = "../ssl-cert-snakeoil.key";

    blockchain bc(argv[1]);
    bc.loadChunk(0);
    bc.loadKeys();

/*
    auto blocks = bc.getBlocksByKeys({ "foo"  });
    for(auto &block : blocks) block.dump();
*/

    bc.dumpBlocks();

    server<client_session> rpc_server(io_context, port, cert_file, key_file, bc);
    rpc_server.start_accept();

    server<node_session> node_server(io_context, port + 1, cert_file, key_file, bc);
    node_server.start_accept();
    io_context.run();
  
    return 0;
}
