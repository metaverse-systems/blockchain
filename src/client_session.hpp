#include "session_handler.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include "blockchain.hpp"
#include "json.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class client_session : public std::enable_shared_from_this<client_session>
{
  private:
    ssl::stream<tcp::socket> ssl_socket;
    boost::asio::streambuf buffer;
    blockchain &bc;

  public:
    explicit client_session(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, blockchain &bc);
    void start();

  private:
    void do_read();
    void do_write();

    static nlohmann::json invalidJsonRpcMessage();
    static nlohmann::json noIdMessage();
    static nlohmann::json invalidMethodMessage(std::string id, std::string method);
    static nlohmann::json invalidParamsMessage(std::string id);
    static nlohmann::json resultMessage(std::string id, std::string result);
};