#include "node_session.hpp"
#include "block.hpp"
#include "json.hpp"

node_session::node_session(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, blockchain &bc)
        : session_handler(std::move(*socket_ptr), bc) {}

std::shared_ptr<session_handler> node_session::create(boost::asio::io_context &io_context, ssl::context &ssl_context, blockchain &bc)
{
    std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream = std::make_shared<ssl::stream<tcp::socket>>(tcp::socket(io_context), ssl_context);
    return std::make_shared<node_session>(std::move(ssl_stream), bc);
}

ssl::stream<tcp::socket> &node_session::get_socket_ref()
{
    return ssl_socket;
};

void node_session::start()
{
    auto self(shared_from_this());
    ssl_socket.async_handshake(ssl::stream_base::server,
        [this, self](const boost::system::error_code& error) {
            if (!error) {
                this->do_read();
            } else {
                // Handle handshake error, logging, cleaning up, etc.
            }
        }
    );
}

void node_session::do_read() 
{
    auto self(shared_from_this());
    boost::asio::async_read_until(this->ssl_socket, this->buffer, '\n',
        [this, self](const boost::system::error_code& ec, std::size_t) {
            if (!ec) {
                std::istream stream(&buffer);
                std::ostream outputStream(&buffer);
                buffer.consume(buffer.size());
                outputStream << "blockchain node server" << std::endl;
                this->do_write();
            }
        });
}

void node_session::do_write()
{
    auto self(shared_from_this());
    boost::asio::async_write(ssl_socket, buffer,
        [this, self](const boost::system::error_code& ec, std::size_t) {
            if (!ec) {
                do_read();
            }
        });
}