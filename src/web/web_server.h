#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Web Server
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include "common/json/json.hpp"

struct mg_mgr;
struct mg_connection;
struct mg_http_message;

namespace ps5dm {

using json = nlohmann::json;

/// HTTP Request
struct WebHttpRequest {
    std::string method;
    std::string uri;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query;
    std::string getHeader(const std::string& name) const;
    std::string getQuery(const std::string& name) const;
};

/// HTTP Response
struct WebHttpResponse {
    int status = 200;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    
    static WebHttpResponse json(const nlohmann::json& j, int status = 200) {
        WebHttpResponse res;
        res.status = status;
        res.headers["Content-Type"] = "application/json";
        res.body = j.dump();
        return res;
    }
    
    static WebHttpResponse text(const std::string& txt, int status = 200) {
        WebHttpResponse res;
        res.status = status;
        res.headers["Content-Type"] = "text/plain";
        res.body = txt;
        return res;
    }
};

using RouteHandler = std::function<WebHttpResponse(const WebHttpRequest&)>;

class WebServer {
public:
    static WebServer& instance();
    
    Result<void> init(int port, const std::string& documentRoot);
    void shutdown();
    
    // Router
    void addRoute(const std::string& method, const std::string& pattern, RouteHandler handler);
    
    // WebSocket
    void broadcastWs(const std::string& message);
    void broadcastWsJson(const json& j) { broadcastWs(j.dump()); }
    
    int activeConnections() const;
    int wsConnections() const;

private:
    WebServer() = default;
    ~WebServer() { shutdown(); }
    
    void serverLoop();
    static void evHandler(mg_connection* c, int ev, void* ev_data);
    
    void handleHttp(mg_connection* c, mg_http_message* hm);
    
    // Auth Check
    bool isAuthorized(mg_connection* c, mg_http_message* hm);
    
    std::atomic<bool> running_{false};
    std::thread serverThread_;
    mg_mgr* mgr_ = nullptr;
    std::string documentRoot_;
    
    struct Route {
        std::string method;
        std::string pattern;
        RouteHandler handler;
    };
    std::vector<Route> routes_;
    
    mutable std::mutex statsMutex_;
    int activeClients_ = 0;
    int wsClients_ = 0;
};

} // namespace ps5dm
