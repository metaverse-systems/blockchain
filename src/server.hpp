#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include "blockchain.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

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
    server(boost::asio::io_context &io_context, unsigned short port, std::string cert_chain_file, std::string private_key_file, blockchain &bc);
    void start_accept();
};