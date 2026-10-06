// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Application Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "core/application.h"
#include "core/logger.h"
#include "core/event_bus.h"
#include "settings/settings.h"
#include "network/http_client.h"
#include "network/tls.h"
#include "database/database.h"
#include "downloader/download_manager.h"
#include "downloader/connection_manager.h"
#include "archive/archive_manager.h"
#include "filesystem/file_operations.h"
#include "filesystem/file_manager.h"
#include "ui/ui_manager.h"
#include "platform/paths.h"
#include "web/web_server.h"

#include <iostream>
#include <cstring>
#include <set>
#include <algorithm>
#include <sys/stat.h>

namespace ps5dm {

static const char* TAG = "App";

Application& Application::instance() {
    static Application app;
    return app;
}

AppArgs Application::parseArgs(int argc, char* argv[]) {
    AppArgs args;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--debug") == 0) {
            args.debugMode = true;
            args.logLevel = LogLevel::DEBUG;
        } else if (strncmp(argv[i], "--log-level=", 12) == 0) {
            std::string level = argv[i] + 12;
            if (level == "trace") args.logLevel = LogLevel::TRACE;
            else if (level == "debug") args.logLevel = LogLevel::DEBUG;
            else if (level == "info")  args.logLevel = LogLevel::INFO;
            else if (level == "warn")  args.logLevel = LogLevel::WARN;
            else if (level == "error") args.logLevel = LogLevel::ERR;
        } else if (strcmp(argv[i], "--test-network") == 0) {
            args.testNetwork = true;
        } else if (strcmp(argv[i], "--test-archive") == 0) {
            args.testArchive = true;
        } else if (strncmp(argv[i], "--config=", 9) == 0) {
            args.configPath = argv[i] + 9;
        }
    }

    return args;
}

