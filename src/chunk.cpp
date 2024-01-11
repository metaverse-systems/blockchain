#include "chunk.hpp"

void chunk::save()
{
    //std::ofstream ofs("chunk" + std::to_string(this->index) + ".dat");
    //boost::archive::binary_oarchive oa(ofs);
    //oa << *this;
    std::cout << "Saving " << this->blocks.size() << " blocks to chunk " << this->index << std::endl;

    std::stringstream ss;
    ss << this->blockchainPath.string() << "/chunk_" << std::setfill('0') << std::setw(6) << this->index << ".dat";

    std::cout << "Writing to file " << ss.str() << std::endl;
    
    std::ofstream ofs(ss.str(), std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);
    oa << *this;

    if(!ofs.good())
    {
        throw new std::runtime_error("Error: Could not write to file " + ss.str());
    }
}

void chunk::load()
{
    //std::ifstream ifs("chunk" + std::to_string(this->index) + ".dat");
    //boost::archive::binary_iarchive ia(ifs);
    //ia >> *this;
    std::stringstream ss;
    ss << "/chunk_" << std::setfill('0') << std::setw(6) << this->index << ".dat";

    std::filesystem::path path = this->blockchainPath.string() + ss.str();

    std::cout << "Loading chunk " << this->index << " from file " << path << std::endl;
    
    std::ifstream ifs(path, std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> *this;

    std::cout << "Loaded " << this->blocks.size() << " blocks from chunk " << this->index << std::endl;
}

void chunk::dump()
{
    std::cout << "Chunk " << this->index << " has " << this->blocks.size() << " blocks" << std::endl;
    for(auto &block : this->blocks)
    {
        block.dump();
    }
}

bool chunk::isBlockPresent(size_t index)
{
    return (this->blocks.size() > 0) && (this->blocks.at(index % this->blocks.size()).index == index);
}
