#include "blockchain.hpp"
#include <boost/asio.hpp>

class session_handler {
public:
    virtual ~session_handler() = default;
    virtual void start() = 0;
    static std::shared_ptr<session_handler> create(boost::asio::io_context &io_context, blockchain &bc);
};