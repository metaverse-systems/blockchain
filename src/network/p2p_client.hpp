#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <string>

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class p2p_client
{
  public:
    p2p_client(boost::asio::io_context &io_context,
                      ssl::context &ssl_context,
                      const std::string &host,
                      unsigned short port)
        : resolver(io_context),
          socket(io_context, ssl_context),
          host(host),
          port(std::to_string(port)) {}
    ssl::stream<tcp::socket> &get_socket_ref();
    void connect();
    template<typename T>
    void send(const T &obj, uint64_t packet_type);

  private:
    tcp::resolver resolver;
    ssl::stream<tcp::socket> socket;
    std::string host;
    std::string port;
    boost::asio::streambuf write_buffer;
};