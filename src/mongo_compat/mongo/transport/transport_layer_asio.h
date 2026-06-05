#pragma once
// Stub: transport layer removed — mongocxx manages its own transport.

namespace mongo {
namespace transport {

enum class ConnectSSLMode { kGlobalSSLMode, kEnableSSL, kDisableSSL };

} // transport
} // mongo
