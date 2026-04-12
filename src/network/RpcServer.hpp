#pragma once

#include "SessionHandler.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include "../IBlockchain.hpp"
#include "../Chunk.hpp"
#include "../json.hpp"
#include "../StreamEntry.hpp"
#include "../SyncState.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class PeerClient;
class PeerManager;

class RpcServer : public SessionHandler, public std::enable_shared_from_this<RpcServer>
{
  private:
    boost::asio::streambuf buffer;
    SyncStatus *sync_status = nullptr;
    PeerClient *peer_client = nullptr;
    PeerManager *peer_manager = nullptr;
    std::vector<std::string> allowed_streams;

  protected:
    std::shared_ptr<SessionHandler> shared_self() override { return shared_from_this(); }
    void on_handshake_complete() override;

  public:
    explicit RpcServer(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, IBlockchain &bc);
    static std::shared_ptr<RpcServer> create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc);
    ssl::stream<tcp::socket> &get_socket_ref();
    void set_sync_status(SyncStatus *status) { sync_status = status; }
    void set_peer_client(PeerClient *client) { peer_client = client; }
    void set_peer_manager(PeerManager *pm) { peer_manager = pm; }
    void set_allowed_streams(const std::vector<std::string> &streams) { allowed_streams = streams; }

  private:
    void do_read();
    void do_write();

    static nlohmann::json invalidJsonRpcMessage();
    static nlohmann::json noIdMessage();
    static nlohmann::json invalidMethodMessage(nlohmann::json id, std::string method);
    static nlohmann::json invalidParamsMessage(nlohmann::json id);
    static nlohmann::json resultMessage(nlohmann::json id, std::string result);
    static nlohmann::json resultJsonMessage(nlohmann::json id, nlohmann::json result);
    static nlohmann::json miningTimeoutMessage(nlohmann::json id, std::string detail);
    static nlohmann::json syncInProgressMessage(nlohmann::json id);
    static nlohmann::json syncStartedMessage(nlohmann::json id);
    static nlohmann::json noPeerMessage(nlohmann::json id);
    static nlohmann::json syncAlreadyInProgressMessage(nlohmann::json id);
    static nlohmann::json errorMessage(nlohmann::json id, int code, std::string message);
    static nlohmann::json errorMessageWithData(nlohmann::json id, int code, std::string message, nlohmann::json data);
};