#include "RpcServer.hpp"
#include "PeerClient.hpp"
#include "../Block.hpp"
#include "../Chunk.hpp"
#include "../json.hpp"
#include "../StreamEntry.hpp"
#include "../PeerManager.hpp"
#include "../MerkleTree.hpp"
#include <stdexcept>
#include <algorithm>
#include <regex>

RpcServer::RpcServer(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, IBlockchain &bc)
        : SessionHandler(std::move(*socket_ptr), bc) { init_dispatch(); }

std::shared_ptr<RpcServer> RpcServer::create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc)
{
    std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream = std::make_shared<ssl::stream<tcp::socket>>(tcp::socket(io_context), ssl_context);
    return std::make_shared<RpcServer>(std::move(ssl_stream), bc);
}

ssl::stream<tcp::socket> &RpcServer::get_socket_ref()
{
    return ssl_socket;
};

void RpcServer::on_handshake_complete()
{
    this->do_read();
}

void RpcServer::do_read() 
{
    auto self(shared_from_this());
    boost::asio::async_read_until(this->ssl_socket, this->buffer, '\n',
        [this, self](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                std::istream stream(&buffer);
                std::ostream outputStream(&buffer);
                std::string received_msg;
                std::getline(stream, received_msg);
                received_msg.push_back('\n');

                nlohmann::json object;
                try
                {
                    object = nlohmann::json::parse(received_msg);
                }
                catch(const nlohmann::detail::parse_error &e)
                {
                    buffer.consume(buffer.size());
                    outputStream << invalidJsonRpcMessage() << std::endl;
                    this->do_write();
                    return;
                }
                
                 
                if(object["jsonrpc"] == nullptr || object["jsonrpc"].get<std::string>() != "2.0")
                {
                    buffer.consume(buffer.size());
                    outputStream << invalidJsonRpcMessage() << std::endl;
                    this->do_write();
                    return;
                }

                if(object["id"] == nullptr)
                {
                    buffer.consume(buffer.size());
                    outputStream << noIdMessage() << std::endl;
                    this->do_write();
                    return;
                }

                std::string method = object["method"].get<std::string>();
                auto it = dispatch_.find(method);
                nlohmann::json response;
                if (it != dispatch_.end()) {
                    response = it->second(object);
                } else {
                    response = invalidMethodMessage(object["id"], method);
                }
                buffer.consume(buffer.size());
                outputStream << response << std::endl;
                this->do_write();
            }
        });
}

void RpcServer::init_dispatch()
{
    dispatch_["publish"] = [this](const nlohmann::json &req) { return handle_publish(req); };
    dispatch_["createStream"] = [this](const nlohmann::json &req) { return handle_createStream(req); };
    dispatch_["listStreams"] = [this](const nlohmann::json &req) { return handle_listStreams(req); };
    dispatch_["getStreamEntries"] = [this](const nlohmann::json &req) { return handle_getStreamEntries(req); };
    dispatch_["getStreamEntry"] = [this](const nlohmann::json &req) { return handle_getStreamEntry(req); };
    dispatch_["requestSync"] = [this](const nlohmann::json &req) { return handle_requestSync(req); };
    dispatch_["getBlockByIndex"] = [this](const nlohmann::json &req) { return handle_getBlockByIndex(req); };
    dispatch_["getBlocksByKeys"] = [this](const nlohmann::json &req) { return handle_getBlocksByKeys(req); };
    dispatch_["addPeer"] = [this](const nlohmann::json &req) { return handle_addPeer(req); };
    dispatch_["removePeer"] = [this](const nlohmann::json &req) { return handle_removePeer(req); };
    dispatch_["listPeers"] = [this](const nlohmann::json &req) { return handle_listPeers(req); };
    dispatch_["banPeer"] = [this](const nlohmann::json &req) { return handle_banPeer(req); };
    dispatch_["unbanPeer"] = [this](const nlohmann::json &req) { return handle_unbanPeer(req); };
    dispatch_["getInclusionProof"] = [this](const nlohmann::json &req) { return handle_getInclusionProof(req); };
    dispatch_["verifyInclusionProof"] = [this](const nlohmann::json &req) { return handle_verifyInclusionProof(req); };
    dispatch_["getBlockHeader"] = [this](const nlohmann::json &req) { return handle_getBlockHeader(req); };
    dispatch_["getNodeStatus"] = [this](const nlohmann::json &req) { return handle_getNodeStatus(req); };
    dispatch_["getBlockRange"] = [this](const nlohmann::json &req) { return handle_getBlockRange(req); };
    dispatch_["getChainLength"] = [this](const nlohmann::json &req) { return handle_getChainLength(req); };
    dispatch_["getChunkCount"] = [this](const nlohmann::json &req) { return handle_getChunkCount(req); };
}

