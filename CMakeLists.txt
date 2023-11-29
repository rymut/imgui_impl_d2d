cmake_minimum_required(VERSION 3.20)

# conan 2.0 required settings
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_CXX_STANDARD 14)
set(CONAN_HOST_PROFILE "auto-cmake")

# download conan provider
set(CONAN_PROVIDER "${CMAKE_BINARY_DIR}/conan_provider.cmake")
set(CONAN_PROVIDER_GITHUB "rymut/cmake-conan")
if(NOT EXISTS "${CONAN_PROVIDER}")
  message(STATUS "Downloading conan_provider.cmake from https://github.com/${CONAN_PROVIDER_GITHUB}")
  file(DOWNLOAD "https://raw.githubusercontent.com/${CONAN_PROVIDER_GITHUB}/develop2/conan_provider.cmake"
                "${CONAN_PROVIDER}"
                TLS_VERIFY ON)
endif()

# configure options
option(IMGUI_IMPL_D2D_BUILD_TESTS "Build imgui_backend_d2d tests" OFF)
option(IMGUI_IMPL_D2D_BUILD_EXAMPLES "Build imgui_backend_d2d examples" ON)
option(IMGUI_IMPL_D2D_BUILD_SHARED_LIBS "Build imgui_backend_d2d as shared library" OFF)

project(imgui_impl_d2d LANGUAGES CXX)
include("${CONAN_PROVIDER}")
find_package(ImGui REQUIRED)

if (NOT IMGUI_IMPL_D2D_BACKENDS_ROOT)
    get_property(IMGUI_IMPL_D2D_BACKENDS_ROOT GLOBAL PROPERTY CONAN_GENERATORS_FOLDER)
    if (IMGUI_IMPL_D2D_BACKENDS_ROOT)
        set(IMGUI_IMPL_D2D_BACKENDS_ROOT "${IMGUI_IMPL_D2D_BACKENDS_ROOT}/../backends")
    else()
        set(IMGUI_IMPL_D2D_BACKENDS_ROOT ".")
    endif()
    option(IMGUI_IMPL_D2D_BACKENDS_ROOT "ImGUI backends root" "${IMGUI_IMPL_D2D_BACKENDS_ROOT}")
endif()

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC "backends/imgui_impl_d2d.h" PRIVATE "backends/imgui_impl_d2d.cpp")
target_link_libraries(${PROJECT_NAME} PUBLIC imgui::imgui)
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER $<TARGET_PROPERTY:${PROJECT_NAME},INTERFACE_SOURCES>)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/imgui/backends)

if (IMGUI_IMPL_D2D_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
