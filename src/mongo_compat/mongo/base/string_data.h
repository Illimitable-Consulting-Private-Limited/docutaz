#pragma once
#include <string>
#include <string_view>

namespace mongo {
// Alias to std::string_view for the old StringData type
using StringData = std::string_view;
}