nlohmann::json RpcServer::handle_publish(const nlohmann::json &request)
{
    if (sync_status && sync_status->isSyncing.load()) {
        return syncInProgressMessage(request["id"]);
    }

    if (request["params"] == nullptr || request["params"].type() != nlohmann::json::value_t::object) {
        return errorMessage(request["id"], -32602, "Invalid params");
    }

    if (!request["params"].contains("stream") || !request["params"]["stream"].is_string()
        || request["params"]["stream"].get<std::string>().empty()) {
        return errorMessage(request["id"], -32602, "Invalid params: stream is required");
    }
    auto stream_name = request["params"]["stream"].get<std::string>();

    if (!isValidStreamName(stream_name)) {
        return errorMessage(request["id"], -32602, "Invalid params: stream name invalid");
    }

    if (!request["params"].contains("key") || !request["params"]["key"].is_string()
        || request["params"]["key"].get<std::string>().empty()) {
        return errorMessage(request["id"], -32602, "Invalid params: key is required");
    }
    auto key = request["params"]["key"].get<std::string>();

    std::string data;
    if (request["params"].contains("data") && request["params"]["data"].is_string()) {
        data = request["params"]["data"].get<std::string>();
    }

    static constexpr size_t kMaxDataSize = 128ULL * 1024 * 1024;
    if (data.size() > kMaxDataSize) {
        return errorMessage(request["id"], -32602, "Invalid params: data exceeds 128 MB limit");
    }

    if (!allowed_streams.empty()) {
        if (std::find(allowed_streams.begin(), allowed_streams.end(), stream_name) == allowed_streams.end()) {
            return errorMessage(request["id"], -32003, "Stream not permitted on this node");
        }
    }

    std::vector<std::string> keys;
    if (request["params"].contains("keys") && request["params"]["keys"].is_array()) {
        keys = request["params"]["keys"].get<std::vector<std::string>>();
    }

    try {
        Block b = bc.publish(stream_name, key, data, keys);
        b.dump();
        bc.saveChunk(b.index / bc.chunkSize);
        bc.saveKeys();

        if (peer_manager) {
            peer_manager->broadcast_block(b);
        }

        return resultMessage(request["id"], b.toJson().dump());
    } catch (const std::runtime_error &e) {
        return miningTimeoutMessage(request["id"], e.what());
    }
}

nlohmann::json RpcServer::handle_createStream(const nlohmann::json &request)
{
    if (request["params"] == nullptr || !request["params"].contains("name")
        || !request["params"]["name"].is_string()
        || request["params"]["name"].get<std::string>().empty()) {
        return errorMessage(request["id"], -32602, "Invalid params: name is required");
    }
    auto name = request["params"]["name"].get<std::string>();
    if (!isValidStreamName(name)) {
        return errorMessage(request["id"], -32602, "Invalid params: stream name invalid");
    }
    try {
        bc.createStream(name);
        return resultMessage(request["id"], "Stream '" + name + "' created");
    } catch (const std::runtime_error &) {
        return errorMessage(request["id"], -32004, "Stream already exists");
    }
}

nlohmann::json RpcServer::handle_listStreams(const nlohmann::json &request)
{
    auto streams = bc.listStreams();
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &s : streams) {
        arr.push_back(s);
    }
    return resultMessage(request["id"], arr.dump());
}

