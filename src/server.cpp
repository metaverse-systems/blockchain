#include "server.hpp"
#include "client_session.hpp"

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
    auto socket = std::make_shared<ssl::stream<tcp::socket>>(io_context, ssl_context);
    
    acceptor_.async_accept(socket->lowest_layer(),
    [this, socket](const boost::system::error_code &error)
    {
        if (!error) {
            socket->async_handshake(ssl::stream_base::server,
            [this, socket](const boost::system::error_code &error)
            {
                if (!error) {
                    std::make_shared<client_session>(socket, this->bc)->start();
                } else {
                    std::cerr << "Handshake failed: " << error.message() << std::endl;
                }
            });
        } else {
            std::cerr << "Accept failed: " << error.message() << std::endl;
        }

        this->start_accept();
    });
}