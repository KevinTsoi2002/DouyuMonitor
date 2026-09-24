cmake_minimum_required(VERSION 3.24)

project(DouyuMonitorNative VERSION 0.2.10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

set(QT_ROOT "$ENV{QT_ROOT}" CACHE PATH "Qt SDK root")
if(QT_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
endif()

find_package(Qt6 CONFIG REQUIRED COMPONENTS
    Gui
    OpenGL
    Qml
    Quick
    QuickControls2
    WebSockets
    Network
    Test
    QuickTest
)

set(MPV_ROOT "$ENV{MPV_ROOT}" CACHE PATH "libmpv SDK root")
find_path(MPV_INCLUDE_DIR NAMES mpv/client.h PATHS "${MPV_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
find_library(MPV_LIBRARY NAMES libmpv mpv PATHS "${MPV_ROOT}" "${MPV_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
set(MPV_RUNTIME_DLL "${MPV_ROOT}/libmpv-2.dll")
if(NOT EXISTS "${MPV_RUNTIME_DLL}")
    message(FATAL_ERROR "libmpv runtime DLL not found: ${MPV_RUNTIME_DLL}")
endif()

add_library(mpv::mpv SHARED IMPORTED GLOBAL)
set_target_properties(mpv::mpv PROPERTIES
    IMPORTED_IMPLIB "${MPV_LIBRARY}"
    IMPORTED_LOCATION "${MPV_RUNTIME_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${MPV_INCLUDE_DIR}"
)

enable_testing()

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set(DOUYU_QML_ICON_ASSETS
    app/qml/assets/douyu_monitor.svg
    app/qml/assets/icons/hamster-agent.jfif
    app/qml/assets/icons/activity.svg
    app/qml/assets/icons/chevron-down.svg
    app/qml/assets/icons/chevron-up.svg
    app/qml/assets/icons/ellipsis.svg
    app/qml/assets/icons/layout-grid.svg
    app/qml/assets/icons/menu.svg
    app/qml/assets/icons/message-circle.svg
    app/qml/assets/icons/minus.svg
    app/qml/assets/icons/pin.svg
    app/qml/assets/icons/plus.svg
    app/qml/assets/icons/settings.svg
    app/qml/assets/icons/square.svg
    app/qml/assets/icons/star.svg
    app/qml/assets/icons/volume-2.svg
    app/qml/assets/icons/x.svg
    app/qml/assets/icons/window-maximize.svg
    app/qml/assets/icons/window-fullscreen.svg
    app/qml/assets/icons/window-minimize.svg
    app/qml/assets/icons/window-close.svg
)

add_library(douyu_guild STATIC
    src/workspace/guild_roster.cpp
)
target_link_libraries(douyu_guild PUBLIC Qt6::Core)
target_include_directories(douyu_guild PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
qt_add_resources(douyu_guild guild_roster_resources
    PREFIX "/guild"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        app/resources/hamster_agent_roster.json
)

add_executable(guild_roster_test
    tests/guild_roster_test.cpp
)
target_link_libraries(guild_roster_test PRIVATE
    Qt6::Core
    Qt6::Test
    douyu_guild
)
add_test(NAME guild_roster_test COMMAND guild_roster_test)
set_property(TEST guild_roster_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
)

add_executable(guild_room_resolver_test
    tests/guild_room_resolver_test.cpp
    src/workspace/guild_room_resolver.cpp
)
target_link_libraries(guild_room_resolver_test PRIVATE
    Qt6::Core
    Qt6::Test
    douyu_guild
)
target_include_directories(guild_room_resolver_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME guild_room_resolver_test COMMAND guild_room_resolver_test)
set_property(TEST guild_room_resolver_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
)

qt_add_library(douyu_qml STATIC)
set_source_files_properties(app/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(douyu_qml
    URI DouyuMonitor
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/DouyuMonitor"
    QML_FILES
        app/qml/Theme.qml
        app/qml/Main.qml
        app/qml/components/AppHeader.qml
        app/qml/components/WindowControls.qml
        app/qml/components/RoomSidebar.qml
        app/qml/components/GuildNavigationPanel.qml
        app/qml/components/GuildMemberRow.qml
        app/qml/components/RoomLibraryView.qml
        app/qml/components/WorkspaceGrid.qml
        app/qml/components/PrimaryRoomDivider.qml
        app/qml/components/RoomTile.qml
        app/qml/components/DanmakuOverlay.qml
        app/qml/components/DanmakuLine.qml
        app/qml/components/DanmakuLaneScheduler.js
        app/qml/components/ToastViewport.qml
        app/qml/components/WorkspaceStatusBar.qml
        app/qml/panels/DanmakuSettingsPanel.qml
        app/qml/panels/MonitoringStatusPanel.qml
        app/qml/panels/WorkspacePresetsPanel.qml
        app/qml/pages/SettingsPage.qml
        app/qml/dialogs/AddRoomDialog.qml
        app/qml/dialogs/NotificationSettingsDialog.qml
        app/qml/dialogs/CloseBehaviorDialog.qml
        app/qml/dialogs/TeamManagerDialog.qml
    RESOURCES
        ${DOUYU_QML_ICON_ASSETS}
)
target_link_libraries(douyu_qml PRIVATE
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

add_executable(qml_engine_smoke_test
    tests/qml_engine_smoke_test.cpp
    src/ui/mpv_quick_item.cpp
)
target_link_libraries(qml_engine_smoke_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::OpenGL
    Qt6::Test
    douyu_qml
    douyu_qmlplugin
    remote_playback
    mpv::mpv
)
qt_import_qml_plugins(qml_engine_smoke_test)
target_include_directories(qml_engine_smoke_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
qt_add_resources(qml_engine_smoke_test qml_engine_test_resources
    PREFIX "/qml"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/app/qml"
    FILES
        app/qml/qmldir
        app/qml/Theme.qml
        app/qml/Main.qml
        app/qml/components/AppHeader.qml
        app/qml/components/WindowControls.qml
        app/qml/components/RoomSidebar.qml
        app/qml/components/GuildNavigationPanel.qml
        app/qml/components/GuildMemberRow.qml
        app/qml/components/RoomLibraryView.qml
        app/qml/components/WorkspaceGrid.qml
        app/qml/components/PrimaryRoomDivider.qml
        app/qml/components/RoomTile.qml
        app/qml/components/DanmakuOverlay.qml
        app/qml/components/DanmakuLine.qml
        app/qml/components/DanmakuLaneScheduler.js
        app/qml/components/ToastViewport.qml
        app/qml/components/WorkspaceStatusBar.qml
        app/qml/panels/DanmakuSettingsPanel.qml
        app/qml/panels/MonitoringStatusPanel.qml
        app/qml/panels/WorkspacePresetsPanel.qml
        app/qml/pages/SettingsPage.qml
        app/qml/dialogs/AddRoomDialog.qml
        app/qml/dialogs/NotificationSettingsDialog.qml
        app/qml/dialogs/CloseBehaviorDialog.qml
        app/qml/dialogs/TeamManagerDialog.qml
        ${DOUYU_QML_ICON_ASSETS}
)
add_custom_command(TARGET qml_engine_smoke_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:qml_engine_smoke_test>/mpv.dll"
)
add_test(NAME qml_engine_smoke_test COMMAND qml_engine_smoke_test)
set_property(TEST qml_engine_smoke_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QT_QPA_PLATFORM=set:offscreen"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
    "QT_QUICK_CONTROLS_STYLE=set:Basic"
)

add_executable(qml_visual_smoke_test
    tests/qml_visual_smoke_test.cpp
    src/ui/mpv_quick_item.cpp
)
target_link_libraries(qml_visual_smoke_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::OpenGL
    Qt6::Test
    douyu_qml
    douyu_qmlplugin
    remote_playback
    mpv::mpv
)
qt_import_qml_plugins(qml_visual_smoke_test)
target_include_directories(qml_visual_smoke_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(qml_visual_smoke_test PRIVATE
    QML_VERIFICATION_DIR="${CMAKE_CURRENT_SOURCE_DIR}/out/verification/qml"
)
qt_add_resources(qml_visual_smoke_test qml_visual_smoke_test_resources
    PREFIX "/qml"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/app/qml"
    FILES
        app/qml/qmldir
        app/qml/Theme.qml
        app/qml/Main.qml
        app/qml/components/AppHeader.qml
        app/qml/components/WindowControls.qml
        app/qml/components/RoomSidebar.qml
        app/qml/components/GuildNavigationPanel.qml
        app/qml/components/GuildMemberRow.qml
        app/qml/components/RoomLibraryView.qml
        app/qml/components/WorkspaceGrid.qml
        app/qml/components/PrimaryRoomDivider.qml
        app/qml/components/RoomTile.qml
        app/qml/components/DanmakuOverlay.qml
        app/qml/components/DanmakuLine.qml
        app/qml/components/DanmakuLaneScheduler.js
        app/qml/components/ToastViewport.qml
        app/qml/components/WorkspaceStatusBar.qml
        app/qml/panels/DanmakuSettingsPanel.qml
        app/qml/panels/MonitoringStatusPanel.qml
        app/qml/panels/WorkspacePresetsPanel.qml
        app/qml/pages/SettingsPage.qml
        app/qml/dialogs/AddRoomDialog.qml
        app/qml/dialogs/NotificationSettingsDialog.qml
        app/qml/dialogs/CloseBehaviorDialog.qml
        app/qml/dialogs/TeamManagerDialog.qml
        ${DOUYU_QML_ICON_ASSETS}
)
add_custom_command(TARGET qml_visual_smoke_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:qml_visual_smoke_test>/mpv.dll"
)
add_test(NAME qml_visual_smoke_test COMMAND qml_visual_smoke_test)
set_property(TEST qml_visual_smoke_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
    "QT_QUICK_CONTROLS_STYLE=set:Basic"
)

add_executable(qml_interaction_test
    tests/qml_interaction_test.cpp
    src/ui/mpv_quick_item.cpp
    src/ui/room_list_model.cpp
    src/ui/workspace_model.cpp
)
target_link_libraries(qml_interaction_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::OpenGL
    Qt6::Test
    remote_playback
    mpv::mpv
)
target_include_directories(qml_interaction_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
qt_add_resources(qml_interaction_test qml_interaction_test_resources
    PREFIX "/qml"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/app/qml"
    FILES
        app/qml/qmldir
        app/qml/Theme.qml
        app/qml/Main.qml
        app/qml/components/AppHeader.qml
        app/qml/components/WindowControls.qml
        app/qml/components/RoomSidebar.qml
        app/qml/components/GuildNavigationPanel.qml
        app/qml/components/GuildMemberRow.qml
        app/qml/components/RoomLibraryView.qml
        app/qml/components/WorkspaceGrid.qml
        app/qml/components/PrimaryRoomDivider.qml
        app/qml/components/RoomTile.qml
        app/qml/components/DanmakuOverlay.qml
        app/qml/components/DanmakuLine.qml
        app/qml/components/DanmakuLaneScheduler.js
        app/qml/components/ToastViewport.qml
        app/qml/components/WorkspaceStatusBar.qml
        app/qml/panels/DanmakuSettingsPanel.qml
        app/qml/panels/MonitoringStatusPanel.qml
        app/qml/panels/WorkspacePresetsPanel.qml
        app/qml/pages/SettingsPage.qml
        app/qml/dialogs/AddRoomDialog.qml
        app/qml/dialogs/NotificationSettingsDialog.qml
        app/qml/dialogs/CloseBehaviorDialog.qml
        app/qml/dialogs/TeamManagerDialog.qml
        ${DOUYU_QML_ICON_ASSETS}
)
add_custom_command(TARGET qml_interaction_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:qml_interaction_test>/mpv.dll"
)
add_test(NAME qml_interaction_test COMMAND qml_interaction_test)
set_property(TEST qml_interaction_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
    "QT_QPA_PLATFORM=set:offscreen"
    "QT_QUICK_CONTROLS_STYLE=set:Basic"
)

add_executable(qml_close_regression_test
    tests/qml_close_regression_test.cpp
    src/ui/app_controller.cpp
    src/workspace/guild_room_resolver.cpp
    src/ui/room_list_model.cpp
    src/ui/workspace_model.cpp
    src/ui/monitoring_model.cpp
    src/ui/mpv_quick_item.cpp
    src/app/windows_notification_service.cpp
    src/app/update_checker.cpp
    src/app/windows_tray_service.cpp
    src/workspace/quality_policy.cpp
    src/workspace/native_workspace_types.cpp
    src/workspace/native_workspace_store.cpp
    src/workspace/notification_policy.cpp
    src/workspace/room_session.cpp
    src/workspace/room_status_scheduler.cpp
    src/workspace/multi_room_coordinator.cpp
    src/workspace/favorite_monitor.cpp
    src/danmaku/danmaku_controller.cpp
)
target_link_libraries(qml_close_regression_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::OpenGL
    Qt6::Test
    douyu_danmaku
    douyu_guild
    remote_playback
    mpv::mpv
)
target_include_directories(qml_close_regression_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(qml_close_regression_test PRIVATE
    DOUYU_TESTING
    FAKE_STREAMGET_SERVICE_PATH="$<TARGET_FILE:fake_streamget_service>"
)
add_dependencies(qml_close_regression_test fake_streamget_service)
qt_add_resources(qml_close_regression_test qml_close_regression_test_resources
    PREFIX "/qml"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/app/qml"
    FILES
        app/qml/qmldir
        app/qml/Theme.qml
        app/qml/Main.qml
        app/qml/components/AppHeader.qml
        app/qml/components/WindowControls.qml
        app/qml/components/RoomSidebar.qml
        app/qml/components/GuildNavigationPanel.qml
        app/qml/components/GuildMemberRow.qml
        app/qml/components/RoomLibraryView.qml
        app/qml/components/WorkspaceGrid.qml
        app/qml/components/PrimaryRoomDivider.qml
        app/qml/components/RoomTile.qml
        app/qml/components/DanmakuOverlay.qml
        app/qml/components/DanmakuLine.qml
        app/qml/components/DanmakuLaneScheduler.js
        app/qml/components/ToastViewport.qml
        app/qml/components/WorkspaceStatusBar.qml
        app/qml/panels/DanmakuSettingsPanel.qml
        app/qml/panels/MonitoringStatusPanel.qml
        app/qml/panels/WorkspacePresetsPanel.qml
        app/qml/pages/SettingsPage.qml
        app/qml/dialogs/AddRoomDialog.qml
        app/qml/dialogs/NotificationSettingsDialog.qml
        app/qml/dialogs/CloseBehaviorDialog.qml
        app/qml/dialogs/TeamManagerDialog.qml
        ${DOUYU_QML_ICON_ASSETS}
)
add_custom_command(TARGET qml_close_regression_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:qml_close_regression_test>/mpv.dll"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:qml_close_regression_test>/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_ROOT}/plugins/platforms/qoffscreen.dll"
        "$<TARGET_FILE_DIR:qml_close_regression_test>/platforms/qoffscreen.dll"
)
add_test(NAME qml_close_regression_test COMMAND qml_close_regression_test)
set_property(TEST qml_close_regression_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
    "QT_QPA_PLATFORM=set:offscreen"
    "QT_QUICK_CONTROLS_STYLE=set:Basic"
)

add_executable(qml_danmaku_overlay_test
    tests/qml_danmaku_overlay_test.cpp
)
target_link_libraries(qml_danmaku_overlay_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_add_resources(qml_danmaku_overlay_test qml_danmaku_overlay_test_resources
    PREFIX "/qml"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/app/qml"
    FILES
        app/qml/components/DanmakuOverlay.qml
        app/qml/components/DanmakuLine.qml
        app/qml/components/DanmakuLaneScheduler.js
)
add_test(NAME qml_danmaku_overlay_test COMMAND qml_danmaku_overlay_test)
set_property(TEST qml_danmaku_overlay_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
    "QT_QPA_PLATFORM=set:offscreen"
    "QT_QUICK_CONTROLS_STYLE=set:Basic"
)

add_executable(room_list_model_test
    tests/room_list_model_test.cpp
    src/ui/room_list_model.cpp
)
target_link_libraries(room_list_model_test PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(room_list_model_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME room_list_model_test COMMAND room_list_model_test)

add_executable(workspace_model_test
    tests/workspace_model_test.cpp
    src/ui/workspace_model.cpp
    src/ui/monitoring_model.cpp
)
target_link_libraries(workspace_model_test PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(workspace_model_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME workspace_model_test COMMAND workspace_model_test)

add_executable(update_checker_test
    tests/update_checker_test.cpp
    src/app/update_checker.cpp
)
target_link_libraries(update_checker_test PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
)
target_include_directories(update_checker_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME update_checker_test COMMAND update_checker_test)
set_property(TEST update_checker_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
)

add_executable(app_controller_test
    tests/app_controller_test.cpp
    src/ui/app_controller.cpp
    src/workspace/guild_room_resolver.cpp
    src/ui/room_list_model.cpp
    src/ui/workspace_model.cpp
    src/ui/monitoring_model.cpp
    src/ui/mpv_quick_item.cpp
    src/app/windows_notification_service.cpp
    src/app/update_checker.cpp
    src/app/windows_tray_service.cpp
    src/workspace/quality_policy.cpp
    src/workspace/native_workspace_types.cpp
    src/workspace/native_workspace_store.cpp
    src/workspace/notification_policy.cpp
    src/workspace/room_session.cpp
    src/workspace/room_status_scheduler.cpp
    src/workspace/multi_room_coordinator.cpp
    src/workspace/favorite_monitor.cpp
    src/danmaku/danmaku_controller.cpp
)
target_link_libraries(app_controller_test PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::OpenGL
    Qt6::Network
    Qt6::Test
    douyu_danmaku
    douyu_guild
    remote_playback
    mpv::mpv
)
target_include_directories(app_controller_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(app_controller_test PRIVATE
    DOUYU_TESTING
    DOUYU_APP_VERSION="${PROJECT_VERSION}"
    FAKE_STREAMGET_SERVICE_PATH="$<TARGET_FILE:fake_streamget_service>"
)
add_dependencies(app_controller_test fake_streamget_service)
add_custom_command(TARGET app_controller_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:app_controller_test>/mpv.dll"
)
add_test(NAME app_controller_test COMMAND app_controller_test)
set_property(TEST app_controller_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
)

add_executable(mpv_quick_item_test
    tests/mpv_quick_item_test.cpp
    src/ui/mpv_quick_item.cpp
)
target_link_libraries(mpv_quick_item_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::OpenGL
    Qt6::Test
    remote_playback
    mpv::mpv
)
target_include_directories(mpv_quick_item_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(mpv_quick_item_test PRIVATE DOUYU_TESTING)
add_custom_command(TARGET mpv_quick_item_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:mpv_quick_item_test>/mpv.dll"
)
add_test(NAME mpv_quick_item_test COMMAND mpv_quick_item_test)
set_property(TEST mpv_quick_item_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
)

add_library(stream_service STATIC
    src/service/stream_service_protocol.cpp
    src/service/streamget_process_client.cpp
)
target_link_libraries(stream_service PUBLIC Qt6::Core)
target_include_directories(stream_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(remote_playback STATIC
    src/media/media_source.cpp
    src/media/remote_playback_controller.cpp
)
target_link_libraries(remote_playback PUBLIC stream_service Qt6::Core)
target_include_directories(remote_playback PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(stream_service_protocol_test
    tests/stream_service_protocol_test.cpp
)
target_link_libraries(stream_service_protocol_test PRIVATE stream_service Qt6::Core Qt6::Test)
target_include_directories(stream_service_protocol_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME stream_service_protocol_test COMMAND stream_service_protocol_test)

add_executable(douyu_danmaku_protocol_test
    tests/douyu_danmaku_protocol_test.cpp
    src/danmaku/douyu_danmaku_protocol.cpp
)
target_link_libraries(douyu_danmaku_protocol_test PRIVATE Qt6::Core Qt6::Test)
target_include_directories(douyu_danmaku_protocol_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME douyu_danmaku_protocol_test COMMAND douyu_danmaku_protocol_test)
set_tests_properties(douyu_danmaku_protocol_test PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_ROOT}/bin"
)

add_executable(danmaku_governance_test
    tests/danmaku_governance_test.cpp
    src/danmaku/danmaku_types.cpp
    src/danmaku/danmaku_governance.cpp
)
target_link_libraries(danmaku_governance_test PRIVATE Qt6::Core Qt6::Test)
target_include_directories(danmaku_governance_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME danmaku_governance_test COMMAND danmaku_governance_test)
set_tests_properties(danmaku_governance_test PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_ROOT}/bin"
)

add_library(douyu_danmaku STATIC
    src/danmaku/douyu_danmaku_protocol.cpp
    src/danmaku/danmaku_types.cpp
    src/danmaku/danmaku_governance.cpp
    src/danmaku/danmaku_socket.cpp
    src/danmaku/danmaku_timer_scheduler.cpp
    src/danmaku/douyu_danmaku_client.cpp
    src/danmaku/danmaku_session_manager.cpp
)
target_link_libraries(douyu_danmaku PUBLIC Qt6::Core Qt6::WebSockets)
target_include_directories(douyu_danmaku PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(douyu_danmaku_client_test
    tests/douyu_danmaku_client_test.cpp
)
target_link_libraries(douyu_danmaku_client_test PRIVATE douyu_danmaku Qt6::Test)
add_test(NAME douyu_danmaku_client_test COMMAND douyu_danmaku_client_test)
set_tests_properties(douyu_danmaku_client_test PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_ROOT}/bin"
)

add_executable(danmaku_session_manager_test
    tests/danmaku_session_manager_test.cpp
)
target_link_libraries(danmaku_session_manager_test PRIVATE douyu_danmaku Qt6::Test)
add_test(NAME danmaku_session_manager_test COMMAND danmaku_session_manager_test)
set_tests_properties(danmaku_session_manager_test PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_ROOT}/bin"
)

add_executable(application_logger_test
    tests/application_logger_test.cpp
    src/app/application_logger.cpp
)
target_link_libraries(application_logger_test PRIVATE Qt6::Core Qt6::Test)
target_include_directories(application_logger_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME application_logger_test COMMAND application_logger_test)
set_tests_properties(application_logger_test PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_ROOT}/bin"
)

add_executable(fake_streamget_service
    tests/fake_streamget_service.cpp
)
target_link_libraries(fake_streamget_service PRIVATE stream_service Qt6::Core)
target_include_directories(fake_streamget_service PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(streamget_process_client_test
    tests/streamget_process_client_test.cpp
)
target_link_libraries(streamget_process_client_test PRIVATE stream_service Qt6::Core Qt6::Test)
target_include_directories(streamget_process_client_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_dependencies(streamget_process_client_test fake_streamget_service)
target_compile_definitions(streamget_process_client_test PRIVATE
    FAKE_STREAMGET_SERVICE_PATH="$<TARGET_FILE:fake_streamget_service>"
)
add_test(NAME streamget_process_client_test COMMAND streamget_process_client_test)

add_executable(remote_playback_controller_test
    tests/remote_playback_controller_test.cpp
)
target_link_libraries(remote_playback_controller_test
    PRIVATE
        remote_playback
        Qt6::Core
        Qt6::Test
)
target_include_directories(remote_playback_controller_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_dependencies(remote_playback_controller_test fake_streamget_service)
target_compile_definitions(remote_playback_controller_test PRIVATE
    FAKE_STREAMGET_SERVICE_PATH="$<TARGET_FILE:fake_streamget_service>"
)
add_test(NAME remote_playback_controller_test COMMAND remote_playback_controller_test)

add_executable(quality_policy_test
    tests/quality_policy_test.cpp
    src/workspace/quality_policy.cpp
)
target_link_libraries(quality_policy_test PRIVATE Qt6::Core Qt6::Test)
target_include_directories(quality_policy_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME quality_policy_test COMMAND quality_policy_test)

add_executable(native_workspace_store_test
    tests/native_workspace_store_test.cpp
    src/workspace/native_workspace_types.cpp
    src/workspace/native_workspace_store.cpp
)
target_link_libraries(native_workspace_store_test PRIVATE douyu_danmaku douyu_guild Qt6::Core Qt6::Test)
target_include_directories(native_workspace_store_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME native_workspace_store_test COMMAND native_workspace_store_test)

add_executable(room_status_scheduler_test
    tests/room_status_scheduler_test.cpp
    src/workspace/room_status_scheduler.cpp
)
target_link_libraries(room_status_scheduler_test PRIVATE Qt6::Core Qt6::Test)
target_include_directories(room_status_scheduler_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME room_status_scheduler_test COMMAND room_status_scheduler_test)

add_executable(notification_policy_test
    tests/notification_policy_test.cpp
    src/workspace/notification_policy.cpp
)
target_link_libraries(notification_policy_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)
target_include_directories(notification_policy_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME notification_policy_test COMMAND notification_policy_test)

add_executable(favorite_monitor_test
    tests/favorite_monitor_test.cpp
    src/workspace/favorite_monitor.cpp
    src/workspace/room_status_scheduler.cpp
    src/workspace/notification_policy.cpp
)
target_link_libraries(favorite_monitor_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)
target_include_directories(favorite_monitor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME favorite_monitor_test COMMAND favorite_monitor_test)
set_property(TEST favorite_monitor_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
)

add_executable(windows_notification_service_test
    tests/windows_notification_service_test.cpp
    src/app/windows_notification_service.cpp
    src/workspace/notification_policy.cpp
)
target_link_libraries(windows_notification_service_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)
target_include_directories(windows_notification_service_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME windows_notification_service_test COMMAND windows_notification_service_test)

add_executable(windows_tray_service_test
    tests/windows_tray_service_test.cpp
    src/app/windows_tray_service.cpp
)
target_link_libraries(windows_tray_service_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
target_include_directories(windows_tray_service_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(windows_tray_service_test PRIVATE DOUYU_TESTING)
add_test(NAME windows_tray_service_test COMMAND windows_tray_service_test)
set_property(TEST windows_tray_service_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
)

add_executable(room_session_test
    tests/room_session_test.cpp
    src/workspace/room_session.cpp
    src/ui/mpv_quick_item.cpp
)
target_link_libraries(room_session_test
    PRIVATE
        remote_playback
        Qt6::Gui
        Qt6::Quick
        Qt6::OpenGL
        Qt6::Test
        mpv::mpv
)
target_include_directories(room_session_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_dependencies(room_session_test fake_streamget_service)
target_compile_definitions(room_session_test PRIVATE
    DOUYU_TESTING
    FAKE_STREAMGET_SERVICE_PATH="$<TARGET_FILE:fake_streamget_service>"
)
add_custom_command(TARGET room_session_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:room_session_test>/mpv.dll"
)
add_test(NAME room_session_test COMMAND room_session_test)
set_property(TEST room_session_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
)

add_executable(multi_room_coordinator_test
    tests/multi_room_coordinator_test.cpp
    src/workspace/multi_room_coordinator.cpp
    src/workspace/room_session.cpp
    src/workspace/room_status_scheduler.cpp
    src/workspace/quality_policy.cpp
    src/ui/mpv_quick_item.cpp
)
target_link_libraries(multi_room_coordinator_test
    PRIVATE
        remote_playback
        Qt6::Gui
        Qt6::Quick
        Qt6::OpenGL
        Qt6::Test
        mpv::mpv
)
target_include_directories(multi_room_coordinator_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_dependencies(multi_room_coordinator_test fake_streamget_service)
target_compile_definitions(multi_room_coordinator_test PRIVATE
    DOUYU_TESTING
    FAKE_STREAMGET_SERVICE_PATH="$<TARGET_FILE:fake_streamget_service>"
)
add_custom_command(TARGET multi_room_coordinator_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:multi_room_coordinator_test>/mpv.dll"
)
add_test(NAME multi_room_coordinator_test COMMAND multi_room_coordinator_test)
set_property(TEST multi_room_coordinator_test PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${QT_ROOT}/bin"
    "QT_PLUGIN_PATH=set:${QT_ROOT}/plugins"
    "QML2_IMPORT_PATH=set:${QT_ROOT}/qml"
    "QT_QPA_PLATFORM=set:offscreen"
)

option(DOUYU_BUILD_SERVICE_TESTS "Register Python StreamGet service tests" ON)
if(DOUYU_BUILD_SERVICE_TESTS)
    if(WIN32)
        set(_douyu_venv_python "${CMAKE_CURRENT_SOURCE_DIR}/.venv/Scripts/python.exe")
    else()
        set(_douyu_venv_python "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin/python")
    endif()
    if(EXISTS "${_douyu_venv_python}")
        set(Python3_EXECUTABLE "${_douyu_venv_python}" CACHE FILEPATH
            "Python interpreter for the StreamGet service tests" FORCE)
    endif()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME streamget_service_python_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s ${CMAKE_CURRENT_SOURCE_DIR}/service/tests -v
        )
        set_tests_properties(streamget_service_python_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/.."
        )
    else()
        message(WARNING "Python3 interpreter not found; service tests are disabled")
    endif()
endif()

option(DOUYU_BUILD_MEDIA_SOURCE_TEST "Build the MediaSource test" ON)
if(DOUYU_BUILD_MEDIA_SOURCE_TEST)
    add_executable(media_source_test
        tests/media_source_test.cpp
        src/media/media_source.cpp
    )

    target_link_libraries(media_source_test
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    target_include_directories(media_source_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    add_test(NAME media_source_test COMMAND media_source_test)
endif()

add_executable(douyu_monitor_native
    app/main.cpp
    src/workspace/guild_room_resolver.cpp
    src/app/application_logger.cpp
    src/media/media_source.cpp
    src/workspace/quality_policy.cpp
    src/workspace/native_workspace_types.cpp
    src/workspace/native_workspace_store.cpp
    src/app/windows_notification_service.cpp
    src/app/update_checker.cpp
    src/app/windows_tray_service.cpp
    src/workspace/notification_policy.cpp
    src/workspace/room_session.cpp
    src/workspace/room_status_scheduler.cpp
    src/workspace/multi_room_coordinator.cpp
    src/workspace/favorite_monitor.cpp
    src/danmaku/danmaku_controller.cpp
    src/ui/mpv_quick_item.cpp
    src/ui/app_controller.cpp
    src/ui/room_list_model.cpp
    src/ui/workspace_model.cpp
    src/ui/monitoring_model.cpp
)

set_target_properties(douyu_monitor_native PROPERTIES
    WIN32_EXECUTABLE TRUE
)

if(WIN32)
    target_sources(douyu_monitor_native PRIVATE app/resources/app_icon.rc)

    add_executable(douyu_monitor_uninstaller WIN32
        app/uninstaller_main.cpp
        app/resources/app_icon.rc
    )
    target_link_libraries(douyu_monitor_uninstaller PRIVATE Shell32 Advapi32 Ole32 User32)
endif()

target_link_libraries(douyu_monitor_native
    PRIVATE
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::OpenGL
        Qt6::Network
        douyu_qml
        douyu_qmlplugin
        douyu_danmaku
        douyu_guild
        remote_playback
        mpv::mpv
)

qt_import_qml_plugins(douyu_monitor_native)

target_include_directories(douyu_monitor_native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(douyu_monitor_native PRIVATE
    DOUYU_APP_VERSION="${PROJECT_VERSION}"
)

if(WIN32)
    target_link_libraries(app_controller_test PRIVATE Shell32 User32)
    target_link_libraries(qml_close_regression_test PRIVATE Shell32 User32)
    target_link_libraries(windows_notification_service_test PRIVATE Shell32 User32)
    target_link_libraries(windows_tray_service_test PRIVATE Shell32 User32)
    target_link_libraries(douyu_monitor_native PRIVATE Shell32 User32)
endif()

add_custom_command(TARGET douyu_monitor_native POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:douyu_monitor_native>/mpv.dll"
    COMMAND "${CMAKE_COMMAND}"
        -DSERVICE_SOURCE:PATH=${CMAKE_CURRENT_SOURCE_DIR}/out/service/streamget_service
        -DSERVICE_DEST:PATH=$<TARGET_FILE_DIR:douyu_monitor_native>
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_streamget_service.cmake"
    VERBATIM
)

if(WIN32 AND EXISTS "${QT_ROOT}/bin/windeployqt.exe")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(DOUYU_QT_DEPLOY_MODE --debug)
        set(DOUYU_QT_PLATFORM_PLUGIN qwindowsd.dll)
        set(DOUYU_QT_STALE_PLATFORM_PLUGIN qwindows.dll)
        set(DOUYU_QT_CORE_DLL Qt6Cored.dll)
    else()
        set(DOUYU_QT_DEPLOY_MODE --release)
        set(DOUYU_QT_PLATFORM_PLUGIN qwindows.dll)
        set(DOUYU_QT_STALE_PLATFORM_PLUGIN qwindowsd.dll)
        set(DOUYU_QT_CORE_DLL Qt6Core.dll)
    endif()
    add_custom_command(TARGET douyu_monitor_native POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E rm -f
            "$<TARGET_FILE_DIR:douyu_monitor_native>/platforms/${DOUYU_QT_STALE_PLATFORM_PLUGIN}"
        COMMAND "${QT_ROOT}/bin/windeployqt.exe"
            ${DOUYU_QT_DEPLOY_MODE}
            --force
            --no-translations
            --no-compiler-runtime
            "$<TARGET_FILE:douyu_monitor_native>"
        COMMAND "${CMAKE_COMMAND}"
            -DRUNTIME_DIR:PATH=$<TARGET_FILE_DIR:douyu_monitor_native>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clean_forbidden_runtime_files.cmake"
        COMMAND "${CMAKE_COMMAND}"
            -DQT_DEPLOY_DIR:PATH=$<TARGET_FILE_DIR:douyu_monitor_native>
            -DQT_PLATFORM_PLUGIN:STRING=${DOUYU_QT_PLATFORM_PLUGIN}
            -DQT_CORE_DLL:STRING=${DOUYU_QT_CORE_DLL}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_qt_windows_deployment.cmake"
        VERBATIM
    )
endif()

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release"
   AND EXISTS "${QT_ROOT}/bin/windeployqt.exe")
    add_custom_command(TARGET douyu_monitor_native POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            -DRUNTIME_DIR:PATH=$<TARGET_FILE_DIR:douyu_monitor_native>
            -DRUNTIME_EXE:FILEPATH=$<TARGET_FILE:douyu_monitor_native>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_runtime_dependencies.cmake"
        VERBATIM
    )
endif()

add_executable(native_self_test_test tests/native_self_test_test.cpp)
target_link_libraries(native_self_test_test PRIVATE Qt6::Core Qt6::Test)
add_dependencies(native_self_test_test douyu_monitor_native)
add_test(NAME native_self_test_media COMMAND native_self_test_test)

add_test(NAME douyu_monitor_native_self_test COMMAND douyu_monitor_native --self-test)

add_executable(mpv_dependency_probe tests/mpv_dependency_probe.cpp)
target_link_libraries(mpv_dependency_probe PRIVATE mpv::mpv)
add_custom_command(TARGET mpv_dependency_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:mpv_dependency_probe>/mpv.dll"
)
add_test(NAME mpv_dependency_probe COMMAND mpv_dependency_probe)

# Qt Test is an SDK-only dependency; keep the deployed application self-test
# isolated while giving standalone Qt Test executables the matching runtime.
set_tests_properties(
    room_list_model_test
    workspace_model_test
    stream_service_protocol_test
    streamget_process_client_test
    remote_playback_controller_test
    quality_policy_test
    native_workspace_store_test
    room_status_scheduler_test
    notification_policy_test
    windows_notification_service_test
    media_source_test
    native_self_test_media
    PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${QT_ROOT}/bin"
)
