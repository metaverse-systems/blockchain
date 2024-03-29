#pragma once

#include "SessionHandler.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include "../IBlockchain.hpp"
#include "../Chunk.hpp"
#include "../json.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class RpcServer : public SessionHandler, public std::enable_shared_from_this<RpcServer>
{
  private:
    boost::asio::streambuf buffer;

  public:
    explicit RpcServer(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, IBlockchain &bc);
    void start() override;
    static std::shared_ptr<RpcServer> create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc);
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