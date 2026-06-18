#include "gtest/gtest.h"

#include "docutaz/core/settings/ConnectionSettings.h"

using Docutaz::ConnectionSettings;

// Guards percent-encoding of connection-string userinfo (username/password).
// A password with reserved characters must be encoded or it corrupts the URI
// (e.g. ':' would be read as the user/password separator, '@' as the host
// delimiter). The driver percent-decodes it back, so over-encoding is safe.

TEST(connection_uri, userinfo_unreserved_chars_unchanged)
{
    // RFC 3986 unreserved set: ALPHA / DIGIT / - . _ ~ — left as-is.
    EXPECT_EQ(ConnectionSettings::percentEncodeUserInfo("Staging-1.0_~aZ9"),
              std::string("Staging-1.0_~aZ9"));
    EXPECT_EQ(ConnectionSettings::percentEncodeUserInfo(""), std::string(""));
}

TEST(connection_uri, userinfo_reserved_chars_are_encoded)
{
    // The characters MongoDB specifically requires encoded in userinfo.
    EXPECT_EQ(ConnectionSettings::percentEncodeUserInfo(":/?#[]@"),
              std::string("%3A%2F%3F%23%5B%5D%40"));
    // '%' itself must be encoded, and space as %20 (not '+').
    EXPECT_EQ(ConnectionSettings::percentEncodeUserInfo("a %b"),
              std::string("a%20%25b"));
    // A realistic password.
    EXPECT_EQ(ConnectionSettings::percentEncodeUserInfo("p@ss:w/rd"),
              std::string("p%40ss%3Aw%2Frd"));
}

// socketTimeoutMS bounds individual socket reads so a stalled driver query can't
// wedge the worker thread forever. It must be present (in ms) when a socket
// timeout is requested, and absent when it isn't.
TEST(connection_uri, socket_timeout_option)
{
    ConnectionSettings c(false);
    c.setServerHost("localhost");
    c.setServerPort(27017);

    const std::string withTimeout = ConnectionSettings::buildMongoUri(&c, 10, 15);
    EXPECT_NE(withTimeout.find("socketTimeoutMS=15000"), std::string::npos);

    const std::string noTimeout = ConnectionSettings::buildMongoUri(&c, 10, 0);
    EXPECT_EQ(noTimeout.find("socketTimeoutMS"), std::string::npos);
}
