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
        : SessionHandler(std::move(*socket_ptr), bc) {}

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

                if(object["method"] == "publish")
                {
                    // Gate publish during sync
                    if (sync_status && sync_status->isSyncing.load()) {
                        buffer.consume(buffer.size());
                        outputStream << syncInProgressMessage(object["id"]) << std::endl;
                        this->do_write();
                        return;
                    }

                    if (object["params"] == nullptr || object["params"].type() != nlohmann::json::value_t::object) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params") << std::endl;
                        this->do_write();
                        return;
                    }

                    // Validate stream param
                    if (!object["params"].contains("stream") || !object["params"]["stream"].is_string()
                        || object["params"]["stream"].get<std::string>().empty()) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: stream is required") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto stream = object["params"]["stream"].get<std::string>();

                    // Validate stream name format
                    if (!isValidStreamName(stream)) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: stream name invalid") << std::endl;
                        this->do_write();
                        return;
                    }

                    // Validate key param
                    if (!object["params"].contains("key") || !object["params"]["key"].is_string()
                        || object["params"]["key"].get<std::string>().empty()) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: key is required") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto key = object["params"]["key"].get<std::string>();

                    // Get data (optional, defaults to empty)
                    std::string data;
                    if (object["params"].contains("data") && object["params"]["data"].is_string()) {
                        data = object["params"]["data"].get<std::string>();
                    }

                    // Validate data size
                    static constexpr size_t kMaxDataSize = 128ULL * 1024 * 1024;
                    if (data.size() > kMaxDataSize) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: data exceeds 128 MB limit") << std::endl;
                        this->do_write();
                        return;
                    }

                    // Per-node stream permissions
                    if (!allowed_streams.empty()) {
                        if (std::find(allowed_streams.begin(), allowed_streams.end(), stream) == allowed_streams.end()) {
                            buffer.consume(buffer.size());
                            outputStream << errorMessage(object["id"], -32003, "Stream not permitted on this node") << std::endl;
                            this->do_write();
                            return;
                        }
                    }

                    // Get optional keys for index
                    std::vector<std::string> keys;
                    if (object["params"].contains("keys") && object["params"]["keys"].is_array()) {
                        keys = object["params"]["keys"].get<std::vector<std::string>>();
                    }

                    try {
                        Block b = bc.publish(stream, key, data, keys);
                        b.dump();
                        bc.saveChunk(b.index / bc.chunkSize);
                        bc.saveKeys();

                        // Broadcast the new block to all connected peers
                        if (peer_manager) {
                            peer_manager->broadcast_block(b);
                        }

                        buffer.consume(buffer.size());
                        outputStream << resultMessage(object["id"], b.toJson().dump()) << std::endl;
                    } catch (const std::runtime_error &e) {
                        buffer.consume(buffer.size());
                        outputStream << miningTimeoutMessage(object["id"], e.what()) << std::endl;
                    }
                    this->do_write();
                    return;
                }

                if(object["method"] == "createStream")
                {
                    if (object["params"] == nullptr || !object["params"].contains("name")
                        || !object["params"]["name"].is_string()
                        || object["params"]["name"].get<std::string>().empty()) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: name is required") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto name = object["params"]["name"].get<std::string>();
                    if (!isValidStreamName(name)) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: stream name invalid") << std::endl;
                        this->do_write();
                        return;
                    }
                    try {
                        bc.createStream(name);
                        buffer.consume(buffer.size());
                        outputStream << resultMessage(object["id"], "Stream '" + name + "' created") << std::endl;
                    } catch (const std::runtime_error &) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32004, "Stream already exists") << std::endl;
                    }
                    this->do_write();
                    return;
                }

                if(object["method"] == "listStreams")
                {
                    auto streams = bc.listStreams();
                    nlohmann::json arr = nlohmann::json::array();
                    for (const auto &s : streams) {
                        arr.push_back(s);
                    }
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], arr.dump()) << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "getStreamEntries")
                {
                    if (object["params"] == nullptr || !object["params"].contains("stream")
                        || !object["params"]["stream"].is_string()
                        || object["params"]["stream"].get<std::string>().empty()) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: stream is required") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto stream = object["params"]["stream"].get<std::string>();
                    std::string key;
                    if (object["params"].contains("key") && object["params"]["key"].is_string()) {
                        key = object["params"]["key"].get<std::string>();
                    }
                    auto entries = bc.getStreamEntries(stream, key);
                    nlohmann::json arr = nlohmann::json::array();
                    for (const auto &[blockIdx, entry] : entries) {
                        nlohmann::json ej;
                        ej["block_index"] = blockIdx;
                        ej["stream"] = entry.stream;
                        ej["key"] = entry.key;
                        ej["data"] = entry.data;
                        arr.push_back(ej);
                    }
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], arr.dump()) << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "getStreamEntry")
                {
                    if (object["params"] == nullptr
                        || !object["params"].contains("stream") || !object["params"]["stream"].is_string()
                        || object["params"]["stream"].get<std::string>().empty()
                        || !object["params"].contains("key") || !object["params"]["key"].is_string()
                        || object["params"]["key"].get<std::string>().empty()) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: stream and key are required") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto stream = object["params"]["stream"].get<std::string>();
                    auto key = object["params"]["key"].get<std::string>();
                    try {
                        auto [blockIdx, entry] = bc.getStreamEntry(stream, key);
                        nlohmann::json ej;
                        ej["block_index"] = blockIdx;
                        ej["stream"] = entry.stream;
                        ej["key"] = entry.key;
                        ej["data"] = entry.data;
                        buffer.consume(buffer.size());
                        outputStream << resultMessage(object["id"], ej.dump()) << std::endl;
                    } catch (const std::runtime_error &) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32601, "Entry not found") << std::endl;
                    }
                    this->do_write();
                    return;
                }

                if(object["method"] == "requestSync")
                {
                    if (sync_status && sync_status->isSyncing.load()) {
                        buffer.consume(buffer.size());
                        outputStream << syncAlreadyInProgressMessage(object["id"]) << std::endl;
                        this->do_write();
                        return;
                    }

                    if (!peer_client || !peer_client->is_connected()) {
                        buffer.consume(buffer.size());
                        outputStream << noPeerMessage(object["id"]) << std::endl;
                        this->do_write();
                        return;
                    }

                    peer_client->start_sync();
                    buffer.consume(buffer.size());
                    outputStream << syncStartedMessage(object["id"]) << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "getBlockByIndex")
                {
                    if(object["params"] == nullptr || object["params"].type() != nlohmann::json::value_t::object || object["params"]["index"] == nullptr)
                    {
                        buffer.consume(buffer.size());
                        outputStream << invalidParamsMessage(object["id"]) << std::endl;
                        this->do_write();
                        return;
                    }
                    auto index = object["params"]["index"].get<size_t>();
                    Block b = bc.getBlockByIndex(index);
                    b.dump();
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], b.toJson().dump()) << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "getBlocksByKeys")
                {
                    if(object["params"] == nullptr || object["params"]["keys"] == nullptr)
                    {
                        buffer.consume(buffer.size());
                        outputStream << invalidParamsMessage(object["id"]) << std::endl;
                        this->do_write();
                        return;
                    }
                    auto keys = object["params"]["keys"].get<std::vector<std::string>>();
                    std::vector<Block> blocks = bc.getBlocksByKeys(keys);
                    nlohmann::json response;

                    for(auto &b : blocks)
                    {
                        response.push_back(b.toJson());
                    }
                    
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], response.dump()) << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "addPeer")
                {
                    if (!peer_manager) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32603, "Peer manager not available") << std::endl;
                        this->do_write();
                        return;
                    }
                    if (object["params"] == nullptr || !object["params"].contains("host") || !object["params"].contains("port")) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: host and port are required") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto host = object["params"]["host"].get<std::string>();
                    auto port = object["params"]["port"].get<uint16_t>();

                    if (peer_manager->is_banned(host, port)) {
                        auto bans = peer_manager->get_bans();
                        for (const auto &ban : bans) {
                            if (ban.host == host && ban.port == port) {
                                buffer.consume(buffer.size());
                                outputStream << errorMessageWithData(object["id"], -32004, "Peer is currently banned", {{"expires", ban.expires}}) << std::endl;
                                this->do_write();
                                return;
                            }
                        }
                    }

                    if (peer_manager->outbound_count() >= peer_manager->get_config().max_outbound) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32003, "Outbound connection limit reached") << std::endl;
                        this->do_write();
                        return;
                    }

                    peer_manager->connect_to(host, port);
                    PeerEntry entry;
                    entry.host = host;
                    entry.port = port;
                    entry.last_seen = static_cast<uint64_t>(std::time(nullptr));
                    peer_manager->add_peer(entry);
                    peer_manager->save_peers();
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], "peer_added") << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "removePeer")
                {
                    if (!peer_manager) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32603, "Peer manager not available") << std::endl;
                        this->do_write();
                        return;
                    }
                    if (object["params"] == nullptr || !object["params"].contains("host") || !object["params"].contains("port")) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: host and port are required") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto host = object["params"]["host"].get<std::string>();
                    auto port = object["params"]["port"].get<uint16_t>();

                    if (!peer_manager->find_peer(host, port)) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32005, "Peer not found") << std::endl;
                        this->do_write();
                        return;
                    }

                    peer_manager->disconnect_and_remove(host, port);
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], "peer_removed") << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "listPeers")
                {
                    if (!peer_manager) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32603, "Peer manager not available") << std::endl;
                        this->do_write();
                        return;
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

                    buffer.consume(buffer.size());
                    outputStream << resultJsonMessage(object["id"], result) << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "banPeer")
                {
                    if (!peer_manager) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32603, "Peer manager not available") << std::endl;
                        this->do_write();
                        return;
                    }
                    if (object["params"] == nullptr || !object["params"].contains("host") || !object["params"].contains("port")) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: host and port are required") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto host = object["params"]["host"].get<std::string>();
                    auto port = object["params"]["port"].get<uint16_t>();
                    uint64_t duration = peer_manager->get_config().ban_duration_seconds;
                    if (object["params"].contains("duration_seconds")) {
                        duration = object["params"]["duration_seconds"].get<uint64_t>();
                    }

                    peer_manager->ban_peer(host, port, "manual", duration);
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], "peer_banned") << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "unbanPeer")
                {
                    if (!peer_manager) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32603, "Peer manager not available") << std::endl;
                        this->do_write();
                        return;
                    }
                    if (object["params"] == nullptr || !object["params"].contains("host") || !object["params"].contains("port")) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params: host and port are required") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto host = object["params"]["host"].get<std::string>();
                    auto port = object["params"]["port"].get<uint16_t>();

                    if (!peer_manager->is_banned(host, port)) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32006, "Peer is not banned") << std::endl;
                        this->do_write();
                        return;
                    }

                    peer_manager->unban_peer(host, port);
                    peer_manager->save_peers();
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], "peer_unbanned") << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "getInclusionProof")
                {
                    if (object["params"] == nullptr || object["params"].type() != nlohmann::json::value_t::object
                        || !object["params"].contains("blockIndex") || !object["params"]["blockIndex"].is_number_integer()
                        || !object["params"].contains("entryIndex") || !object["params"]["entryIndex"].is_number_integer()) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto blockIndex = object["params"]["blockIndex"].get<size_t>();
                    auto entryIndex = object["params"]["entryIndex"].get<size_t>();
                    try {
                        auto result = bc.getInclusionProof(blockIndex, entryIndex);
                        buffer.consume(buffer.size());
                        outputStream << resultJsonMessage(object["id"], result) << std::endl;
                    } catch (const std::out_of_range &e) {
                        std::string msg = e.what();
                        int code = -32001;
                        if (msg.find("Entry") != std::string::npos) {
                            code = -32002;
                            msg = "Entry not found";
                        } else {
                            msg = "Block not found";
                        }
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], code, msg) << std::endl;
                    }
                    this->do_write();
                    return;
                }

                if(object["method"] == "verifyInclusionProof")
                {
                    if (object["params"] == nullptr || object["params"].type() != nlohmann::json::value_t::object
                        || !object["params"].contains("blockIndex") || !object["params"]["blockIndex"].is_number_integer()
                        || !object["params"].contains("leafHash") || !object["params"]["leafHash"].is_string()
                        || !object["params"].contains("proof") || !object["params"]["proof"].is_array()) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params") << std::endl;
                        this->do_write();
                        return;
                    }
                    // Validate proof array elements
                    for (const auto &elem : object["params"]["proof"]) {
                        if (!elem.contains("hash") || !elem["hash"].is_string()
                            || !elem.contains("isLeft") || !elem["isLeft"].is_boolean()) {
                            buffer.consume(buffer.size());
                            outputStream << errorMessage(object["id"], -32602, "Invalid params") << std::endl;
                            this->do_write();
                            return;
                        }
                    }
                    auto blockIndex = object["params"]["blockIndex"].get<size_t>();
                    auto leafHash = object["params"]["leafHash"].get<std::string>();
                    auto proofArray = object["params"]["proof"];
                    try {
                        auto result = bc.verifyInclusionProof(blockIndex, leafHash, proofArray);
                        buffer.consume(buffer.size());
                        outputStream << resultJsonMessage(object["id"], result) << std::endl;
                    } catch (const std::out_of_range &) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32001, "Block not found") << std::endl;
                    }
                    this->do_write();
                    return;
                }

                if(object["method"] == "getBlockHeader")
                {
                    if (object["params"] == nullptr || object["params"].type() != nlohmann::json::value_t::object
                        || !object["params"].contains("blockIndex") || !object["params"]["blockIndex"].is_number_integer()) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto blockIndex = object["params"]["blockIndex"].get<size_t>();
                    try {
                        Block b = bc.getBlockByIndex(blockIndex);
                        buffer.consume(buffer.size());
                        outputStream << resultJsonMessage(object["id"], b.toHeaderJson()) << std::endl;
                    } catch (const std::out_of_range &) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32001, "Block not found") << std::endl;
                    }
                    this->do_write();
                    return;
                }
                
                if(object["method"] == "getNodeStatus")
                {
                    nlohmann::json result;
                    result["chainLength"] = bc.getChainLength();
                    result["chunkCount"] = bc.getChunkCount();
                    result["syncState"] = (sync_status && sync_status->isSyncing.load()) ? "syncing" : "idle";
                    result["currentDifficulty"] = bc.getCurrentDifficulty();
                    result["inboundPeers"] = peer_manager ? peer_manager->inbound_count() : static_cast<size_t>(0);
                    result["outboundPeers"] = peer_manager ? peer_manager->outbound_count() : static_cast<size_t>(0);
                    result["nodeUuid"] = peer_manager ? peer_manager->get_node_uuid() : "";
                    buffer.consume(buffer.size());
                    outputStream << resultJsonMessage(object["id"], result) << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "getBlockRange")
                {
                    if (object["params"] == nullptr || object["params"].type() != nlohmann::json::value_t::object
                        || !object["params"].contains("startIndex") || !object["params"]["startIndex"].is_number_integer()
                        || !object["params"].contains("endIndex") || !object["params"]["endIndex"].is_number_integer()) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid params") << std::endl;
                        this->do_write();
                        return;
                    }
                    auto startIndex = object["params"]["startIndex"].get<size_t>();
                    auto endIndex = object["params"]["endIndex"].get<size_t>();
                    bool headersOnly = false;
                    if (object["params"].contains("headersOnly") && object["params"]["headersOnly"].is_boolean()) {
                        headersOnly = object["params"]["headersOnly"].get<bool>();
                    }

                    if (startIndex > endIndex) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Invalid range: startIndex exceeds endIndex") << std::endl;
                        this->do_write();
                        return;
                    }

                    static constexpr size_t kMaxBlockRange = 1000;
                    if (endIndex - startIndex + 1 > kMaxBlockRange) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32602, "Range too large: maximum 1000 blocks per request") << std::endl;
                        this->do_write();
                        return;
                    }

                    size_t chainLength = bc.getChainLength();
                    if (startIndex >= chainLength) {
                        buffer.consume(buffer.size());
                        outputStream << errorMessage(object["id"], -32001, "Start index out of range") << std::endl;
                        this->do_write();
                        return;
                    }

                    if (endIndex >= chainLength) {
                        endIndex = chainLength - 1;
                    }

                    nlohmann::json blocks = nlohmann::json::array();
                    for (size_t i = startIndex; i <= endIndex; i++) {
                        Block b = bc.getBlockByIndex(i);
                        blocks.push_back(headersOnly ? b.toHeaderJson() : b.toJson());
                    }
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], blocks.dump()) << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "getChainLength")
                {
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], std::to_string(bc.getChainLength())) << std::endl;
                    this->do_write();
                    return;
                }

                if(object["method"] == "getChunkCount")
                {
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], std::to_string(bc.getChunkCount())) << std::endl;
                    this->do_write();
                    return;
                }

                buffer.consume(buffer.size());
                outputStream << invalidMethodMessage(object["id"], object["method"]) << std::endl;
                this->do_write();
            }
        });
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