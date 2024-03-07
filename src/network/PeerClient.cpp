#include "PeerClient.hpp"
#include "packet_header.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

ssl::stream<tcp::socket> &PeerClient::get_socket_ref()
{
    return socket;
}

void PeerClient::connect()
{
    auto endpoints = this->resolver.resolve(this->host, this->port);
    boost::asio::async_connect(this->socket.lowest_layer(), endpoints,
        [this](const boost::system::error_code &ec, tcp::endpoint)
        {
            if (!ec)
            {
                this->socket.async_handshake(ssl::stream_base::client,
                    [this](const boost::system::error_code &ec)
                    {
                        if (!ec)
                        {
                            // Successfully connected and handshaked.
                            // Now you can start asynchronous read/write.
                        }
                    });
            }
        });
}

template<typename T>
void PeerClient::send(const T &obj, uint64_t packet_type)
{
    std::stringstream ss;
    boost::archive::binary_oarchive oa(ss);
    oa << obj;
    std::string serialized_str = ss.str();

    packet_header header(serialized_str.size(), packet_type);
    std::vector<char> header_data(sizeof(header));
    std::memcpy(header_data.data(), &header, sizeof(header));

    std::ostream stream(&this->write_buffer);
    stream.write(header_data.data(), header_data.size());
    stream << serialized_str;

    boost::asio::async_write(this->socket, this->write_buffer,
        [this](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                std::cout << "Data sent successfully!" << std::endl;
            } else {
                std::cerr << "Error sending data: " << ec.message() << std::endl;
            }
        });
}