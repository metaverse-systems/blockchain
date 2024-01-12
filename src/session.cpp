#include "session.hpp"

session::session(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr)
        : ssl_socket(std::move(*socket_ptr)) {}

void session::start()
{
    this->do_read();
}

void session::do_read() 
{
    auto self(shared_from_this());
    boost::asio::async_read_until(ssl_socket, buffer, '\n',
        [this, self](const boost::system::error_code& ec, std::size_t) {
            if (!ec) {
                std::cout << "Received message: ";
                std::cout.write(
                    boost::asio::buffer_cast<const char*>(buffer.data()),
                    buffer.size());
                buffer.consume(buffer.size());  // Clear the buffer
                do_write();
            }
        });
}

void session::do_write()
{
    auto self(shared_from_this());
    boost::asio::async_write(ssl_socket, buffer,
        [this, self](const boost::system::error_code& ec, std::size_t) {
            if (!ec) {
                do_read();  // Wait for the next message from the client
            }
        });
}
