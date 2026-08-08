cmake_minimum_required(VERSION 3.20)

project(nvidia_bios_reader
    VERSION 0.4.0
    DESCRIPTION "Read-only NVIDIA VBIOS memory profile, strap, and timing analysis"
    HOMEPAGE_URL "https://github.com/fmuniztriana/nvidia-bios-reader"
    LANGUAGES CXX
)

set(NVBR_VERSION "${PROJECT_VERSION}-beta.1" CACHE STRING
    "User-visible version string (set to the release version for release builds)")

if(WIN32)
    enable_language(RC)
endif()

option(NVBR_BUILD_GUI "Build the native Windows GUI" ON)

add_library(nvbr_core STATIC
    src/nvbios_reader.cpp
)

target_include_directories(nvbr_core PUBLIC include)
target_compile_features(nvbr_core PUBLIC cxx_std_20)
target_compile_definitions(nvbr_core PUBLIC NVBR_VERSION="${NVBR_VERSION}")

add_executable(nvidia-bios-reader-cli
    src/cli.cpp
)

target_link_libraries(nvidia-bios-reader-cli PRIVATE nvbr_core)

if(WIN32)
    set(NVBR_FILE_DESCRIPTION "NVIDIA BIOS Reader command-line interface")
    set(NVBR_INTERNAL_NAME "nvidia-bios-reader-cli")
    set(NVBR_ORIGINAL_FILENAME "nvidia-bios-reader-cli.exe")
    configure_file(resources/version.rc.in cli-version.rc @ONLY)
    target_sources(nvidia-bios-reader-cli PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/cli-version.rc
    )
endif()

if(WIN32 AND NVBR_BUILD_GUI)
    add_executable(nvidia-bios-reader WIN32
        src/gui.cpp
    )
    target_link_libraries(nvidia-bios-reader PRIVATE
        nvbr_core
        comctl32
        comdlg32
        shell32
    )
    set(NVBR_FILE_DESCRIPTION "NVIDIA BIOS Reader graphical interface")
    set(NVBR_INTERNAL_NAME "nvidia-bios-reader")
    set(NVBR_ORIGINAL_FILENAME "nvidia-bios-reader.exe")
    configure_file(resources/version.rc.in gui-version.rc @ONLY)
    target_sources(nvidia-bios-reader PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/gui-version.rc
    )
endif()

function(nvbr_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

nvbr_warnings(nvbr_core)
nvbr_warnings(nvidia-bios-reader-cli)
if(WIN32 AND NVBR_BUILD_GUI)
    nvbr_warnings(nvidia-bios-reader)
endif()

include(CTest)

if(BUILD_TESTING)
    add_test(NAME cli_help COMMAND nvidia-bios-reader-cli --help)
    add_test(NAME cli_version COMMAND nvidia-bios-reader-cli --version)
endif()

include(GNUInstallDirs)
install(TARGETS nvidia-bios-reader-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(WIN32 AND NVBR_BUILD_GUI)
    install(TARGETS nvidia-bios-reader RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
