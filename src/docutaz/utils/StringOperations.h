#pragma once

#include <string>

namespace Docutaz
{
    // Capitalize first char (Mongo errors often come all lower case)
    std::string captilizeFirstChar(std::string str);
}
