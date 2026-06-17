
# Automatically generate code for Qt moc, uic and rcc files
# Since CMake 2.8.6
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

#set(sanitize "-fsanitize=address")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${sanitize}")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${sanitize}")

set(SOURCES
    # Isolated Scope #1
    core/utils/QtUtils.cpp
    core/utils/StdUtils.cpp
    core/utils/Logger.cpp
    core/UpdateChecker.cpp
    core/QueryHistory.cpp
    core/HexUtils.cpp
    core/utils/BsonUtils.cpp
    core/settings/CredentialSettings.cpp
    core/settings/ConnectionSettings.cpp
    core/Event.cpp
    core/Enums.cpp
    core/EventError.cpp
    core/EventBusSubscriber.cpp
    core/EventBusDispatcher.cpp
    core/EventWrapper.cpp
    core/EventBus.cpp
    core/KeyboardManager.cpp
    core/domain/MongoNamespace.cpp
    core/domain/MongoFunction.cpp
    core/domain/MongoUtils.cpp
    core/domain/MongoCollection.cpp
    core/domain/MongoCollectionInfo.cpp
    core/domain/MongoQueryInfo.cpp
    core/domain/CursorPosition.cpp
    core/domain/ScriptInfo.cpp
    core/events/MongoEventsInfo.cpp
    shell/db/ptimeutil.cpp
    shell/bson/json.cpp

    # Isolated Scope #2
    core/engine/MongoshEngine.cpp
    core/events/MongoEvents.cpp
    core/domain/MongoDocument.cpp
    gui/AppStyle.cpp
    core/domain/MongoServer.cpp
    core/domain/MongoShell.cpp
    core/domain/MongoDatabase.cpp
    core/domain/App.cpp
    core/mongodb/MongoClient.cpp
    core/mongodb/MongoWorker.cpp
    core/mongodb/ReplicaSet.cpp
    core/settings/SettingsManager.cpp
    core/AppRegistry.cpp
    utils/StringOperations.cpp
    utils/common.cpp
    utils/SimpleCrypt.cpp
    utils/DocutazCrypt.cpp

    # Isolated Scope #3
    gui/GuiRegistry.cpp
    gui/dialogs/AboutDialog.cpp
    gui/dialogs/EulaDialog.cpp
    gui/dialogs/ConnectionAdvancedTab.cpp
    gui/dialogs/ConnectionAuthTab.cpp
    gui/dialogs/ConnectionBasicTab.cpp
    gui/dialogs/ConnectionDiagnosticDialog.cpp
    gui/dialogs/ConnectionDialog.cpp
    gui/dialogs/CopyCollectionDialog.cpp
    gui/widgets/workarea/IndicatorLabel.cpp
    gui/dialogs/CreateCollectionDialog.cpp
    gui/dialogs/CreateDatabaseDialog.cpp
    gui/dialogs/CreateUserDialog.cpp
    gui/utils/ComboBoxUtils.cpp
    gui/utils/DialogUtils.cpp

    # Isolated scope #4
    gui/dialogs/PreferencesDialog.cpp
    gui/dialogs/ConnectionsDialog.cpp
    gui/dialogs/ExportDialog.cpp
    gui/dialogs/ChangeShellTimeoutDialog.cpp

    # Isolated scope #5
    gui/editors/PlainJavaScriptEditor.cpp
    gui/editors/JSLexer.cpp
    gui/editors/JsBeautifier.cpp
    gui/editors/FindFrame.cpp
    gui/widgets/explorer/AddEditIndexDialog.cpp
    gui/widgets/workarea/ScriptWidget.cpp

    # Isolated scope #6
    gui/widgets/explorer/ExplorerCollectionTreeItem.cpp
    gui/widgets/explorer/ExplorerCollectionIndexesDir.cpp
    gui/widgets/explorer/ExplorerCollectionIndexItem.cpp
    gui/widgets/explorer/ExplorerTreeItem.cpp
    gui/widgets/explorer/ExplorerReplicaSetFolderItem.cpp
    gui/widgets/explorer/ExplorerReplicaSetTreeItem.cpp
    gui/widgets/explorer/ExplorerDatabaseTreeItem.cpp
    gui/widgets/explorer/ExplorerDatabaseCategoryTreeItem.cpp
    gui/widgets/explorer/ExplorerUserTreeItem.cpp
    gui/widgets/explorer/ExplorerFunctionTreeItem.cpp
    gui/dialogs/DocumentTextEditor.cpp
    gui/dialogs/FunctionTextEditor.cpp

    # Isolated scope #7
    gui/widgets/explorer/ExplorerServerTreeItem.cpp
    gui/widgets/explorer/ExplorerTreeWidget.cpp
    gui/widgets/explorer/ExplorerWidget.cpp
    gui/widgets/workarea/BsonTableModel.cpp
    gui/widgets/workarea/BsonTableView.cpp
    gui/widgets/workarea/BsonTreeItem.cpp
    gui/widgets/workarea/BsonTreeModel.cpp
    gui/widgets/workarea/BsonTreeView.cpp
    core/domain/Notifier.cpp

    # Isolated scope #8
    gui/widgets/workarea/CollectionStatsTreeItem.cpp
    gui/widgets/workarea/CollectionStatsTreeWidget.cpp
    gui/widgets/workarea/JsonPrepareThread.cpp
    gui/widgets/workarea/OutputItemContentWidget.cpp
    gui/widgets/workarea/OutputItemHeaderWidget.cpp
    gui/widgets/workarea/OutputWidget.cpp
    gui/widgets/workarea/PagingWidget.cpp
    gui/widgets/workarea/ProgressBarPopup.cpp
    gui/widgets/workarea/QueryWidget.cpp
    gui/widgets/workarea/WorkAreaTabBar.cpp
    gui/widgets/workarea/WorkAreaTabWidget.cpp
    gui/widgets/workarea/WelcomeTab.cpp

    # Final scope
    gui/widgets/LogWidget.cpp
    gui/widgets/workarea/QueryHistoryTab.cpp
    gui/MainWindow.cpp

    gui/dialogs/SSHTunnelTab.cpp
    gui/dialogs/SSLTab.cpp
    core/settings/SshSettings.cpp
    core/settings/SslSettings.cpp
    core/settings/ReplicaSetSettings.cpp
    core/mongodb/SshTunnelWorker.cpp

    resources/docutaz.qrc
    gui/resources/gui.qrc
)