nlohmann::json RpcServer::handle_getStreamEntries(const nlohmann::json &request)
{
    if (request["params"] == nullptr || !request["params"].contains("stream")
        || !request["params"]["stream"].is_string()
        || request["params"]["stream"].get<std::string>().empty()) {
        return errorMessage(request["id"], -32602, "Invalid params: stream is required");
    }
    auto stream_name = request["params"]["stream"].get<std::string>();
    std::string key;
    if (request["params"].contains("key") && request["params"]["key"].is_string()) {
        key = request["params"]["key"].get<std::string>();
    }
    auto entries = bc.getStreamEntries(stream_name, key);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &[blockIdx, entry] : entries) {
        nlohmann::json ej;
        ej["block_index"] = blockIdx;
        ej["stream"] = entry.stream;
        ej["key"] = entry.key;
        ej["data"] = entry.data;
        arr.push_back(ej);
    }
    return resultMessage(request["id"], arr.dump());
}

nlohmann::json RpcServer::handle_getStreamEntry(const nlohmann::json &request)
{
    if (request["params"] == nullptr
        || !request["params"].contains("stream") || !request["params"]["stream"].is_string()
        || request["params"]["stream"].get<std::string>().empty()
        || !request["params"].contains("key") || !request["params"]["key"].is_string()
        || request["params"]["key"].get<std::string>().empty()) {
        return errorMessage(request["id"], -32602, "Invalid params: stream and key are required");
    }
    auto stream_name = request["params"]["stream"].get<std::string>();
    auto key = request["params"]["key"].get<std::string>();
    try {
        auto [blockIdx, entry] = bc.getStreamEntry(stream_name, key);
        nlohmann::json ej;
        ej["block_index"] = blockIdx;
        ej["stream"] = entry.stream;
        ej["key"] = entry.key;
        ej["data"] = entry.data;
        return resultMessage(request["id"], ej.dump());
    } catch (const std::runtime_error &) {
        return errorMessage(request["id"], -32601, "Entry not found");
    }
}

nlohmann::json RpcServer::handle_requestSync(const nlohmann::json &request)
{
    if (sync_status && sync_status->isSyncing.load()) {
        return syncAlreadyInProgressMessage(request["id"]);
    }

    if (!peer_client || !peer_client->is_connected()) {
        return noPeerMessage(request["id"]);
    }

    peer_client->start_sync();
    return syncStartedMessage(request["id"]);
}

nlohmann::json RpcServer::handle_getBlockByIndex(const nlohmann::json &request)
{
    if (request["params"] == nullptr || request["params"].type() != nlohmann::json::value_t::object || request["params"]["index"] == nullptr) {
        return invalidParamsMessage(request["id"]);
    }
    auto index = request["params"]["index"].get<size_t>();
    if (index >= bc.getChainLength()) {
        return errorMessage(request["id"], -32001, "Block not found");
    }
    Block b = bc.getBlockByIndex(index);
    b.dump();
    return resultMessage(request["id"], b.toJson().dump());
}

nlohmann::json RpcServer::handle_getBlocksByKeys(const nlohmann::json &request)
{
    if (request["params"] == nullptr || request["params"]["keys"] == nullptr) {
        return invalidParamsMessage(request["id"]);
    }
    auto keys = request["params"]["keys"].get<std::vector<std::string>>();
    std::vector<Block> blocks = bc.getBlocksByKeys(keys);
    nlohmann::json result;

    for (auto &b : blocks) {
        result.push_back(b.toJson());
    }

    return resultMessage(request["id"], result.dump());
}

nlohmann::json RpcServer::handle_addPeer(const nlohmann::json &request)
{
    if (!peer_manager) {
        return errorMessage(request["id"], -32603, "Peer manager not available");
    }
    if (request["params"] == nullptr || !request["params"].contains("host") || !request["params"].contains("port")) {
        return errorMessage(request["id"], -32602, "Invalid params: host and port are required");
    }
    auto host = request["params"]["host"].get<std::string>();
    auto port = request["params"]["port"].get<uint16_t>();

    if (peer_manager->is_banned(host, port)) {
        auto bans = peer_manager->get_bans();
        for (const auto &ban : bans) {
            if (ban.host == host && ban.port == port) {
                return errorMessageWithData(request["id"], -32004, "Peer is currently banned", {{"expires", ban.expires}});
            }
        }
    }

    if (peer_manager->outbound_count() >= peer_manager->get_config().max_outbound) {
        return errorMessage(request["id"], -32003, "Outbound connection limit reached");
    }

    peer_manager->connect_to(host, port);
    PeerEntry entry;
    entry.host = host;
    entry.port = port;
    entry.last_seen = static_cast<uint64_t>(std::time(nullptr));
    peer_manager->add_peer(entry);
    peer_manager->save_peers();
    return resultMessage(request["id"], "peer_added");
}

