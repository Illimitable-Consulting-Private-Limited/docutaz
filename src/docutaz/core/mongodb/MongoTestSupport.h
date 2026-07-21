#pragma once

#include <mongocxx/instance.hpp>

// Test-only support shared across the MongoClient integration tests.
namespace docutaz_test
{
    // mongocxx permits EXACTLY ONE instance per process, constructed before any
    // client and outliving them all; a second construction throws. Every test
    // translation unit calls this instead of creating its own static instance —
    // the function-local static in this inline function has a single definition
    // program-wide, so all callers share the one instance regardless of test
    // ordering.
    inline void ensureMongocxxInstance()
    {
        static mongocxx::instance instance{};
    }
}
