#include "ksSubscriber.h"
#include "core/ksShm.h"
#include <iostream>

Subscriber::Subscriber(const std::string& uri, const std::string& path, const Headers& headers) {
    uri_ = uri;
    path_ = path;

    wsClient_ = std::make_unique<WebsocketClient>(uri_, path_, headers);
}

Subscriber::~Subscriber() {
    if (connected_)
    stop();
    delete wsClient_.release();
}

void Subscriber::initShm(const std::string& shm_name) {
    unifiedBook_.initShm(shm_name);
}

void Subscriber::start() {
    wsClient_->connect();
    connected_ = true;
}

void Subscriber::stop() {
    wsClient_->disconnect();
    connected_ = false;
}

void Subscriber::listenLoop() {
    while (connected_) {
        ksApiMessage msg = wsClient_->readMessageFromWebSocket();
        std::cout << "Processing message: " << msg.dump() << std::endl;

        switch (websocketMsgTypeMap[msg["type"]]) {
            case WebsocketMsgType::ticker:
                // Process ticker message
                std::cout << "Ticker message received." << std::endl;
                break;
            case WebsocketMsgType::orderbook_snapshot:
                // Process orderbook snapshot message
                unifiedBook_.initNewBook(msg);
                std::cout << "Orderbook snapshot message received." << std::endl;
                break;
            case WebsocketMsgType::orderbook_delta:
                // Process orderbook delta message
                unifiedBook_.updateBookWithDelta(msg);
                std::cout << "Orderbook delta message received." << std::endl;
                break;
            default:
                std::cout << "Unknown message type received." << std::endl;
                break;
        }
    }
}

void Subscriber::listenAsync() {
    wsClient_->startAsyncRead([this](const ksApiMessage& msg) {
        std::cout << "Processing async message: " << msg.dump() << std::endl;
        
        switch (websocketMsgTypeMap[msg["type"]]) {
            case WebsocketMsgType::ticker:
                std::cout << "Ticker message received." << std::endl;
                break;
            case WebsocketMsgType::orderbook_snapshot:
                unifiedBook_.initNewBook(msg);
                std::cout << "Orderbook snapshot message received." << std::endl;
                break;
            case WebsocketMsgType::orderbook_delta:
                unifiedBook_.updateBookWithDelta(msg);
                std::cout << "Orderbook delta message received." << std::endl;
                break;
            default:
                std::cout << "Unknown message type received." << std::endl;
                break;
        }
    });
    
    wsClient_->run();
}

void Subscriber::subscribeNewFeed(const std::string& channel, const std::string& market, const std::vector<std::string>& markets, const std::string& market_id, const std::vector<std::string>& market_ids) {

    ksApiMessage msg;
    msg["cmd"] = "subscribe";
    msg["id"] = m_id_++;
    msg["params"] = ksApiMessage::object();
    msg["params"]["channels"] = std::vector<std::string>{channel};
    std::cout << "Subscribing to channel: " << channel << std::endl;
    if (!market.empty()) {
        msg["params"]["market_ticker"] = market;
    }
    if (!markets.empty()) {
        msg["params"].erase("market_ticker");
        msg["params"]["market_tickers"] = markets;
    }
    if (!market_id.empty()) {
        msg["params"].erase("market_tickers");
        msg["params"]["market_id"] = market_id;
    }
    if (!market_ids.empty()) {
        msg["params"].erase("market_id");
        msg["params"]["market_ids"] = market_ids;
    }
    std::string msg_str = msg.dump();
    wsClient_->sendMessageToWebSocket(msg_str);
    std::cout << "Subscribing with message: " << msg_str << std::endl;

}

ksConfigSettings loadAuthConfig(const std::string& config_file_path) {
    std::cout << "Loading config from: " << config_file_path << std::endl;
    try {
        auto config = cpptoml::parse_file(config_file_path);
        ksConfigSettings settings = ksConfigSettings::object();

        for (const auto& kv : *config) {
            settings[kv.first] = kv.second->as<std::string>()->get();
        }
        return settings;
    } catch (const cpptoml::parse_exception& e) {
        throw std::runtime_error(std::string("Failed to parse config file: ") + e.what());
    }
}

ksConfigSettings loadSubConfig(const std::string& config_file_path) {
    std::cout << "Loading config from: " << config_file_path << std::endl;
    try {
        auto config = cpptoml::parse_file(config_file_path);
        ksConfigSettings settings = ksConfigSettings::object();

        for (const auto& kv : *config) {
            if (kv.second->is_array()) {
                std::vector<std::string> arr;
                for (const auto& item : *kv.second->as_array()) {
                    arr.push_back(item->as<std::string>()->get());
                }
                settings[kv.first] = arr;
            } else {
                settings[kv.first] = kv.second->as<std::string>()->get();
            }
        }
        return settings;
    } catch (const cpptoml::parse_exception& e) {
        throw std::runtime_error(std::string("Failed to parse config file: ") + e.what());
    }
}


int main(int argc, char* argv[]) {

    int c;
    std::string auth_cfg_path = "";
    std::string sub_cfg_path = "";

    while ((c = getopt(argc, argv, ":a:s:")) != -1) {
        switch(c) {
            case 'a':
                auth_cfg_path = optarg;
                break;
            case 's':
                sub_cfg_path = optarg;
                break;
            default:
                std::cerr << "Unknown option: " << c << std::endl;
                return 1;
        }
    }

    if (auth_cfg_path.empty() || sub_cfg_path.empty()) {
        std::cerr << "Usage: " << argv[0] << " -a <auth_config_path> -s <subscription_config_path>" << std::endl;
        return 1;
    }

    std::cout << "Auth config path: " << auth_cfg_path << std::endl;
    std::cout << "Subscription config path: " << sub_cfg_path << std::endl;

    ksConfigSettings authSettings = loadAuthConfig(auth_cfg_path);
    ksConfigSettings subSettings = loadSubConfig(sub_cfg_path);

    std::string url = authSettings["URL"];
    std::string path = authSettings["PATH"];

    std::string api_key = authSettings["API_KEY"];
    std::string method = "GET";
    std::string key_path = authSettings["SIGN_KEY_PATH"];

    int64_t millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    std::string timestamp = std::to_string(millis);

    std::string prehash = timestamp + method + path;

    std::cout << "Prehash string: " << prehash << std::endl;

    SecretKey secretKey(key_path);
    std::string signature = secretKey.sign(prehash.c_str());

    Headers headers;
    headers["KALSHI-ACCESS-KEY"] = api_key;
    headers["KALSHI-ACCESS-SIGNATURE"] = signature;
    headers["KALSHI-ACCESS-TIMESTAMP"] = timestamp;

    std::cout << "Headers prepared:" << std::endl;
    for (const auto& kv : headers) {
        std::cout << kv.first << ": " << kv.second << std::endl;
    }

    std::string shm_name = subSettings.value("SHM_NAME", std::string(SHM_DEFAULT_NAME));

    Subscriber subscriber(url, path, headers);
    subscriber.initShm(shm_name);
    subscriber.start();

    std::string requestType = "orderbook_delta";

    std::string& requestRef = requestType;

    std::cout << "Beginning subscription to feeds..." << std::endl;

    for (const auto& subfeed : subSettings["SUBSCRIBE_TICKERS"]) {
        std::cout << "Subscribing to feed: " << subfeed << std::endl;
        subscriber.subscribeNewFeed(requestRef, subfeed);
    }

    // subscriber.listenLoop();

    subscriber.listenAsync();

    return 0;
}