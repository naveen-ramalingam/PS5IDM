// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Web Server Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "web/web_server.h"
#include "core/logger.h"
#include "settings/settings.h"
#include "mongoose/mongoose.h"
#include <sstream>

namespace ps5dm {

static const char* TAG = "WebServer";

std::string WebHttpRequest::getHeader(const std::string& name) const {
    auto it = headers.find(name);
    return it != headers.end() ? it->second : "";
}

std::string WebHttpRequest::getQuery(const std::string& name) const {
    auto it = query.find(name);
    return it != query.end() ? it->second : "";
}

WebServer& WebServer::instance() {
    static WebServer server;
    return server;
}

Result<void> WebServer::init(int port, const std::string& documentRoot) {
    if (running_) return Result<void>::success();
    
    documentRoot_ = documentRoot;
    mgr_ = new mg_mgr;
    mg_mgr_init(mgr_);
    
    std::string url = "http://0.0.0.0:" + std::to_string(port);
    mg_connection* c = mg_http_listen(mgr_, url.c_str(), WebServer::evHandler, this);
    if (!c) {
        delete mgr_;
        mgr_ = nullptr;
        return Result<void>::failure(Error::make(1, "Failed to start web server on port " + std::to_string(port)));
    }
    
    running_ = true;
    serverThread_ = std::thread(&WebServer::serverLoop, this);
    
    LOG_INFO(TAG, "Web server started on " + url);
    return Result<void>::success();
}

void WebServer::shutdown() {
    if (!running_) return;
    
    LOG_INFO(TAG, "Shutting down web server...");
    running_ = false;
    
    // Wake up the manager to exit early
    if (mgr_) {
        mg_wakeup(mgr_, 0, nullptr, 0);
    }
    
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    if (mgr_) {
        mg_mgr_free(mgr_);
        delete mgr_;
        mgr_ = nullptr;
    }
    
    LOG_INFO(TAG, "Web server stopped.");
}

void WebServer::serverLoop() {
    while (running_) {
        mg_mgr_poll(mgr_, 500); // 500ms timeout
    }
}

void WebServer::addRoute(const std::string& method, const std::string& pattern, RouteHandler handler) {
    routes_.push_back({method, pattern, std::move(handler)});
}

void WebServer::broadcastWs(const std::string& message) {
    if (!mgr_ || !running_) return;
    
    // Safe thread-safe broadcast to all WS clients
    // (In mongoose, mg_wakeup is often used, but mg_ws_send can be used if careful,
    // though Mongoose 7 prefers calling it from the main mongoose thread. We will use a safe wrapper or broadcast loop).
    // For simplicity, we just iterate connections if we are careful, or enqueue.
    // Proper Mongoose 7 thread-safe way is to use mg_wakeup and handle it in the evHandler.
    // Let's pass the message via heap allocation to mg_wakeup.
    std::string* msgCopy = new std::string(message);
    mg_wakeup(mgr_, 0, msgCopy, sizeof(std::string*));
}

int WebServer::activeConnections() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return activeClients_;
}

int WebServer::wsConnections() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return wsClients_;
}

bool WebServer::isAuthorized(mg_connection* c, mg_http_message* hm) {
    auto& settings = Settings::instance();
    if (!settings.webAuthenticationEnabled()) return true;
    
    // Check Cookie for session token
    struct mg_str* cookie = mg_http_get_header(hm, "Cookie");
    if (cookie) {
        std::string cookieStr(cookie->buf, cookie->len);
        if (cookieStr.find("session=admin_token") != std::string::npos) {
            return true;
        }
    }
    
    // Fallback to basic auth checks or other token logic here if needed
    // For now, if no valid cookie is present, they aren't authorized
    
    return false;
}