# Core sources are compiled once into an OBJECT library shared by the
# application executable and the unit-test target. Building them here (rather
# than harvesting docutaz's object files in src/docutaz-unit-tests) removes the
# fragile, platform-specific object-file scraping and lets the tests link on
# every platform, Linux included. An OBJECT library (not STATIC) is used so the
# AUTORCC-generated Qt resource initializers are linked directly into each
# consumer instead of being dropped by the linker as unreferenced archive
# members. Usage requirements are PUBLIC so both consumers inherit them.
add_library(docutaz_core OBJECT ${SOURCES})

target_link_libraries(docutaz_core
    PUBLIC
        Qt6::Widgets
        Qt6::Network
        Qt6::Xml
        Qt6::PrintSupport
        qscintilla
        mongo::mongocxx_shared
        mongo::bsoncxx_shared
        # ssl/crypto are the imported targets created by our own
        # cmake/FindOpenSSL.cmake. We deliberately do NOT use the standard
        # OpenSSL::SSL/OpenSSL::Crypto targets: those only exist when the
        # mongo-c-driver pulls OpenSSL transitively (Linux), but not on macOS
        # (Secure Transport) or Windows (SChannel).
        ssl
        crypto
        ssh
        Threads::Threads)   # Target from FindThreads CMake module

if(APPLE)
  find_library(SECURITY NAMES Security)
  find_library(CORE_FOUNDATION NAMES CoreFoundation)
  target_link_libraries(docutaz_core
    PUBLIC
        ${SECURITY}
        ${CORE_FOUNDATION}
        -lresolv)
endif(APPLE)

target_include_directories(docutaz_core
    PUBLIC
        ${CMAKE_HOME_DIRECTORY}/src
        ${CMAKE_HOME_DIRECTORY}/src/mongo_compat)

# Application executable: the entry point plus the shared core objects.
add_executable(docutaz MACOSX_BUNDLE WIN32 app/main.cpp)
target_link_libraries(docutaz PRIVATE docutaz_core)

# Get build number
find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE BUILD_NUMBER   
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )    
else(GIT_FOUND)
    MESSAGE( FATAL_ERROR "Unable to find Git" )
    set(BUILD_NUMBER 0)
endif(GIT_FOUND)

if(BUILD_NUMBER STREQUAL "")
   MESSAGE( FATAL_ERROR "Failed to get BUILD_NUMBER" )
endif()

set(BUILD_NUMBER ${BUILD_NUMBER} CACHE INTERNAL "")