Result<void> Application::init(const AppArgs& args) {
    args_ = args;

    // ─── Create platform ────────────────────────────────────────────
    platform_ = createPlatform();
    auto platformResult = platform_->init();
    if (!platformResult.ok()) {
        return Result<void>::failure(platformResult.error);
    }

    // ─── Create data directories ────────────────────────────────────
    std::string dataPath = platform_->getDataPath();
    mkdir(dataPath.c_str(), 0755);
    mkdir(platform_->getTempPath().c_str(), 0755);

    // ─── Initialize logger ──────────────────────────────────────────
    std::string logPath = Paths::join(dataPath, "ps5dm.log");
    Logger::instance().init(logPath, args.logLevel);

    LOG_INFO(TAG, "═══════════════════════════════════════════");
    LOG_INFO(TAG, "  PS5 Download Manager v" + std::string(APP_VERSION));
    LOG_INFO(TAG, "  Platform: " + platform_->getSystemVersion());
    LOG_INFO(TAG, "═══════════════════════════════════════════");

    // ─── Load settings ──────────────────────────────────────────────
    std::string settingsPath = args.configPath.empty() ?
        Paths::join(dataPath, "settings.conf") : args.configPath;
    Settings::instance().load(settingsPath);

    // Apply log level from settings if not overridden by CLI
    if (!args.debugMode) {
        Logger::instance().setLevel(Settings::instance().logLevel());
    }

    // ─── Initialize TLS ─────────────────────────────────────────────
    auto tlsResult = TLSConfig::init();
    if (!tlsResult.ok()) {
        LOG_WARN(TAG, "TLS init warning: " + tlsResult.error.message);
    }

    // ─── Initialize libcurl ─────────────────────────────────────────
    HttpClient::globalInit();

    // ─── Initialize database ────────────────────────────────────────
    std::string dbPath = Paths::join(dataPath, "ps5dm.db");
    auto dbResult = Database::instance().open(dbPath);
    if (!dbResult.ok()) {
        LOG_WARN(TAG, "Database warning: " + dbResult.error.message);
    }

    // ─── Initialize download manager ────────────────────────────────
    // Update download/temp dirs based on platform
    auto& settings = Settings::instance();
    if (settings.downloadDirectory() == "/data/downloads") {
        settings.setDownloadDirectory(Paths::join(dataPath, "downloads"));
    }
    if (settings.tempDirectory() == "/data/downloads/.tmp") {
        settings.setTempDirectory(Paths::join(dataPath, "downloads/.tmp"));
    }

    auto dlResult = DownloadManager::instance().init();
    if (!dlResult.ok()) {
        LOG_ERROR(TAG, "Download manager init failed: " + dlResult.error.message);
    }

    // Configure speed limit
    ConnectionManager::instance().setSpeedLimit(settings.downloadSpeedLimit());

    // ─── Initialize UI ──────────────────────────────────────────────
    auto uiResult = UIManager::instance().init(platform_.get());
    if (!uiResult.ok()) {
        LOG_ERROR(TAG, "UI init failed: " + uiResult.error.message);
    }

    // ─── Recover incomplete downloads ───────────────────────────────
    if (settings.resumeDownloadsOnStart()) {
        int recovered = DownloadManager::instance().recoverDownloads();
        if (recovered > 0) {
            LOG_INFO(TAG, "Recovered " + std::to_string(recovered) + " incomplete downloads");
        }
    }

    // ─── Initialize graphics ────────────────────────────────────────
    platform_->initGraphics();

    // ─── Initialize Web Server ──────────────────────────────────────
    if (settings.webServerEnabled()) {
        int port = settings.webServerPort();
        std::string docRoot = Paths::join(dataPath, "web");
        mkdir(docRoot.c_str(), 0755);
        
        auto& web = WebServer::instance();
        web.addRoute("GET", "/api/v1/status", [](const WebHttpRequest&) {
            json j = {
                {"status", "running"},
                {"version", APP_VERSION},
                {"downloads", DownloadManager::instance().getDownloads().size()},
                {"activeDownloads", DownloadManager::instance().activeCount()}
            };
            return WebHttpResponse::json(j);
        });
        
        web.addRoute("GET", "/api/v1/downloads", [](const WebHttpRequest&) {
            auto tasks = DownloadManager::instance().getDownloads();
            json list = json::array();
            for (auto& t : tasks) {
                double pct = t->totalSize > 0 ? (double)(t->downloadedSize.load()) / (double)t->totalSize * 100.0 : 0.0;
                int64_t speed = t->speedTracker.currentSpeed();
                int64_t remaining = t->totalSize - t->downloadedSize.load();
                int64_t eta = (speed > 0 && remaining > 0) ? remaining / speed : 0;
                list.push_back({
                    {"id", t->id},
                    {"url", t->url},
                    {"filename", t->filename},
                    {"progress", pct},
                    {"speed", speed},
                    {"downloaded", (int64_t)t->downloadedSize.load()},
                    {"total", (int64_t)t->totalSize},
                    {"eta", eta},
                    {"status", static_cast<int>(t->status)}
                });
            }
            return WebHttpResponse::json({{"success", true}, {"data", list}});
        });
        
        web.addRoute("POST", "/api/v1/downloads", [](const WebHttpRequest& req) {
            try {
                auto j = json::parse(req.body);
                AddDownloadParams p;
                p.url = j.value("url", "");
                if (p.url.empty()) return WebHttpResponse::json({{"success", false}, {"error", {{"message", "URL is required"}}}});
                p.filename = j.value("filename", "");
                p.startImmediately = j.value("startImmediately", true);
                
                auto res = DownloadManager::instance().addDownload(p);
                if (res.ok()) {
                    return WebHttpResponse::json({{"success", true}, {"data", {{"id", res.value.value()}}}});
                } else {
                    return WebHttpResponse::json({{"success", false}, {"error", {{"message", res.error.message}}}});
                }
            } catch (const std::exception& e) {
                return WebHttpResponse::json({{"success", false}, {"error", {{"message", "Invalid JSON"}}}});
            }
        });
        
        web.addRoute("POST", "/api/v1/downloads/action", [](const WebHttpRequest& req) {
            try {
                auto j = json::parse(req.body);
                DownloadId id = j.value("id", static_cast<DownloadId>(0));
                std::string action = j.value("action", "");
                
                Result<void> res = Result<void>::success();
                if (action == "pause") {
                    res = DownloadManager::instance().pauseDownload(id);
                } else if (action == "resume") {
                    res = DownloadManager::instance().resumeDownload(id);
                } else if (action == "cancel") {
                    res = DownloadManager::instance().cancelDownload(id);
                } else {
                    return WebHttpResponse::json({{"success", false}, {"error", {{"message", "Invalid action"}}}});
                }
                
                if (res.ok()) return WebHttpResponse::json({{"success", true}});
                else return WebHttpResponse::json({{"success", false}, {"error", {{"message", res.error.message}}}});
            } catch (...) {
                return WebHttpResponse::json({{"success", false}, {"error", {{"message", "Invalid request"}}}});
            }
        });
        
        web.addRoute("GET", "/api/v1/archives", [](const WebHttpRequest&) {
            ArchiveManager::instance().rescan();
            auto groups = ArchiveManager::instance().groups();
            json list = json::array();
            for (auto& g : groups) {
                list.push_back({
                    {"id", g->id},
                    {"baseName", g->baseName},
                    {"firstPartPath", g->firstPartPath},
                    {"isComplete", g->isComplete()},
                    {"expectedParts", g->expectedParts},
                    {"foundPartCount", g->foundPartCount()},
                    {"totalSize", g->totalSize},
                    {"status", static_cast<int>(g->status)}
                });
            }
            return WebHttpResponse::json({{"success", true}, {"data", list}});
        });

        web.addRoute("POST", "/api/v1/archives/action", [](const WebHttpRequest& req) {
            try {
                auto j = json::parse(req.body);
                ArchiveGroupId id = j.value("id", static_cast<ArchiveGroupId>(0));
                std::string action = j.value("action", "");
                std::string password = j.value("password", "");
                
                if (action == "extract") {
                    ArchiveManager::instance().extractGroup(id, "", password);
                    return WebHttpResponse::json({{"success", true}});
                } else if (action == "remove") {
                    ArchiveManager::instance().removeGroup(id);
                    return WebHttpResponse::json({{"success", true}});
                } else {
                    return WebHttpResponse::json({{"success", false}, {"error", {{"message", "Invalid action"}}}});
                }
            } catch (...) {
                return WebHttpResponse::json({{"success", false}, {"error", {{"message", "Invalid request"}}}});
            }
        });
        
        // ── Link Grabber endpoint ──────────────────────────────────────────────
        web.addRoute("POST", "/api/v1/links/grab", [](const WebHttpRequest& req) {
            try {
                auto j = json::parse(req.body);
                std::string pageUrl = j.value("url", "");
                if (pageUrl.empty()) {
                    return WebHttpResponse::json({{"success", false}, {"error", {{"message", "URL is required"}}}});
                }

                // Fetch the page HTML
                HttpClient client;
                client.setUserAgent("Mozilla/5.0 (PlayStation 5) PS5DM/1.0");
                auto result = client.get(pageUrl);
                if (!result.ok()) {
                    return WebHttpResponse::json({{"success", false}, {"error", {{"message", "Failed to fetch page: " + result.error.message}}}});
                }

                std::string html = result.get().body;

                // Extract base URL for resolving relative links
                std::string baseUrl;
                {
                    auto pos = pageUrl.find("://");
                    if (pos != std::string::npos) {
                        auto slashPos = pageUrl.find('/', pos + 3);
                        baseUrl = (slashPos != std::string::npos) ? pageUrl.substr(0, slashPos) : pageUrl;
                    }
                }
                // Directory base for relative paths
                std::string dirBase = pageUrl;
                {
                    auto lastSlash = dirBase.rfind('/');
                    if (lastSlash != std::string::npos && lastSlash > 8) {
                        dirBase = dirBase.substr(0, lastSlash + 1);
                    }
                }

                // File extensions we look for
                const std::vector<std::string> extensions = {
                    ".pkg", ".zip", ".rar", ".7z", ".tar", ".gz", ".bz2", ".xz",
                    ".iso", ".bin", ".img", ".dmg", ".apk", ".ipa",
                    ".mp4", ".mkv", ".avi", ".mov", ".wmv",
                    ".exe", ".msi", ".deb", ".rpm",
                    ".part1", ".part01", ".r00", ".r01", ".z01",
                    ".001", ".002"
                };

                // Parse href="..." and src="..." from HTML
                json links = json::array();
                std::set<std::string> seen;
                size_t searchPos = 0;

                auto extractAttrLinks = [&](const std::string& attr) {
                    searchPos = 0;
                    std::string needle = attr + "=\"";
                    while ((searchPos = html.find(needle, searchPos)) != std::string::npos) {
                        searchPos += needle.size();
                        auto endPos = html.find('"', searchPos);
                        if (endPos == std::string::npos) break;

                        std::string link = html.substr(searchPos, endPos - searchPos);
                        searchPos = endPos + 1;

                        // Skip empty, javascript:, mailto:, #
                        if (link.empty() || link[0] == '#' || link.find("javascript:") == 0 || link.find("mailto:") == 0) continue;

                        // Resolve relative URLs
                        if (link.find("://") == std::string::npos) {
                            if (link[0] == '/') {
                                link = baseUrl + link;
                            } else {
                                link = dirBase + link;
                            }
                        }

                        // Check if it matches a known file extension
                        std::string lowerLink = link;
                        std::transform(lowerLink.begin(), lowerLink.end(), lowerLink.begin(), ::tolower);
                        
                        // Remove query params for extension check
                        std::string checkLink = lowerLink;
                        auto qPos = checkLink.find('?');
                        if (qPos != std::string::npos) checkLink = checkLink.substr(0, qPos);

                        bool matches = false;
                        for (auto& ext : extensions) {
                            if (checkLink.find(ext) != std::string::npos) {
                                matches = true;
                                break;
                            }
                        }

                        if (matches && seen.find(link) == seen.end()) {
                            seen.insert(link);
                            // Extract filename
                            std::string filename = link;
                            auto slashP = filename.rfind('/');
                            if (slashP != std::string::npos) filename = filename.substr(slashP + 1);
                            auto queryP = filename.find('?');
                            if (queryP != std::string::npos) filename = filename.substr(0, queryP);

                            links.push_back({
                                {"url", link},
                                {"filename", filename}
                            });
                        }
                    }
                };

                extractAttrLinks("href");
                extractAttrLinks("src");
                extractAttrLinks("action");
                // Also try single-quoted attributes
                auto extractSingleQuoteLinks = [&](const std::string& attr) {
                    searchPos = 0;
                    std::string needle = attr + "='";
                    while ((searchPos = html.find(needle, searchPos)) != std::string::npos) {
                        searchPos += needle.size();
                        auto endPos = html.find('\'', searchPos);
                        if (endPos == std::string::npos) break;

                        std::string link = html.substr(searchPos, endPos - searchPos);
                        searchPos = endPos + 1;

                        if (link.empty() || link[0] == '#' || link.find("javascript:") == 0) continue;

                        if (link.find("://") == std::string::npos) {
                            if (link[0] == '/') {
                                link = baseUrl + link;
                            } else {
                                link = dirBase + link;
                            }
                        }

                        std::string lowerLink = link;
                        std::transform(lowerLink.begin(), lowerLink.end(), lowerLink.begin(), ::tolower);
                        std::string checkLink = lowerLink;
                        auto qPos = checkLink.find('?');
                        if (qPos != std::string::npos) checkLink = checkLink.substr(0, qPos);

                        bool matches = false;
                        for (auto& ext : extensions) {
                            if (checkLink.find(ext) != std::string::npos) {
                                matches = true;
                                break;
                            }
                        }

                        if (matches && seen.find(link) == seen.end()) {
                            seen.insert(link);
                            std::string filename = link;
                            auto slashP = filename.rfind('/');
                            if (slashP != std::string::npos) filename = filename.substr(slashP + 1);
                            auto queryP = filename.find('?');
                            if (queryP != std::string::npos) filename = filename.substr(0, queryP);

                            links.push_back({
                                {"url", link},
                                {"filename", filename}
                            });
                        }
                    }
                };
                extractSingleQuoteLinks("href");
                extractSingleQuoteLinks("src");
                extractSingleQuoteLinks("action");

                return WebHttpResponse::json({
                    {"success", true},
                    {"data", {
                        {"pageUrl", pageUrl},
                        {"pageTitle", ""},
                        {"linksFound", links.size()},
                        {"links", links}
                    }}
                });
            } catch (const std::exception& e) {
                return WebHttpResponse::json({{"success", false}, {"error", {{"message", std::string("Parse error: ") + e.what()}}}});
            } catch (...) {
                return WebHttpResponse::json({{"success", false}, {"error", {{"message", "Unknown error"}}}});
            }
        });

        web.addRoute("GET", "/api/v1/files", [](const WebHttpRequest& req) {
            // Very simple path handling, in reality we'd parse query params
            auto path = Settings::instance().downloadDirectory(); 
            auto result = FileOperations::listDirectory(path);
            if (!result.ok()) {
                return WebHttpResponse::json({{"success", false}, {"error", {{"message", result.error.message}}}});
            }
            
            json list = json::array();
            for (auto& f : result.get()) {
                list.push_back({
                    {"name", f.name},
                    {"path", f.path},
                    {"size", f.size},
                    {"isDirectory", f.isDirectory},
                    {"isArchive", f.isArchive}
                });
            }
            return WebHttpResponse::json({{"success", true}, {"data", list}});
        });

        web.addRoute("GET", "/api/v1/settings", [](const WebHttpRequest&) {
            auto& s = Settings::instance();
            json j = {
                {"downloadDir", s.downloadDirectory()},
                {"maxConcurrentDownloads", s.maxActiveDownloads()},
                {"downloadSpeedLimit", s.downloadSpeedLimit()}
            };
            return WebHttpResponse::json({{"success", true}, {"data", j}});
        });

        web.addRoute("POST", "/api/v1/settings", [](const WebHttpRequest& req) {
            try {
                auto j = json::parse(req.body);
                auto& s = Settings::instance();
                
                if (j.contains("downloadDir")) {
                    s.setDownloadDirectory(j["downloadDir"].get<std::string>());
                }
                if (j.contains("maxConcurrentDownloads")) {
                    s.setMaxActiveDownloads(j["maxConcurrentDownloads"].get<int>());
                }
                if (j.contains("downloadSpeedLimit")) {
                    s.setDownloadSpeedLimit(j["downloadSpeedLimit"].get<FileSize>());
                }
                
                s.save();
                return WebHttpResponse::json({{"success", true}});
            } catch (...) {
                return WebHttpResponse::json({{"success", false}, {"error", {{"message", "Invalid request"}}}});
            }
        });

        if (!web.init(port, docRoot).ok()) {
            LOG_ERROR(TAG, "Failed to initialize Web Server");
        } else {
            // Forward events to WebSocket
            EventBus::instance().subscribe(EventType::DOWNLOAD_PROGRESS, [](const Event& e) {
                try {
                    auto evt = std::any_cast<DownloadProgressEvent>(e.data);
                    json j = {
                        {"event", "download.progress"},
                        {"data", {
                            {"id", evt.downloadId},
                            {"progress", evt.percentage},
                            {"speed", evt.speed},
                            {"downloaded", evt.downloadedBytes},
                            {"total", evt.totalBytes},
                            {"eta", 0} // ETA not in struct directly, but could be calculated
                        }}
                    };
                    WebServer::instance().broadcastWsJson(j);
                } catch (...) {}
            });
            EventBus::instance().subscribe(EventType::DOWNLOAD_COMPLETED, [](const Event& e) {
                try {
                    auto evt = std::any_cast<DownloadStatusEvent>(e.data);
                    json j = { {"event", "download.completed"}, {"data", {{"id", evt.downloadId}}} };
                    WebServer::instance().broadcastWsJson(j);
                } catch (...) {}
            });
        }
    }

    initialized_ = true;
    LOG_INFO(TAG, "Application initialized successfully");
    return Result<void>::success();
}

