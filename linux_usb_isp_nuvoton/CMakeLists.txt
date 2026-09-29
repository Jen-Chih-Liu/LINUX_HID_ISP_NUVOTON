cmake_minimum_required(VERSION 3.16)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(linux_usb_isp_nuvoton LANGUAGES C CXX)
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(FORCE_FETCH_LIBUSB "Force downloading libusb-1.0 via FetchContent" OFF)

set(LIBUSB_FOUND FALSE)

if(NOT FORCE_FETCH_LIBUSB)
  # 1. Try FindPackage config / module if available (e.g. vcpkg or installed cmake package)
  find_package(libusb QUIET)
  if(TARGET libusb::usb-1.0)
    message(STATUS "Found libusb::usb-1.0 target")
    set(LIBUSB_TARGET libusb::usb-1.0)
    set(LIBUSB_FOUND TRUE)
  elseif(TARGET libusb-1.0)
    message(STATUS "Found libusb-1.0 target")
    set(LIBUSB_TARGET libusb-1.0)
    set(LIBUSB_FOUND TRUE)
  endif()

  # 2. Try PkgConfig (standard on Linux: pkg-config --cflags --libs libusb-1.0)
  if(NOT LIBUSB_FOUND)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PKG_LIBUSB QUIET IMPORTED_TARGET libusb-1.0)
      if(TARGET PkgConfig::PKG_LIBUSB)
        message(STATUS "Found system libusb-1.0 via pkg-config")
        set(LIBUSB_TARGET PkgConfig::PKG_LIBUSB)
        set(LIBUSB_FOUND TRUE)
      endif()
    endif()
  endif()

  # 3. Try standard find_path / find_library
  if(NOT LIBUSB_FOUND)
    find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES libusb-1.0)
    find_library(LIBUSB_LIBRARY NAMES usb-1.0 libusb-1.0)
    if(LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARY)
      message(STATUS "Found system libusb-1.0 library: ${LIBUSB_LIBRARY}")
      add_library(libusb_system UNKNOWN IMPORTED)
      set_target_properties(libusb_system PROPERTIES
        IMPORTED_LOCATION "${LIBUSB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUSB_INCLUDE_DIR}"
      )
      set(LIBUSB_TARGET libusb_system)
      set(LIBUSB_FOUND TRUE)
    endif()
  endif()
endif()

# If not found or forced, fetch and build libusb-1.0 statically
if(NOT LIBUSB_FOUND)
  message(STATUS "libusb-1.0 not found on system (or FORCE_FETCH_LIBUSB enabled). Using FetchContent to build libusb-1.0...")
  include(FetchContent)
  set(LIBUSB_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
  set(LIBUSB_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(LIBUSB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(LIBUSB_BUILD_TESTS OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    libusb_cmake
    GIT_REPOSITORY https://github.com/libusb/libusb-cmake.git
    GIT_TAG        v1.0.30-0
  )
  FetchContent_MakeAvailable(libusb_cmake)
  set(LIBUSB_TARGET usb-1.0)
endif()

add_executable(linux_usb_isp_nuvoton
  linux_usb_isp_nuvoton.cpp
)

target_link_libraries(linux_usb_isp_nuvoton PRIVATE ${LIBUSB_TARGET})

target_include_directories(linux_usb_isp_nuvoton PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
  target_compile_definitions(linux_usb_isp_nuvoton PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
  )
endif()

# -------------------------------------------------------------
# Cppcheck & MISRA static analysis (Pre-build check)
# -------------------------------------------------------------
option(ENABLE_CPPCHECK "Run Cppcheck with MISRA analysis before compilation" ON)
if(ENABLE_CPPCHECK)
  find_program(CPPCHECK_EXECUTABLE
    NAMES cppcheck
    PATHS
      "C:/Program Files/Cppcheck"
      "C:/Program Files (x86)/Cppcheck"
  )

  find_file(MISRA_JSON_PATH "misra.json"
    PATHS
      "${CMAKE_CURRENT_SOURCE_DIR}/.."
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_SOURCE_DIR}"
      "${CMAKE_SOURCE_DIR}/LINUX_HID_ISP_NUVOTON"
    NO_DEFAULT_PATH
  )

  if(CPPCHECK_EXECUTABLE AND MISRA_JSON_PATH)
    message(STATUS "Cppcheck found: ${CPPCHECK_EXECUTABLE}")
    message(STATUS "MISRA configuration found: ${MISRA_JSON_PATH}")

    set(CPPCHECK_EXTRA_FLAGS "")
    if(WIN32)
      list(APPEND CPPCHECK_EXTRA_FLAGS "-D_WIN32")
    endif()

    add_custom_target(run_cppcheck
      COMMAND "${CPPCHECK_EXECUTABLE}"
        "--addon=${MISRA_JSON_PATH}"
        "--suppress=missingIncludeSystem"
        "--suppress=misra-config"
        "--inline-suppr"
        "--template={file}({line}): {severity}: {message} [{id}]"
        ${CPPCHECK_EXTRA_FLAGS}
        "-I${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/linux_usb_isp_nuvoton.cpp"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
      COMMENT "Running Cppcheck (MISRA: ${MISRA_JSON_PATH}) before compiling..."
      VERBATIM
    )

    # Ensure run_cppcheck executes BEFORE linux_usb_isp_nuvoton compiles
    add_dependencies(linux_usb_isp_nuvoton run_cppcheck)
  else()
    if(NOT CPPCHECK_EXECUTABLE)
      message(STATUS "Cppcheck executable not found, skipping static analysis.")
    endif()
    if(NOT MISRA_JSON_PATH)
      message(STATUS "misra.json not found, skipping MISRA check.")
    endif()
  endif()
endif()

install(TARGETS linux_usb_isp_nuvoton
  RUNTIME DESTINATION bin
)
