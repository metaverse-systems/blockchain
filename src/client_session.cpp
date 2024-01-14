#include "client_session.hpp"
#include "block.hpp"
#include "json.hpp"

client_session::client_session(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, blockchain &bc)
        : session_handler(std::move(*socket_ptr), bc) {}

std::shared_ptr<session_handler> client_session::create(boost::asio::io_context &io_context, ssl::context &ssl_context, blockchain &bc)
{
    std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream = std::make_shared<ssl::stream<tcp::socket>>(tcp::socket(io_context), ssl_context);
    return std::make_shared<client_session>(std::move(ssl_stream), bc);
}

ssl::stream<tcp::socket> &client_session::get_socket_ref()
{
    return ssl_socket;
};

void client_session::start()
{
    auto self(shared_from_this());
    ssl_socket.async_handshake(ssl::stream_base::server,
        [this, self](const boost::system::error_code& error) {
            if (!error) {
                this->do_read();
            } else {
                // Handle handshake error, logging, cleaning up, etc.
            }
        }
    );
}

void client_session::do_read() 
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
                    block b = bc.addBlock(object["params"]["data"], object["params"]["keys"]);
                    b.dump();
                    bc.saveChunk(b.index / bc.chunkSize);
                    bc.saveKeys();
                    buffer.consume(buffer.size());
                    outputStream << resultMessage(object["id"], b.toJson()) << std::endl;
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
                    block b = bc.getBlockByIndex(index);
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
                    std::vector<block> blocks = bc.getBlocksByKeys(keys);
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
                
                buffer.consume(buffer.size());
                outputStream << invalidMethodMessage(object["id"], object["method"]) << std::endl;
                this->do_write();
            }
        });
}

void client_session::do_write()
{
    auto self(shared_from_this());
    boost::asio::async_write(ssl_socket, buffer,
        [this, self](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                do_read();
            }
        });
}

nlohmann::json client_session::invalidJsonRpcMessage()
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32600;
    response["error"]["message"] = "Invalid JSON-RPC message";
    response["id"] = nullptr;
    return response;
}

nlohmann::json client_session::noIdMessage()
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32600;
    response["error"]["message"] = "JSON-RPC requests must include an 'id'";
    response["id"] = nullptr;
    return response;
}

nlohmann::json client_session::invalidMethodMessage(std::string id, std::string method)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32601;
    response["error"]["message"] = "Invalid method: " + method;
    response["id"] = id;
    return response;
}

nlohmann::json client_session::invalidParamsMessage(std::string id)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = -32602;
    response["error"]["message"] = "Invalid parameters";
    response["id"] = id;
    return response;
}

nlohmann::json client_session::resultMessage(std::string id, std::string result)
{
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["result"] = result;
    response["id"] = id;
    return response;
}