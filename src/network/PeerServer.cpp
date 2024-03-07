#include "PeerServer.hpp"
#include "../Block.hpp"

PeerServer::PeerServer(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, IBlockchain &bc)
        : session_handler(std::move(*socket_ptr), bc) {}

std::shared_ptr<PeerServer> PeerServer::create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc)
{
    std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream = std::make_shared<ssl::stream<tcp::socket>>(tcp::socket(io_context), ssl_context);
    return std::make_shared<PeerServer>(std::move(ssl_stream), bc);
}

ssl::stream<tcp::socket> &PeerServer::get_socket_ref()
{
    return ssl_socket;
};

void PeerServer::start()
{
    auto self(shared_from_this());
    ssl_socket.async_handshake(ssl::stream_base::server,
        [this, self](const boost::system::error_code &error) {
            if (!error) {
                this->do_read_header();
            } else {
                // Handle handshake error, logging, cleaning up, etc.
            }
        }
    );
}

void PeerServer::do_read_header()
{
    auto self(shared_from_this());
    boost::asio::async_read(this->ssl_socket, this->buffer,
        boost::asio::transfer_exactly(sizeof(packet_header)),
        [this, self](const boost::system::error_code& ec, std::size_t /*length*/) {
            if (!ec) {
                std::istream is(&this->buffer);
                packet_header header;
                is.read(reinterpret_cast<char*>(&header), sizeof(packet_header));

                this->do_read_body(header);
            } else {
                std::cerr << "Read header operation failed: " << ec.message() << std::endl;
            }
        });
}

void PeerServer::do_read_body(const packet_header &header) 
{
    auto self(shared_from_this());
    boost::asio::async_read(this->ssl_socket, this->buffer, boost::asio::transfer_exactly(header.length),
        [this, self, header](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                std::istream stream(&buffer);
                std::string serialized_str(header.length, 0);
                stream.read(&serialized_str[0], header.length);

                std::istringstream iss(serialized_str);
                switch(header.type)
                {
                    case packet_type::BLOCK:
                    {
                        boost::archive::binary_iarchive ia(iss);
                        Block b;
                        ia >> b;
                        std::cout << "Received block: " << std::endl;
                        b.dump();
                        break;
                    }
                    default:
                        std::cerr << "Received unknown packet type: " << header.type << std::endl;
                        break;
                }

                std::ostream outputStream(&buffer);
                buffer.consume(buffer.size());
                outputStream << "blockchain node server" << std::endl;
                this->do_write();
            }
        });
}

void PeerServer::do_write()
{
    auto self(shared_from_this());
    boost::asio::async_write(ssl_socket, buffer,
        [this, self](const boost::system::error_code& ec, std::size_t) {
            if (!ec) {
                do_read_header();
            }
        });
}