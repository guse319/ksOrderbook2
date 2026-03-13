// ksTypes.h
#pragma once

#include <map>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>
#include <unordered_map>

// SECRET KEY
using Key = char*;

// WEBSOCKET
using Headers = std::map<std::string, std::string>;
using MessageHandler = std::function<void(const std::string&)>;

// SUBSCRIBER
using ksApiMessage = nlohmann::json;
using ksConfigSettings = nlohmann::json;

enum class WebsocketMsgType {
    ticker,
    ticker_v2,
    orderbook_snapshot,
    orderbook_delta,
    trade,
    fill,
    market_position,
    market_lifecycle_v2,
    event_lifecycle,
    multivariate_lookup
};

inline std::unordered_map<std::string, WebsocketMsgType> websocketMsgTypeMap = {
    {"ticker", WebsocketMsgType::ticker},
    {"ticker_v2", WebsocketMsgType::ticker_v2},
    {"orderbook_snapshot", WebsocketMsgType::orderbook_snapshot},
    {"orderbook_delta", WebsocketMsgType::orderbook_delta},
    {"trade", WebsocketMsgType::trade},
    {"fill", WebsocketMsgType::fill},
    {"market_position", WebsocketMsgType::market_position},
    {"market_lifecycle_v2", WebsocketMsgType::market_lifecycle_v2},
    {"event_lifecycle", WebsocketMsgType::event_lifecycle},
    {"multivariate_lookup", WebsocketMsgType::multivariate_lookup}
};

// ORDERBOOK
using Price = std::uint32_t;
using Quantity = std::uint32_t;
using QuantityDelta = int32_t;

enum class Side {
    BID,
    ASK
};

enum class YesNo {
    NO,
    YES
};

struct Level {
    Price price_;
    Quantity quantity_;
};

using BidLevels = std::map<Price, Quantity, std::less<Price>>;
using AskLevels = std::map<Price, Quantity, std::greater<Price>>;

struct MarketSide {
    BidLevels bids_;
    AskLevels asks_;

    void addBid(Price price, Quantity quantity) {
        bids_[price] = quantity;
    }

    void addAsk(Price price, Quantity quantity) {
        asks_[price] = quantity;
    }

    void updateBid(Price price, QuantityDelta q_delta) {
        bids_[price] += q_delta;
    }

    void updateAsk(Price price, QuantityDelta q_delta) {
        asks_[price] += q_delta;
    }

    Quantity getBidQuantity(Price price) {
        return bids_[price];
    }

    Quantity getAskQuantity(Price price) {
        return asks_[price];
    }
};