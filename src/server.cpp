#include "server.hpp"
#include "session.hpp"

server::server(boost::asio::io_context &io_context, unsigned short port, std::string cert_chain_file, std::string private_key_file, blockchain &bc)
    : io_context(io_context),
      port(port),
      ssl_context(ssl::context::sslv23),
      acceptor_(io_context),
      cert_chain_file(cert_chain_file),
      private_key_file(private_key_file),
      bc(bc)
{
    ssl_context.set_options(
        ssl::context::default_workarounds |
        ssl::context::no_sslv2 |
        ssl::context::single_dh_use);

    ssl_context.use_certificate_chain_file(this->cert_chain_file);
    ssl_context.use_private_key_file(this->private_key_file, ssl::context::pem);

    // Set up the acceptor to listen on IPv6 and IPv4 in dual-stack mode
    tcp::resolver resolver(this->io_context);
    tcp::endpoint endpoint = *resolver.resolve({tcp::v6(), std::to_string(this->port)}).begin();
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.set_option(boost::asio::ip::v6_only(false)); // Set to true to disable dual-stack (IPv4 & IPv6)
    acceptor_.bind(endpoint);
    acceptor_.listen();
}

void server::start_accept()
{
    // Create a new socket for the next connection as a shared pointer
    auto socket = std::make_shared<ssl::stream<tcp::socket>>(io_context, ssl_context);
    
    // Accept the next connection.
    acceptor_.async_accept(socket->lowest_layer(),
    [this, socket](const boost::system::error_code &error)  // capture socket as shared_ptr by value
    {
        if (!error) {
            // Handle the connection with `socket`.
            std::cout << "Connection accepted" << std::endl;
            socket->async_handshake(ssl::stream_base::server,
            [this, socket](const boost::system::error_code &error)  // capture socket as shared_ptr by value
            {
                if (!error) {
                    std::cout << "Handshake successful" << std::endl;

                    // You may need to adapt the `session` constructor to accept
                    // a `std::shared_ptr<ssl::stream<tcp::socket>>` instead of
                    // a moved `ssl::stream<tcp::socket>` depending on its implementation.
                    std::make_shared<session>(socket, this->bc)->start();
                } else {
                    std::cerr << "Handshake failed: " << error.message() << std::endl;
                }
            });
        } else {
            std::cerr << "Accept failed: " << error.message() << std::endl;
        }

        // Call `start_accept()` to accept the next connection.
        this->start_accept();
    });
}