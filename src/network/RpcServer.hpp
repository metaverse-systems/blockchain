#pragma once

#include "SessionHandler.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include "../IBlockchain.hpp"
#include "../IChainReader.hpp"
#include "../IChainWriter.hpp"
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
    friend class RpcHandlerTests;

  public:
    using RpcHandler = std::function<nlohmann::json(const nlohmann::json &)>;

  private:
    boost::asio::streambuf buffer;
    IChainReader &reader_;
    IChainWriter &writer_;
    SyncStatus *sync_status = nullptr;
    PeerClient *peer_client = nullptr;
    PeerManager *peer_manager = nullptr;
    std::vector<std::string> allowed_streams;
    std::unordered_map<std::string, RpcHandler> dispatch_;

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
    void init_dispatch();

    nlohmann::json handle_publish(const nlohmann::json &request);
    nlohmann::json handle_createStream(const nlohmann::json &request);
    nlohmann::json handle_listStreams(const nlohmann::json &request);
    nlohmann::json handle_getStreamEntries(const nlohmann::json &request);
    nlohmann::json handle_getStreamEntry(const nlohmann::json &request);
    nlohmann::json handle_requestSync(const nlohmann::json &request);
    nlohmann::json handle_getBlockByIndex(const nlohmann::json &request);
    nlohmann::json handle_getBlocksByKeys(const nlohmann::json &request);
    nlohmann::json handle_addPeer(const nlohmann::json &request);
    nlohmann::json handle_removePeer(const nlohmann::json &request);
    nlohmann::json handle_listPeers(const nlohmann::json &request);
    nlohmann::json handle_banPeer(const nlohmann::json &request);
    nlohmann::json handle_unbanPeer(const nlohmann::json &request);
    nlohmann::json handle_getInclusionProof(const nlohmann::json &request);
    nlohmann::json handle_verifyInclusionProof(const nlohmann::json &request);
    nlohmann::json handle_getBlockHeader(const nlohmann::json &request);
    nlohmann::json handle_getNodeStatus(const nlohmann::json &request);
    nlohmann::json handle_getBlockRange(const nlohmann::json &request);
    nlohmann::json handle_getChainLength(const nlohmann::json &request);
    nlohmann::json handle_getChunkCount(const nlohmann::json &request);

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