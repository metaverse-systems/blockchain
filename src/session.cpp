#include "session.hpp"
#include "block.hpp"
#include "json.hpp"

session::session(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, blockchain &bc)
        : ssl_socket(std::move(*socket_ptr)), bc(bc) {}

void session::start()
{
    this->do_read();
}

void session::do_read() 
{
    auto self(shared_from_this());
    boost::asio::async_read_until(this->ssl_socket, this->buffer, '\n',
        [this, self](const boost::system::error_code& ec, std::size_t) {
            if (!ec) {
                std::istream stream(&buffer);
                std::ostream outputStream(&buffer);
                std::string received_msg;
                std::getline(stream, received_msg);
                received_msg.push_back('\n');

                nlohmann::json object = nlohmann::json::parse(received_msg);
                if(object["jsonrpc"].get<std::string>() != "2.0")
                {
                    std::cout << "Received invalid JSON-RPC message" << std::endl;
                    std::cout << "Received message: " << object << std::endl;
                    buffer.consume(buffer.size());  // Clear the buffer
                    outputStream << "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32600, \"message\": \"Invalid JSON-RPC message\"}, \"id\": null}" << std::endl;
                    do_write();
                    return;
                }

                if(object["id"] == nullptr)
                {
                    std::cout << "JSON-RPC requests must include an 'id'" << std::endl;
                    std::cout << "Received message: " << object << std::endl;
                    buffer.consume(buffer.size());  // Clear the buffer
                    outputStream << "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32600, \"message\": \"JSON-RPC requests must include an 'id'\"}, \"id\": null}" << std::endl;
                    do_write();
                    return;
                }

                if(object["method"] == "addBlock")
                {
                    std::cout << "Received addBlock command" << std::endl;
                    std::cout << "Data: " << object["data"] << std::endl;
                    std::cout << "Keys: " << object["keys"] << std::endl;
                    std::cout << "Received message: " << object << std::endl;
                    block b = bc.addBlock(object["data"], object["keys"]);
                    b.dump();
                    bc.saveChunk(b.index / bc.chunkSize);
                    bc.saveKeys();
                    buffer.consume(buffer.size());  // Clear the buffer
                    nlohmann::json response;
                    response["jsonrpc"] = "2.0";
                    response["result"] = b.hash;
                    response["id"] = object["id"];
                    outputStream << response << std::endl;
                    do_write();
                    return;
                }
                
                buffer.consume(buffer.size());  // Clear the buffer
                do_write();
            }
        });
}

void session::do_write()
{
    auto self(shared_from_this());
    boost::asio::async_write(ssl_socket, buffer,
        [this, self](const boost::system::error_code& ec, std::size_t) {
            if (!ec) {
                do_read();  // Wait for the next message from the client
            }
        });
}
