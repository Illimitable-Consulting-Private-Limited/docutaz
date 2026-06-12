if(SYSTEM_LINUX OR SYSTEM_MACOSX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
elseif(SYSTEM_WINDOWS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_UNICODE -DUNICODE -DWIN32 -D_WIN32 -D_CRT_SECURE_NO_WARNINGS")

    # Do not show some warnings. We turned off the same warnings as MongoDB do
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4355 /wd4800 /wd4267 /wd4244 /wd4290 /wd4068 /wd4351")

    # The /MP option can reduce the total time to compile the source files. This option
    # causes the compiler to create one or more copies of itself, each in a separate process.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} /MP")

    set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} psapi.lib bcrypt.lib Iphlpapi.lib dbghelp.lib ws2_32.lib winmm.lib version.lib")
endif()

set(PROJECT_NAME "Docutaz")
set(PROJECT_NAME_TITLE ${PROJECT_NAME})
set(PROJECT_DOMAIN "")
set(PROJECT_COMPANYNAME "Docutaz")
set(PROJECT_COPYRIGHT "Copyright (C) 2026 ${PROJECT_COMPANYNAME}. All Rights Reserved.")
set(PROJECT_COMPANYNAME_DOMAIN "")
set(PROJECT_GITHUB_FORK "")
set(PROJECT_GITHUB_ISSUES "")
# "owner/repo" used for the GitHub Releases update check (see UpdateChecker).
set(PROJECT_GITHUB_REPO "Illimitable-Consulting-Private-Limited/docutaz")

string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWERCASE)
