#include "chunk.hpp"

void chunk::save()
{
    std::stringstream ss;
    ss << "/chunk_" << std::setfill('0') << std::setw(6) << this->index << ".dat";
    std::filesystem::path path = this->blockchainPath.string() + ss.str();

    std::cout << "Saving " << this->blocks.size() << " blocks to chunk " << this->index << " in " << path << std::endl;
    
    std::ofstream ofs(path, std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);
    oa << *this;

    if(!ofs.good())
    {
        throw new std::runtime_error("Error: Could not write to file " + ss.str());
    }
}

void chunk::load()
{
    std::stringstream ss;
    ss << "/chunk_" << std::setfill('0') << std::setw(6) << this->index << ".dat";

    std::filesystem::path path = this->blockchainPath.string() + ss.str();

    std::ifstream ifs(path, std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> *this;

    std::cout << "Loaded " << this->blocks.size() << " blocks from chunk " << this->index << " in " << path << std::endl;
}