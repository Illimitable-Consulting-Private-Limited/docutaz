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
Fix the error find(...).toArray is not a function and aggregate([...]).toArray is not a function.
