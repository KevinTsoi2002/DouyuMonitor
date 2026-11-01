if(NOT DEFINED PACKAGE_DIR)
    message(FATAL_ERROR "PACKAGE_DIR is required")
endif()
if(NOT DEFINED NATIVE_ROOT)
    get_filename_component(NATIVE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
if(NOT IS_DIRECTORY "${PACKAGE_DIR}")
    message(FATAL_ERROR "StreamGet package directory was not found: ${PACKAGE_DIR}")
endif()

set(_manifest_path "${PACKAGE_DIR}/build-provenance.json")
if(NOT EXISTS "${_manifest_path}")
    message(FATAL_ERROR "StreamGet package provenance manifest is missing")
endif()
if(NOT EXISTS "${PACKAGE_DIR}/streamget_service.exe")
    message(FATAL_ERROR "StreamGet package executable is missing")
endif()
file(READ "${_manifest_path}" _manifest)
string(JSON _schema ERROR_VARIABLE _json_error GET "${_manifest}" schemaVersion)
if(_json_error OR NOT _schema EQUAL 1)
    message(FATAL_ERROR "StreamGet package provenance manifest has an unsupported schema")
endif()

set(_expected_inputs
    service/streamget_service.py
    service/protocol.py
    service/douyu_backend.py
    service/requirements.txt
    service/requirements-build.txt
    service/requirements-lock-win-py313.txt
    service/verify_dependency_lock.py
    service/package_provenance.py
    scripts/bootstrap-streamget-service.ps1
    scripts/build-streamget-service.ps1
    app/resources/hamster_agent_roster.json
)
string(JSON _input_count ERROR_VARIABLE _json_error LENGTH "${_manifest}" inputs)
list(LENGTH _expected_inputs _expected_input_count)
if(_json_error OR NOT _input_count EQUAL _expected_input_count)
    message(FATAL_ERROR "StreamGet package provenance input list is incomplete")
endif()

foreach(_relative_path IN LISTS _expected_inputs)
    string(JSON _expected_hash ERROR_VARIABLE _json_error
        GET "${_manifest}" inputs "${_relative_path}")
    if(_json_error)
        message(FATAL_ERROR "StreamGet package provenance is missing input: ${_relative_path}")
    endif()
    set(_source_path "${NATIVE_ROOT}/${_relative_path}")
    if(NOT EXISTS "${_source_path}")
        message(FATAL_ERROR "StreamGet provenance source was not found: ${_source_path}")
    endif()
    file(SHA256 "${_source_path}" _actual_hash)
    string(TOLOWER "${_expected_hash}" _expected_hash)
    string(TOLOWER "${_actual_hash}" _actual_hash)
    if(NOT _actual_hash STREQUAL _expected_hash)
        message(FATAL_ERROR "StreamGet provenance source hash mismatch: ${_relative_path}")
    endif()
endforeach()

string(JSON _file_count ERROR_VARIABLE _json_error LENGTH "${_manifest}" files)
if(_json_error OR _file_count LESS 1)
    message(FATAL_ERROR "StreamGet package provenance file list is empty or invalid")
endif()

file(GLOB_RECURSE _package_files LIST_DIRECTORIES false RELATIVE "${PACKAGE_DIR}"
    "${PACKAGE_DIR}/*")
set(_verified_file_count 0)
foreach(_relative_path IN LISTS _package_files)
    file(TO_CMAKE_PATH "${_relative_path}" _relative_path)
    if(_relative_path STREQUAL "build-provenance.json")
        continue()
    endif()
    string(JSON _expected_hash ERROR_VARIABLE _json_error
        GET "${_manifest}" files "${_relative_path}" sha256)
    if(_json_error)
        message(FATAL_ERROR "StreamGet package has an unlisted file: ${_relative_path}")
    endif()
    file(SHA256 "${PACKAGE_DIR}/${_relative_path}" _actual_hash)
    string(TOLOWER "${_expected_hash}" _expected_hash)
    string(TOLOWER "${_actual_hash}" _actual_hash)
    if(NOT _actual_hash STREQUAL _expected_hash)
        message(FATAL_ERROR "StreamGet package file hash mismatch: ${_relative_path}")
    endif()
    math(EXPR _verified_file_count "${_verified_file_count} + 1")
endforeach()
if(NOT _verified_file_count EQUAL _file_count)
    message(FATAL_ERROR "StreamGet package file count differs from its provenance manifest")
endif()

message(STATUS "Verified StreamGet package provenance: ${PACKAGE_DIR}")
