#include "Chunk.hpp"
#include "utils.hpp"

void Chunk::save()
{
    auto fname = chunkFilename(this->index);
    std::filesystem::path finalPath = this->blockchainPath / fname;
    std::filesystem::path tmpPath = this->blockchainPath / (fname + ".tmp");

    std::cout << "Saving " << this->blocks.size() << " blocks to chunk " << this->index << " in " << finalPath << std::endl;
    
    {
        std::ofstream ofs(tmpPath, std::ios::binary);
        if(!ofs.good())
        {
            throw std::runtime_error("Error: Could not write to file " + tmpPath.string());
        }
        boost::archive::binary_oarchive oa(ofs);
        oa << *this;

        if(!ofs.good())
        {
            throw std::runtime_error("Error: Could not write to file " + tmpPath.string());
        }
    }

    std::filesystem::rename(tmpPath, finalPath);
    this->clearDirty();
}

void Chunk::load()
{
    std::filesystem::path path = this->blockchainPath / chunkFilename(this->index);

    if (!std::filesystem::exists(path)) {
        return;
    }

    std::ifstream ifs(path, std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> *this;

    this->clearDirty();

    std::cout << "Loaded " << this->blocks.size() << " blocks from chunk " << this->index << " in " << path << std::endl;
}