nlohmann::json RpcServer::handle_removePeer(const nlohmann::json &request)
{
    if (!peer_manager) {
        return errorMessage(request["id"], -32603, "Peer manager not available");
    }
    if (request["params"] == nullptr || !request["params"].contains("host") || !request["params"].contains("port")) {
        return errorMessage(request["id"], -32602, "Invalid params: host and port are required");
    }
    auto host = request["params"]["host"].get<std::string>();
    auto port = request["params"]["port"].get<uint16_t>();

    if (!peer_manager->find_peer(host, port)) {
        return errorMessage(request["id"], -32005, "Peer not found");
    }

    peer_manager->disconnect_and_remove(host, port);
    return resultMessage(request["id"], "peer_removed");
}

nlohmann::json RpcServer::handle_listPeers(const nlohmann::json &request)
{
    if (!peer_manager) {
        return errorMessage(request["id"], -32603, "Peer manager not available");
    }

    nlohmann::json result;
    result["node_uuid"] = peer_manager->get_node_uuid();
    result["discovery_enabled"] = peer_manager->is_discovery_enabled();
    result["outbound_count"] = peer_manager->outbound_count();
    result["inbound_count"] = peer_manager->inbound_count();
    result["max_outbound"] = peer_manager->get_config().max_outbound;
    result["max_inbound"] = peer_manager->get_config().max_inbound;

    nlohmann::json peers_json = nlohmann::json::array();
    for (const auto &p : peer_manager->get_peers()) {
        nlohmann::json pj;
        pj["host"] = p.host;
        pj["port"] = p.port;
        pj["node_uuid"] = p.node_uuid;
        pj["last_seen"] = p.last_seen;
        pj["error_count"] = p.error_count;
        peers_json.push_back(pj);
    }
    result["peers"] = peers_json;

    nlohmann::json bans_json = nlohmann::json::array();
    for (const auto &b : peer_manager->get_bans()) {
        nlohmann::json bj;
        bj["host"] = b.host;
        bj["port"] = b.port;
        bj["reason"] = b.reason;
        bj["expires"] = b.expires;
        bans_json.push_back(bj);
    }
    result["bans"] = bans_json;

    return resultJsonMessage(request["id"], result);
}

nlohmann::json RpcServer::handle_banPeer(const nlohmann::json &request)
{
    if (!peer_manager) {
        return errorMessage(request["id"], -32603, "Peer manager not available");
    }
    if (request["params"] == nullptr || !request["params"].contains("host") || !request["params"].contains("port")) {
        return errorMessage(request["id"], -32602, "Invalid params: host and port are required");
    }
    auto host = request["params"]["host"].get<std::string>();
    auto port = request["params"]["port"].get<uint16_t>();
    uint64_t duration = peer_manager->get_config().ban_duration_seconds;
    if (request["params"].contains("duration_seconds")) {
        duration = request["params"]["duration_seconds"].get<uint64_t>();
    }

    peer_manager->ban_peer(host, port, "manual", duration);
    return resultMessage(request["id"], "peer_banned");
}

nlohmann::json RpcServer::handle_unbanPeer(const nlohmann::json &request)
{
    if (!peer_manager) {
        return errorMessage(request["id"], -32603, "Peer manager not available");
    }
    if (request["params"] == nullptr || !request["params"].contains("host") || !request["params"].contains("port")) {
        return errorMessage(request["id"], -32602, "Invalid params: host and port are required");
    }
    auto host = request["params"]["host"].get<std::string>();
    auto port = request["params"]["port"].get<uint16_t>();

    if (!peer_manager->is_banned(host, port)) {
        return errorMessage(request["id"], -32006, "Peer is not banned");
    }

    peer_manager->unban_peer(host, port);
    peer_manager->save_peers();
    return resultMessage(request["id"], "peer_unbanned");
}

