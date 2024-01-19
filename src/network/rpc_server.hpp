#pragma once

#include "session_handler.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include "../blockchain.hpp"
#include "../json.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class rpc_server : public session_handler, public std::enable_shared_from_this<rpc_server>
{
  private:
    boost::asio::streambuf buffer;

  public:
    explicit rpc_server(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, blockchain &bc);
    void start() override;
    static std::shared_ptr<session_handler> create(boost::asio::io_context &io_context, ssl::context &ssl_context, blockchain &bc);
    ssl::stream<tcp::socket> &get_socket_ref();

  private:
    void do_read();
    void do_write();

    static nlohmann::json invalidJsonRpcMessage();
    static nlohmann::json noIdMessage();
    static nlohmann::json invalidMethodMessage(std::string id, std::string method);
    static nlohmann::json invalidParamsMessage(std::string id);
    static nlohmann::json resultMessage(std::string id, std::string result);
};