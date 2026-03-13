#ifndef KS_WEBSOCKET_H
#define KS_WEBSOCKET_H

#include "ksTypes.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>

using tcp = boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

class WebsocketClient {
public:
    WebsocketClient(const std::string& url, const std::string& path, const Headers& headers);
    ~WebsocketClient();

    void connect();

    void disconnect();

    ksApiMessage readMessageFromWebSocket();

    void sendMessageToWebSocket(const std::string& message);
    
    void startAsyncRead(std::function<void(const ksApiMessage&)> onMessage);
    
    void run();
    
private:
    void asyncReadMessage(std::function<void(const ksApiMessage&)> onMessage);
    std::string uri_;
    std::string path_;

    std::string ping_ = "0x9";
    std::string pong_ = "0xA";

    net::io_context ioc_;
    tcp::resolver resolver_{ioc_};
    ssl::context ssl_ctx_{ssl::context::tlsv12};
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_ssl_{ioc_, ssl_ctx_};
    beast::flat_buffer buffer_;

    Headers headers_;

    MessageHandler messageHandler_;

};

#endif // KS_WEBSOCKET_H