#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include "../blockchain.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

template<typename SessionHandler>
class server
{
  private:
    boost::asio::io_context &io_context;
    unsigned short port;
    ssl::context ssl_context;
    tcp::acceptor acceptor_;
    std::string cert_chain_file;
    std::string private_key_file;
    blockchain &bc;

  public:
    server(boost::asio::io_context &io_context, unsigned short port, std::string cert_chain_file, std::string private_key_file, blockchain &bc)
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
        acceptor_.set_option(boost::asio::ip::v6_only(false));
        acceptor_.bind(endpoint);
        acceptor_.listen();
    }

    void start_accept()
    {
        auto new_session = SessionHandler::create(io_context, ssl_context, bc);

        acceptor_.async_accept(new_session->get_socket_ref().lowest_layer(),
        [this, new_session](const boost::system::error_code& error)
        {
            if (!error) {
                new_session->start();
            } else {
                std::cerr << "Accept failed: " << error.message() << std::endl;
            }
            this->start_accept();
        });
    }
};