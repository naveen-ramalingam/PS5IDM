// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - TLS Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "network/tls.h"
#include "core/logger.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>

namespace ps5dm {

static const char* TAG = "TLS";

Result<void> TLSConfig::init() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    LOG_INFO(TAG, "OpenSSL initialized: " + getVersion());
    return Result<void>::success();
}

void TLSConfig::cleanup() {
    EVP_cleanup();
    ERR_free_strings();
    LOG_INFO(TAG, "OpenSSL cleaned up");
}

std::string TLSConfig::getVersion() {
    return OpenSSL_version(OPENSSL_VERSION);
}

bool TLSConfig::isAvailable() {
    return true;
}

} // namespace ps5dm
