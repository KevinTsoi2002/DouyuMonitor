if(NOT DEFINED SERVICE_SOURCE OR NOT DEFINED SERVICE_DEST)
    message(FATAL_ERROR "SERVICE_SOURCE and SERVICE_DEST are required")
endif()

if(NOT EXISTS "${SERVICE_SOURCE}")
    message(FATAL_ERROR "streamget_service package was not found: ${SERVICE_SOURCE}")
endif()
if(NOT IS_DIRECTORY "${SERVICE_SOURCE}")
    message(FATAL_ERROR "StreamGet package must be an onedir directory with provenance")
endif()

get_filename_component(NATIVE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(PACKAGE_DIR "${SERVICE_SOURCE}")
include("${CMAKE_CURRENT_LIST_DIR}/verify_streamget_service_package.cmake")

file(COPY "${SERVICE_SOURCE}" DESTINATION "${SERVICE_DEST}")

set(PACKAGE_DIR "${SERVICE_DEST}/streamget_service")
include("${CMAKE_CURRENT_LIST_DIR}/verify_streamget_service_package.cmake")