nlohmann::json RpcServer::handle_getInclusionProof(const nlohmann::json &request)
{
    if (request["params"] == nullptr || request["params"].type() != nlohmann::json::value_t::object
        || !request["params"].contains("blockIndex") || !request["params"]["blockIndex"].is_number_integer()
        || !request["params"].contains("entryIndex") || !request["params"]["entryIndex"].is_number_integer()) {
        return errorMessage(request["id"], -32602, "Invalid params");
    }
    auto blockIndex = request["params"]["blockIndex"].get<size_t>();
    auto entryIndex = request["params"]["entryIndex"].get<size_t>();
    try {
        auto result = bc.getInclusionProof(blockIndex, entryIndex);
        return resultJsonMessage(request["id"], result);
    } catch (const std::out_of_range &e) {
        std::string msg = e.what();
        int code = -32001;
        if (msg.find("Entry") != std::string::npos) {
            code = -32002;
            msg = "Entry not found";
        } else {
            msg = "Block not found";
        }
        return errorMessage(request["id"], code, msg);
    }
}

nlohmann::json RpcServer::handle_verifyInclusionProof(const nlohmann::json &request)
{
    if (request["params"] == nullptr || request["params"].type() != nlohmann::json::value_t::object
        || !request["params"].contains("blockIndex") || !request["params"]["blockIndex"].is_number_integer()
        || !request["params"].contains("leafHash") || !request["params"]["leafHash"].is_string()
        || !request["params"].contains("proof") || !request["params"]["proof"].is_array()) {
        return errorMessage(request["id"], -32602, "Invalid params");
    }
    for (const auto &elem : request["params"]["proof"]) {
        if (!elem.contains("hash") || !elem["hash"].is_string()
            || !elem.contains("isLeft") || !elem["isLeft"].is_boolean()) {
            return errorMessage(request["id"], -32602, "Invalid params");
        }
    }
    auto blockIndex = request["params"]["blockIndex"].get<size_t>();
    auto leafHash = request["params"]["leafHash"].get<std::string>();
    auto proofArray = request["params"]["proof"];
    try {
        auto result = bc.verifyInclusionProof(blockIndex, leafHash, proofArray);
        return resultJsonMessage(request["id"], result);
    } catch (const std::out_of_range &) {
        return errorMessage(request["id"], -32001, "Block not found");
    }
}

nlohmann::json RpcServer::handle_getBlockHeader(const nlohmann::json &request)
{
    if (request["params"] == nullptr || request["params"].type() != nlohmann::json::value_t::object
        || !request["params"].contains("blockIndex") || !request["params"]["blockIndex"].is_number_integer()) {
        return errorMessage(request["id"], -32602, "Invalid params");
    }
    auto blockIndex = request["params"]["blockIndex"].get<size_t>();
    try {
        Block b = bc.getBlockByIndex(blockIndex);
        return resultJsonMessage(request["id"], b.toHeaderJson());
    } catch (const std::out_of_range &) {
        return errorMessage(request["id"], -32001, "Block not found");
    }
}

nlohmann::json RpcServer::handle_getNodeStatus(const nlohmann::json &request)
{
    nlohmann::json result;
    result["chainLength"] = bc.getChainLength();
    result["chunkCount"] = bc.getChunkCount();
    result["syncState"] = (sync_status && sync_status->isSyncing.load()) ? "syncing" : "idle";
    result["currentDifficulty"] = bc.getCurrentDifficulty();
    result["inboundPeers"] = peer_manager ? peer_manager->inbound_count() : static_cast<size_t>(0);
    result["outboundPeers"] = peer_manager ? peer_manager->outbound_count() : static_cast<size_t>(0);
    result["nodeUuid"] = peer_manager ? peer_manager->get_node_uuid() : "";
    return resultJsonMessage(request["id"], result);
}