int Application::run() {
    if (!initialized_) {
        std::cerr << "Application not initialized!" << std::endl;
        return 1;
    }

    LOG_INFO(TAG, "Starting main loop...");
    running_ = true;

    mainLoop();

    shutdown();
    return 0;
}

void Application::mainLoop() {
    auto lastTime = SteadyClock::now();
    int frameCount = 0;
    auto lastStorageCheck = SteadyClock::now();

    while (running_.load() && !platform_->shouldQuit()) {
        auto currentTime = SteadyClock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // ─── Input ──────────────────────────────────────────────
        platform_->pollInput();
        while (platform_->hasInputEvent()) {
            auto event = platform_->nextInputEvent();
            UIManager::instance().handleInput(event);
        }

        // ─── Update ─────────────────────────────────────────────
        UIManager::instance().update(deltaTime);

        // ─── Render ─────────────────────────────────────────────
        platform_->beginFrame();

        // Clear screen (terminal mode)
        std::cout << "\033[2J\033[H";  // ANSI clear + home
        std::cout << UIManager::instance().render();
        std::cout.flush();

        platform_->endFrame();
        platform_->swapBuffers();

        // ─── Periodic checks ────────────────────────────────────
        auto elapsed = std::chrono::duration_cast<Seconds>(currentTime - lastStorageCheck);
        if (elapsed.count() >= 60) {
            checkStorageSpace();
            checkNetworkStatus();
            lastStorageCheck = currentTime;
        }

        // Frame rate limiting (~30 fps for terminal)
        platform_->sleepMs(33);
        frameCount++;
    }
}