void WebServer::handleHttp(mg_connection* c, mg_http_message* hm) {
    std::string method(hm->method.buf, hm->method.len);
    std::string uri(hm->uri.buf, hm->uri.len);
    
    if (method == "OPTIONS") {
        mg_http_reply(c, 204, "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type, Authorization, Cookie\r\n", "");
        return;
    }

    // Auth endpoints
    if (uri == "/api/v1/auth/login" && method == "POST") {
        auto& s = Settings::instance();
        std::string body(hm->body.buf, hm->body.len);
        try {
            auto j = json::parse(body);
            if (j.value("username", "") == s.webUsername() && j.value("password", "") == s.webPassword()) {
                mg_http_reply(c, 200, "Content-Type: application/json\r\nSet-Cookie: session=admin_token; Path=/; HttpOnly\r\n", "{\"success\":true}");
                return;
            }
        } catch (...) {}
        
        mg_http_reply(c, 401, "Content-Type: application/json\r\n", "{\"success\":false,\"error\":{\"message\":\"Invalid credentials\"}}");
        return;
    }

    // Check auth for API
    if (uri.find("/api/") == 0) {
        if (!isAuthorized(c, hm)) {
            mg_http_reply(c, 401, "Content-Type: application/json\r\n", "{\"success\":false,\"error\":{\"message\":\"Unauthorized\"}}");
            return;
        }
        
        // Find route
        for (const auto& route : routes_) {
            // Simple prefix or exact match (for simplicity we use exact match or basic path params)
            // Real routing would use regex, but we will do simple string prefix for parameterized routes
            bool match = false;
            if (route.pattern.back() == '*') {
                std::string prefix = route.pattern.substr(0, route.pattern.length() - 1);
                match = (uri.find(prefix) == 0);
            } else {
                match = (uri == route.pattern);
            }
            
            if (match && (method == route.method || route.method == "ANY")) {
                WebHttpRequest req;
                req.method = method;
                req.uri = uri;
                req.body = std::string(hm->body.buf, hm->body.len);
                // Parse headers/query omitted for brevity, will add as needed
                
                WebHttpResponse res = route.handler(req);
                
                // Add CORS
                res.headers["Access-Control-Allow-Origin"] = "*";
                
                std::ostringstream headers;
                for (auto& h : res.headers) headers << h.first << ": " << h.second << "\r\n";
                
                mg_http_reply(c, res.status, headers.str().c_str(), "%s", res.body.c_str());
                return;
            }
        }
        
        mg_http_reply(c, 404, "Content-Type: application/json\r\n", "{\"success\":false,\"error\":{\"message\":\"Not found\"}}");
        return;
    }
    
    // Static files
    if (method == "GET") {
        struct mg_http_serve_opts opts = {};
        opts.root_dir = documentRoot_.c_str();
        mg_http_serve_dir(c, hm, &opts);
    } else {
        mg_http_reply(c, 405, "", "Method not allowed");
    }
}

void WebServer::evHandler(mg_connection* c, int ev, void* ev_data) {
    WebServer* server = static_cast<WebServer*>(c->fn_data);
    if (!server) return; // Should not happen
    
    if (ev == MG_EV_ACCEPT) {
        std::lock_guard<std::mutex> lock(server->statsMutex_);
        server->activeClients_++;
    } else if (ev == MG_EV_CLOSE) {
        std::lock_guard<std::mutex> lock(server->statsMutex_);
        server->activeClients_--;
        if (c->is_websocket) server->wsClients_--;
    } else if (ev == MG_EV_WAKEUP) {
        struct mg_str* data = (struct mg_str*) ev_data;
        if (data && data->len == sizeof(std::string*)) {
            std::string* msg;
            std::memcpy(&msg, data->buf, sizeof(std::string*));
            for (mg_connection* t = c->mgr->conns; t != nullptr; t = t->next) {
                if (t->is_websocket) {
                    mg_ws_send(t, msg->data(), msg->size(), WEBSOCKET_OP_TEXT);
                }
            }
            delete msg;
        }
    } else if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message* hm = (struct mg_http_message*) ev_data;
        std::string uri(hm->uri.buf, hm->uri.len);
        
        if (uri == "/ws") {
            // Upgrade to websocket
            mg_ws_upgrade(c, hm, nullptr);
            std::lock_guard<std::mutex> lock(server->statsMutex_);
            server->wsClients_++;
            LOG_INFO(TAG, "WebSocket client connected");
        } else {
            server->handleHttp(c, hm);
        }
    } else if (ev == MG_EV_WS_MSG) {
        // Handle incoming WS messages (e.g. ping/pong, simple commands)
        struct mg_ws_message* wm = (struct mg_ws_message*) ev_data;
        // Just echo or handle specific WS commands if needed
    }
}

} // namespace ps5dm
