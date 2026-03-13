#include "ksWebsocket.h"
#include <iostream>

WebsocketClient::WebsocketClient(const std::string& uri, const std::string& path, const Headers& headers) : uri_(uri), path_(path), headers_(headers) {
    // Constructor implementation (e.g., parse URI)
}

WebsocketClient::~WebsocketClient() {
    // Destructor implementation
    disconnect();
}

void WebsocketClient::connect() {

    std::cout << "Starting connection to " << uri_ << path_ << std::endl;

    // Parse URI to extract hostname
    std::string host;
    size_t scheme_end = uri_.find("://");
    if (scheme_end != std::string::npos) {
        size_t host_start = scheme_end + 3;
        size_t path_start = uri_.find("/", host_start);
        host = uri_.substr(host_start, (path_start != std::string::npos ? path_start : uri_.length()) - host_start);
    } else {
        host = uri_;
    }
    
    std::string port = uri_.find("wss") != std::string::npos ? "443" : "80";
    
    auto const results = resolver_.resolve(host, port);
    beast::error_code ec;

    // Connect underlying TCP layer
    auto ep = net::connect(beast::get_lowest_layer(ws_ssl_).socket(), results, ec);
    if (ec) {
        throw beast::system_error{ec};
    }
    std::cout << "Connected to " << ep.address() << ":" << ep.port() << std::endl;

    // Set WebSocket decorator (headers) before the websocket handshake
    if (!headers_.empty()) {
        ws_ssl_.set_option(websocket::stream_base::decorator(
            [this](websocket::request_type& req) {
                for (const auto& kv : headers_) {
                    req.set(kv.first, kv.second);
                }
            }
        ));
    }

    // Set SNI (required by many TLS servers)
    if (!SSL_set_tlsext_host_name(ws_ssl_.next_layer().native_handle(), host.c_str())) {
        throw std::runtime_error("Failed to set SNI on SSL handle");
    }

    std::cout << "SNI set to " << host << std::endl;

    // Perform TLS handshake on the SSL stream
    ws_ssl_.next_layer().handshake(ssl::stream_base::client);

    std::cout << "TLS handshake completed" << std::endl;

    ws_ssl_.handshake(host, path_);

    std::cout << "WebSocket handshake completed" << std::endl;

    // Read messages
    // for (;;) {
    //     buffer_.consume(buffer_.size());
    //     ws_ssl_.read(buffer_, ec);
    //     std::cout << "Received message: " << beast::make_printable(buffer_.data()) << std::endl;
    //     if (ec) {
    //         throw beast::system_error{ec};
    //     }
    // }
}

void WebsocketClient::disconnect() {
    beast::error_code ec;
    ws_ssl_.close(websocket::close_code::normal, ec);
    if (ec) {
        throw beast::system_error{ec};
    }
    std::cout << "WebSocket connection closed." << std::endl;
}

ksApiMessage WebsocketClient::readMessageFromWebSocket() {
    beast::error_code ec;
    buffer_.consume(buffer_.size());
    ws_ssl_.read(buffer_, ec);
    if (ec) {
        throw beast::system_error{ec};
    }
    std::cout << "Received message: " << beast::make_printable(buffer_.data()) << std::endl;
    return ksApiMessage::parse(beast::buffers_to_string(buffer_.data()));
}

void WebsocketClient::sendMessageToWebSocket(const std::string& message) {
    beast::error_code ec;
    ws_ssl_.write(net::buffer(message), ec);
    if (ec) {
        throw beast::system_error{ec};
    }
    std::cout << "Sent message: " << message << std::endl;
}

void WebsocketClient::asyncReadMessage(std::function<void(const ksApiMessage&)> onMessage) {
    ws_ssl_.async_read(
        buffer_,
        [this, onMessage](beast::error_code ec, std::size_t bytes_transferred) {
            (void)bytes_transferred;
            
            if (ec) {
                std::cerr << "Async read error: " << ec.message() << std::endl;
                return;
            }
            
            // Parse and handle message
            try {
                ksApiMessage msg = ksApiMessage::parse(beast::buffers_to_string(buffer_.data()));
                std::cout << "Received async message: " << msg.dump() << std::endl;
                onMessage(msg);
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse message: " << e.what() << std::endl;
            }
            
            // Clear buffer and read next message
            buffer_.consume(buffer_.size());
            asyncReadMessage(onMessage);
        }
    );
}

void WebsocketClient::startAsyncRead(std::function<void(const ksApiMessage&)> onMessage) {
    asyncReadMessage(onMessage);
}

void WebsocketClient::run() {
    ioc_.run();
}