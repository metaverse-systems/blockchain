#include <iostream>
#include "block.hpp"
#include "blockchain.hpp"
#include "server.hpp"
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

    server s(io_context, port, cert_file, key_file, bc);
    s.start_accept();
    io_context.run();
  
    return 0;
}
