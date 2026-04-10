#include "RpcServer.hpp"
#include "PeerClient.hpp"
#include "../Block.hpp"
#include "../Chunk.hpp"
#include "../json.hpp"
#include "../PeerManager.hpp"
#include <stdexcept>

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

                if(object["method"] == "addBlock")
                {
                    // Gate addBlock during sync
                    if (sync_status && sync_status->isSyncing.load()) {
                        buffer.consume(buffer.size());
                        outputStream << syncInProgressMessage(object["id"]) << std::endl;
                        this->do_write();
                        return;
                    }

                    try {
                        Block b = bc.addBlock(object["params"]["data"], object["params"]["keys"]);
                        b.dump();
                        bc.saveChunk(b.index / bc.chunkSize);
                        bc.saveKeys();
                        buffer.consume(buffer.size());
                        outputStream << resultMessage(object["id"], b.toJson().dump()) << std::endl;
                    } catch (const std::runtime_error &e) {
                        buffer.consume(buffer.size());
                        outputStream << miningTimeoutMessage(object["id"], e.what()) << std::endl;
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
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32600;
    response["error"]["message"] = "Invalid JSON-RPC message";
    response["id"] = nullptr;
    return response;
}

nlohmann::json RpcServer::noIdMessage()
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32600;
    response["error"]["message"] = "JSON-RPC requests must include an 'id'";
    response["id"] = nullptr;
    return response;
}

nlohmann::json RpcServer::invalidMethodMessage(std::string id, std::string method)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32601;
    response["error"]["message"] = "Invalid method: " + method;
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::invalidParamsMessage(std::string id)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32602;
    response["error"]["message"] = "Invalid parameters";
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::resultMessage(std::string id, std::string result)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["result"] = result;
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::miningTimeoutMessage(std::string id, std::string detail)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32000;
    response["error"]["message"] = detail;
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::syncInProgressMessage(std::string id)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32001;
    response["error"]["message"] = "Node is syncing";
    response["error"]["data"] = "addBlock is unavailable while chain synchronization is in progress";
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::syncStartedMessage(std::string id)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["result"] = "sync_started";
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::noPeerMessage(std::string id)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32003;
    response["error"]["message"] = "No peer connected";
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::syncAlreadyInProgressMessage(std::string id)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32002;
    response["error"]["message"] = "Sync already in progress";
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::resultJsonMessage(std::string id, nlohmann::json result)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["result"] = result;
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::errorMessage(std::string id, int code, std::string message)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = code;
    response["error"]["message"] = message;
    response["id"] = id;
    return response;
}

nlohmann::json RpcServer::errorMessageWithData(std::string id, int code, std::string message, nlohmann::json data)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = code;
    response["error"]["message"] = message;
    response["error"]["data"] = data;
    response["id"] = id;
    return response;
}