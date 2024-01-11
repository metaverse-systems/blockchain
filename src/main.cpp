#include <iostream>
#include "block.hpp"
#include "blockchain.hpp"
#include <chrono>

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path to blockchain directory>" << std::endl;
        return 1;
    }

    blockchain bc(argv[1]);
    
 //   bc.addBlock("{ data: 'foo' }", { "foo" });
//    bc.saveChunk(0);
//    bc.saveKeys();

//    bc.dumpBlocks();
//    bc.dumpKeys();

//    bc.loadChunk(0);
//    bc.loadKeys();
    auto block = bc.getBlockByIndex(1);
    std::cout << block.hash << std::endl;

    block.dump();

    bc.dumpBlocks();

    return 0;
}
