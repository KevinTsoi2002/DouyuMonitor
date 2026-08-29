if(NOT DEFINED RUNTIME_DIR)
    message(FATAL_ERROR "RUNTIME_DIR is required")
endif()
if(NOT EXISTS "${RUNTIME_DIR}")
    message(FATAL_ERROR "Runtime directory was not found: ${RUNTIME_DIR}")
endif()

set(FORBIDDEN_RUNTIME_FILE_NAMES
    Qt6Widgets.dll
    Qt6Widgetsd.dll
    Qt6OpenGLWidgets.dll
    Qt6OpenGLWidgetsd.dll
    Qt6WebEngineCore.dll
    Qt6WebEngineCored.dll
    Qt6WebEngine.dll
    Qt6WebEngined.dll
    Qt6WebEngineQuick.dll
    Qt6WebEngineQuickd.dll
    Qt6WebEngineWidgets.dll
    Qt6WebEngineWidgetsd.dll
    QtWebEngineProcess.exe
    QtWebEngineProcessd.exe
    electron.exe
    chrome.exe
    chrome.dll
    node.exe
)

file(GLOB_RECURSE RUNTIME_FILES LIST_DIRECTORIES false "${RUNTIME_DIR}/*")
foreach(runtime_file IN LISTS RUNTIME_FILES)
    get_filename_component(runtime_name "${runtime_file}" NAME)
    list(FIND FORBIDDEN_RUNTIME_FILE_NAMES "${runtime_name}" forbidden_index)
    if(NOT forbidden_index EQUAL -1)
        file(REMOVE "${runtime_file}")
    endif()
endforeach()
