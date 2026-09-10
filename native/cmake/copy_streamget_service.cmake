if(NOT DEFINED SERVICE_SOURCE OR NOT DEFINED SERVICE_DEST)
    message(FATAL_ERROR "SERVICE_SOURCE and SERVICE_DEST are required")
endif()

if(NOT EXISTS "${SERVICE_SOURCE}")
    message(STATUS "streamget_service package not found; skipping packaged service copy")
    return()
endif()

if(IS_DIRECTORY "${SERVICE_SOURCE}")
    file(COPY "${SERVICE_SOURCE}" DESTINATION "${SERVICE_DEST}")
else()
    file(COPY_FILE "${SERVICE_SOURCE}" "${SERVICE_DEST}" ONLY_IF_DIFFERENT)
endif()
