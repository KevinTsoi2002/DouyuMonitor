if(NOT DEFINED RUNTIME_DIR)
    message(FATAL_ERROR "RUNTIME_DIR is required")
endif()
if(NOT DEFINED RUNTIME_EXE)
    message(FATAL_ERROR "RUNTIME_EXE is required")
endif()
if(NOT EXISTS "${RUNTIME_EXE}")
    message(FATAL_ERROR "Release executable was not found: ${RUNTIME_EXE}")
endif()

set(REQUIRED_RUNTIME_FILES
    Qt6Core.dll
    Qt6Gui.dll
    Qt6Qml.dll
    Qt6Quick.dll
    mpv.dll
)
foreach(runtime_file IN LISTS REQUIRED_RUNTIME_FILES)
    if(NOT EXISTS "${RUNTIME_DIR}/${runtime_file}")
        message(FATAL_ERROR "Required runtime file is missing: ${runtime_file}")
    endif()
endforeach()
if(NOT EXISTS "${RUNTIME_DIR}/streamget_service/streamget_service.exe"
   AND NOT EXISTS "${RUNTIME_DIR}/streamget_service.exe")
    message(FATAL_ERROR "Required StreamGet service executable is missing")
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
file(GLOB_RECURSE RUNTIME_FILES RELATIVE "${RUNTIME_DIR}" LIST_DIRECTORIES false
    "${RUNTIME_DIR}/*")
foreach(runtime_file IN LISTS RUNTIME_FILES)
    get_filename_component(runtime_name "${runtime_file}" NAME)
    list(FIND FORBIDDEN_RUNTIME_FILE_NAMES "${runtime_name}" forbidden_index)
    if(NOT forbidden_index EQUAL -1)
        message(FATAL_ERROR "Forbidden runtime file is present: ${runtime_file}")
    endif()
endforeach()

file(GLOB RUNTIME_QML_MODULES LIST_DIRECTORIES true "${RUNTIME_DIR}/qml/*")
if(NOT RUNTIME_QML_MODULES)
    message(FATAL_ERROR "No QML modules were deployed under ${RUNTIME_DIR}/qml")
endif()

find_program(DUMPBIN_EXECUTABLE NAMES dumpbin.exe dumpbin REQUIRED)
execute_process(
    COMMAND "${DUMPBIN_EXECUTABLE}" /DEPENDENTS "${RUNTIME_EXE}"
    RESULT_VARIABLE dumpbin_result
    OUTPUT_VARIABLE dumpbin_output
    ERROR_VARIABLE dumpbin_error
)
if(NOT dumpbin_result EQUAL 0)
    message(FATAL_ERROR "dumpbin /DEPENDENTS failed: ${dumpbin_error}")
endif()

string(TOLOWER "${dumpbin_output}" dependency_output)
foreach(required_dependency IN ITEMS qt6core qt6gui qt6qml qt6quick mpv.dll)
    string(FIND "${dependency_output}" "${required_dependency}" dependency_index)
    if(dependency_index EQUAL -1)
        message(FATAL_ERROR "Required executable dependency is missing: ${required_dependency}")
    endif()
endforeach()
foreach(forbidden_dependency IN ITEMS qt6widgets qt6webengine electron.exe chrome.dll node.exe)
    string(FIND "${dependency_output}" "${forbidden_dependency}" dependency_index)
    if(NOT dependency_index EQUAL -1)
        message(FATAL_ERROR "Forbidden executable dependency found: ${forbidden_dependency}")
    endif()
endforeach()
