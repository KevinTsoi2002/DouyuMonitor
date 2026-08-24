cmake_minimum_required(VERSION 3.24)

project(DouyuMonitorNative VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

set(QT_ROOT "$ENV{QT_ROOT}" CACHE PATH "Qt SDK root")
if(QT_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
endif()

find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets OpenGLWidgets Test)

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

option(DOUYU_BUILD_PLAYER_SURFACE_TEST "Build the PlayerSurface test" ON)
if(DOUYU_BUILD_PLAYER_SURFACE_TEST)
    add_executable(player_surface_test
        tests/player_surface_test.cpp
        src/media/media_source.cpp
        src/media/player_surface.cpp
    )

    target_link_libraries(player_surface_test
        PRIVATE
            Qt6::Widgets
            Qt6::OpenGLWidgets
            Qt6::Test
            mpv::mpv
    )

    target_include_directories(player_surface_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    add_custom_command(TARGET player_surface_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MPV_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:player_surface_test>/libmpv-2.dll"
    )

    add_test(NAME player_surface_test COMMAND player_surface_test)
endif()

option(DOUYU_BUILD_MAIN_WINDOW_TEST "Build the MainWindow test" ON)
if(DOUYU_BUILD_MAIN_WINDOW_TEST)
    add_executable(main_window_test
        tests/main_window_test.cpp
        src/app/main_window.cpp
        src/media/media_source.cpp
        src/media/player_surface.cpp
    )

    target_link_libraries(main_window_test
        PRIVATE
            Qt6::Widgets
            Qt6::OpenGLWidgets
            Qt6::Test
            mpv::mpv
    )

    target_include_directories(main_window_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    add_custom_command(TARGET main_window_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MPV_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:main_window_test>/libmpv-2.dll"
    )

    add_test(NAME main_window_test COMMAND main_window_test)
endif()

add_executable(douyu_monitor_native
    app/main.cpp
    src/app/main_window.cpp
    src/media/media_source.cpp
    src/media/player_surface.cpp
)

target_link_libraries(douyu_monitor_native
    PRIVATE
        Qt6::Widgets
        Qt6::OpenGLWidgets
        mpv::mpv
)

target_include_directories(douyu_monitor_native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_custom_command(TARGET douyu_monitor_native POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:douyu_monitor_native>/libmpv-2.dll"
)

if(WIN32 AND EXISTS "${QT_ROOT}/bin/windeployqt.exe")
    add_custom_command(TARGET douyu_monitor_native POST_BUILD
        COMMAND "${QT_ROOT}/bin/windeployqt.exe"
            --no-translations
            --no-compiler-runtime
            "$<TARGET_FILE:douyu_monitor_native>"
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
        "$<TARGET_FILE_DIR:mpv_dependency_probe>/libmpv-2.dll"
)
add_test(NAME mpv_dependency_probe COMMAND mpv_dependency_probe)
