#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include "blockchain.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class session : public std::enable_shared_from_this<session>
{
  private:
    ssl::stream<tcp::socket> ssl_socket;
    boost::asio::streambuf buffer;
    blockchain &bc;

  public:
    explicit session(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, blockchain &bc);
    void start();

  private:
    void do_read();
    void do_write();
};