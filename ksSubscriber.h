#ifndef KS_SUBSCRIBER_H
#define KS_SUBSCRIBER_H

#include "core/ksWebsocket.h"
#include "core/ksSecretKey.h"
#include "core/ksUnifiedBook.h"
#include "core/ksTypes.h"
#include "core/ksLogger.h"
#include <cpptoml.h>
#include <getopt.h>
#include <string>
#include <chrono>
#include <functional>
#include <memory>
#include <map>

class Subscriber {
public:
    Subscriber(const std::string& uri, const std::string& path, const Headers& headers);
    ~Subscriber();

    void start();
    void stop();

    void initShm(const std::string& shm_name);

    void listenLoop();
    
    void listenAsync();

    void subscribeNewFeed(const std::string& channel, const std::string& market="", const std::vector<std::string>& markets={}, const std::string& market_id="", const std::vector<std::string>& market_ids={});

private:
    std::string uri_;
    std::string path_;
    std::unique_ptr<WebsocketClient> wsClient_;

    UnifiedBook unifiedBook_;

    uint64_t m_id_ = 1;

    bool connected_ = false;
};

ksConfigSettings loadAuthConfig(const std::string& config_file_path);
ksConfigSettings loadSubConfig(const std::string& config_file_path);

#endif // KS_SUBSCRIBER_H