target_compile_definitions(docutaz_core
    PUBLIC
        BUILDING_ROBO
        PROJECT_NAME="${PROJECT_NAME}"
        PROJECT_NAME_TITLE="${PROJECT_NAME_TITLE}"
        PROJECT_COPYRIGHT="${PROJECT_COPYRIGHT}"
        PROJECT_DOMAIN="${PROJECT_DOMAIN}"
        PROJECT_COMPANYNAME="${PROJECT_COMPANYNAME}"
        PROJECT_COMPANYNAME_DOMAIN="${PROJECT_COMPANYNAME_DOMAIN}"
        PROJECT_GITHUB_FORK="${PROJECT_GITHUB_FORK}"
        PROJECT_GITHUB_ISSUES="${PROJECT_GITHUB_ISSUES}"
        PROJECT_GITHUB_REPO="${PROJECT_GITHUB_REPO}"
        PROJECT_VERSION="${PROJECT_VERSION}"
        PROJECT_VERSION_SHORT="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
        BUILD_NUMBER="${BUILD_NUMBER}"
        PROJECT_NAME_LOWERCASE="${PROJECT_NAME_LOWERCASE}"
        PROJECT_QT_VERSION="${Qt6_VERSION}"
        MongoDB_VERSION="8.0"
        OPENSSL_VERSION="${OPENSSL_VERSION}"
        LIBSSH2_VERSION="${LIBSSH2_VERSION}"
        GOOGLE_TEST_VERSION="${GOOGLE_TEST_VERSION}"
)

if(SYSTEM_WINDOWS)
    # Create Windows Resource file
    set(windows_icon "${CMAKE_SOURCE_DIR}/install/windows/docutaz.ico")
    configure_file(
        "${CMAKE_SOURCE_DIR}/install/windows/winres.rc.in"
        "${CMAKE_BINARY_DIR}/winres.rc")
    unset(windows_icon)

    # Add additional source file to "docutaz" target
    target_sources(docutaz PRIVATE "${CMAKE_BINARY_DIR}/winres.rc")

    # Rename output name for Windows
    set_target_properties(docutaz PROPERTIES OUTPUT_NAME "docutaz")

    # Suppress "warning C4291: ...no matching operator delete found".
    # (The old C4477 sprintf warning is gone now that ptimeutil uses snprintf.)
    # These warnings come from the core sources, which now live in docutaz_core.
    set_target_properties(docutaz_core PROPERTIES COMPILE_FLAGS "/wd4477 /wd4291")

    # Start debug program with console
    set_target_properties(docutaz PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")

    # Qt6 renamed the Qt5 "WinMain" helper to the EntryPoint module, which
    # supplies WinMain() for the /SUBSYSTEM:WINDOWS (GUI) executable and
    # forwards it to main(). Not every Qt distribution ships EntryPoint's CMake
    # package config (notably some aqtinstall layouts: the .lib is present but
    # lib/cmake/Qt6EntryPoint/ is not), so we cannot REQUIRE it. Probe quietly;
    # link the imported target when available, otherwise fall back to the CRT's
    # own entry point (mainCRTStartup), which calls main() directly and removes
    # the need for Qt's WinMain shim entirely.
    find_package(Qt6 QUIET COMPONENTS EntryPoint)
    if(TARGET Qt6::EntryPoint)
        target_link_libraries(docutaz PRIVATE Qt6::EntryPoint)
    else()
        message(STATUS "Qt6::EntryPoint not found; using CRT entry point (/ENTRY:mainCRTStartup)")
        target_link_options(docutaz PRIVATE "/ENTRY:mainCRTStartup")
    endif()

elseif(SYSTEM_MACOSX)
    string(TIMESTAMP CURRENT_YEAR "%Y")
    set_target_properties(docutaz PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        OUTPUT_NAME "Docutaz"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/install/macosx/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Docutaz"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.docutaz.docutaz"
        MACOSX_BUNDLE_INFO_STRING "Docutaz ${PROJECT_VERSION}. Copyright Docutaz, 2026 - ${CURRENT_YEAR}"
        MACOSX_BUNDLE_SHORT_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright Docutaz, 2026 - ${CURRENT_YEAR}"
        MACOSX_BUNDLE_ICON_FILE "docutaz")

elseif(SYSTEM_LINUX)
    set_target_properties(docutaz PROPERTIES
        BUILD_RPATH "/usr/local/lib64"
        INSTALL_RPATH "$ORIGIN/../lib"
        OUTPUT_NAME "docutaz")
    # DT_RPATH (not DT_RUNPATH) propagates to transitive deps so libmongoc2/libbson2
    # are found without LD_LIBRARY_PATH or system ldconfig changes.
    target_link_options(docutaz PRIVATE "LINKER:--disable-new-dtags")
endif()

# Install
include(DocutazInstall)

# Tests and shell targets are excluded from the default build (EXCLUDE_FROM_ALL)
# and reference old MongoDB driver — kept here as stubs for reference only.
