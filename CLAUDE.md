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
Change all the branding from robo3t to Docutaz
Update the image in the welcome screen with the one at branding/docutaz-branding-trans.png
Remove robo3t icon from tabs use image at branding/docutaz-logo-trans.png
Find references to *.studio3t.com, *.robomongo.org
Find usage of studio3t and robomongo
