if(NOT DEFINED QT_DEPLOY_DIR)
    message(FATAL_ERROR "QT_DEPLOY_DIR is required")
endif()
if(NOT DEFINED QT_PLATFORM_PLUGIN)
    message(FATAL_ERROR "QT_PLATFORM_PLUGIN is required")
endif()
if(NOT DEFINED QT_CORE_DLL)
    message(FATAL_ERROR "QT_CORE_DLL is required")
endif()

set(QT_PLATFORM_DIR "${QT_DEPLOY_DIR}/platforms")
if(NOT EXISTS "${QT_DEPLOY_DIR}/${QT_CORE_DLL}")
    message(FATAL_ERROR "Expected Qt core DLL ${QT_CORE_DLL} was not deployed under ${QT_DEPLOY_DIR}.")
endif()

if(NOT EXISTS "${QT_PLATFORM_DIR}/${QT_PLATFORM_PLUGIN}")
    message(FATAL_ERROR
        "Expected Qt Windows platform plugin ${QT_PLATFORM_PLUGIN} was not deployed under "
        "${QT_PLATFORM_DIR}; run the matching windeployqt configuration before launching it.")
endif()

if(QT_PLATFORM_PLUGIN STREQUAL "qwindowsd.dll")
    set(QT_STALE_PLATFORM_PLUGIN "qwindows.dll")
else()
    set(QT_STALE_PLATFORM_PLUGIN "qwindowsd.dll")
endif()

if(EXISTS "${QT_PLATFORM_DIR}/${QT_STALE_PLATFORM_PLUGIN}")
    message(FATAL_ERROR
        "Qt deployment contains both ${QT_PLATFORM_PLUGIN} and ${QT_STALE_PLATFORM_PLUGIN}; "
        "Debug and Release platform plugins must not be mixed.")
endif()