nlohmann::json RpcServer::handle_getBlockRange(const nlohmann::json &request)
{
    if (request["params"] == nullptr || request["params"].type() != nlohmann::json::value_t::object
        || !request["params"].contains("startIndex") || !request["params"]["startIndex"].is_number_integer()
        || !request["params"].contains("endIndex") || !request["params"]["endIndex"].is_number_integer()) {
        return errorMessage(request["id"], -32602, "Invalid params");
    }
    auto startIndex = request["params"]["startIndex"].get<size_t>();
    auto endIndex = request["params"]["endIndex"].get<size_t>();
    bool headersOnly = false;
    if (request["params"].contains("headersOnly") && request["params"]["headersOnly"].is_boolean()) {
        headersOnly = request["params"]["headersOnly"].get<bool>();
    }

    if (startIndex > endIndex) {
        return errorMessage(request["id"], -32602, "Invalid range: startIndex exceeds endIndex");
    }

    static constexpr size_t kMaxBlockRange = 1000;
    if (endIndex - startIndex + 1 > kMaxBlockRange) {
        return errorMessage(request["id"], -32602, "Range too large: maximum 1000 blocks per request");
    }

    size_t chainLength = bc.getChainLength();
    if (startIndex >= chainLength) {
        return errorMessage(request["id"], -32001, "Start index out of range");
    }

    if (endIndex >= chainLength) {
        endIndex = chainLength - 1;
    }

    nlohmann::json blocks = nlohmann::json::array();
    for (size_t i = startIndex; i <= endIndex; i++) {
        Block b = bc.getBlockByIndex(i);
        blocks.push_back(headersOnly ? b.toHeaderJson() : b.toJson());
    }
    return resultMessage(request["id"], blocks.dump());
}

nlohmann::json RpcServer::handle_getChainLength(const nlohmann::json &request)
{
    return resultMessage(request["id"], std::to_string(bc.getChainLength()));
}

nlohmann::json RpcServer::handle_getChunkCount(const nlohmann::json &request)
{
    return resultMessage(request["id"], std::to_string(bc.getChunkCount()));
}

void RpcServer::do_write()
{
    auto self(shared_from_this());
    boost::asio::async_write(ssl_socket, buffer,
        [this, self](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                do_read();
            }
        });
}

nlohmann::json RpcServer::invalidJsonRpcMessage()
{
    return errorMessageWithData(nullptr, -32600, "Invalid JSON-RPC message", nullptr);
}

nlohmann::json RpcServer::noIdMessage()
{
    return errorMessageWithData(nullptr, -32600, "JSON-RPC requests must include an 'id'", nullptr);
}

nlohmann::json RpcServer::invalidMethodMessage(nlohmann::json id, std::string method)
{
    return errorMessage(id, -32601, "Invalid method: " + method);
}

nlohmann::json RpcServer::invalidParamsMessage(nlohmann::json id)
{
    return errorMessage(id, -32602, "Invalid parameters");
}

nlohmann::json RpcServer::resultMessage(nlohmann::json id, std::string result)
{
    return resultJsonMessage(id, result);
}

nlohmann::json RpcServer::miningTimeoutMessage(nlohmann::json id, std::string detail)
{
    return errorMessage(id, -32000, detail);
}

nlohmann::json RpcServer::syncInProgressMessage(nlohmann::json id)
{
    return errorMessageWithData(id, -32001, "Node is syncing",
                                 "publish is unavailable while chain synchronization is in progress");
}

nlohmann::json RpcServer::syncStartedMessage(nlohmann::json id)
{
    return resultJsonMessage(id, "sync_started");
}

nlohmann::json RpcServer::noPeerMessage(nlohmann::json id)
{
    return errorMessage(id, -32003, "No peer connected");
}

nlohmann::json RpcServer::syncAlreadyInProgressMessage(nlohmann::json id)
{
    return errorMessage(id, -32002, "Sync already in progress");
}

nlohmann::json RpcServer::resultJsonMessage(nlohmann::json id, nlohmann::json result)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["result"] = result;
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::errorMessage(nlohmann::json id, int code, std::string message)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = code;
    response["error"]["message"] = message;
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::errorMessageWithData(nlohmann::json id, int code, std::string message, nlohmann::json data)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = code;
    response["error"]["message"] = message;
    response["error"]["data"] = data;
    response["id"] = id;
    return response;
}