cmake_minimum_required(VERSION 3.20)

project(nvidia_bios_reader VERSION 0.2.0 LANGUAGES CXX)

option(NVBR_BUILD_GUI "Build the native Windows GUI" ON)

add_library(nvbr_core STATIC
    src/nvbios_reader.cpp
)

target_include_directories(nvbr_core PUBLIC include)
target_compile_features(nvbr_core PUBLIC cxx_std_20)
target_compile_definitions(nvbr_core PUBLIC NVBR_VERSION="${PROJECT_VERSION}")

add_executable(nvidia-bios-reader-cli
    src/cli.cpp
)

target_link_libraries(nvidia-bios-reader-cli PRIVATE nvbr_core)

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