void Application::checkStorageSpace() {
    auto& settings = Settings::instance();
    auto info = platform_->getStorageInfo(settings.downloadDirectory());
    auto threshold = settings.lowStorageThreshold();

    if (info.freeBytes > 0 && info.freeBytes < threshold) {
        LOG_WARN(TAG, "Low storage: " + formatSize(info.freeBytes) + " free");
        EventBus::instance().publish(EventType::STORAGE_WARNING,
            StorageEvent{info.freeBytes, threshold, settings.downloadDirectory()});
        UIManager::instance().showNotification("Low Storage",
            "Only " + formatSize(info.freeBytes) + " remaining", LogLevel::WARN);
    }
}

void Application::checkNetworkStatus() {
    auto status = platform_->getNetworkStatus();
    static bool wasConnected = true;

    if (!status.connected && wasConnected) {
        LOG_WARN(TAG, "Network disconnected");
        EventBus::instance().publish(EventType::NETWORK_DISCONNECTED);
        UIManager::instance().showNotification("Network Lost",
            "Downloads paused automatically", LogLevel::WARN);
        DownloadManager::instance().pauseAll();
    } else if (status.connected && !wasConnected) {
        LOG_INFO(TAG, "Network restored");
        EventBus::instance().publish(EventType::NETWORK_CONNECTED);
        UIManager::instance().showNotification("Network Restored",
            "Resuming downloads...", LogLevel::INFO);
        DownloadManager::instance().resumeAll();
    }

    wasConnected = status.connected;
}

void Application::requestShutdown() {
    running_ = false;
}

void Application::shutdown() {
    LOG_INFO(TAG, "Shutting down...");

    // Save settings
    Settings::instance().save();

    // Shutdown subsystems in reverse order
    UIManager::instance().shutdown();
    WebServer::instance().shutdown();
    DownloadManager::instance().shutdown();
    ArchiveManager::instance().clear();
    Database::instance().close();
    HttpClient::globalCleanup();
    TLSConfig::cleanup();

    if (platform_) {
        platform_->shutdown();
    }

    Logger::instance().shutdown();
    initialized_ = false;
}

} // namespace ps5dm
