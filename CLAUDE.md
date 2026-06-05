# Project: Docutaz — MongoDB 8+ fork of Robomongo

## Goal
Replace the embedded MongoDB shell with:
1. `libmongocxx` for all driver/GUI operations (MongoClient.cpp)
2. `mongosh` subprocess for shell tabs (MongoshEngine replacing ScriptEngine)

## The implementation guide
See `docs/implementation-guide.md` for the full plan with code.

## Build command
cd build && cmake .. -GNinja && ninja -j$(nproc)

## Current phase
Read the implementation guide. Your only job today is Phase 1: strip the embedded MongoDB shell from CMakeLists.txt, add find_package(libmongocxx), add find_package(libbsoncxx), and add a mongocxx::instance in main.cpp. Replace any mongo:: driver calls with stubs that compile. The project must compile at the end of this session. Run cmake and ninja and fix every error before finishing.
Replace all mongo::BSONObj with bsoncxx::document::value. Start with MongoDocument.h and MongoShellResult.h, then run the compiler and fix each error. Do not skip any file.
