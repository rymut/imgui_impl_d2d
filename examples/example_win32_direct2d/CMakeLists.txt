project(example_win32_direct2d LANGUAGES CXX)

set(IMGUI_BACKENDS "${IMGUI_IMPL_D2D_BACKENDS_ROOT}/imgui_impl_win32.cpp" "${IMGUI_IMPL_D2D_BACKENDS_ROOT}/imgui_impl_win32.h") 

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE main.cpp ${IMGUI_BACKENDS})
target_include_directories(${PROJECT_NAME} PRIVATE "${IMGUI_IMPL_D2D_BACKENDS_ROOT}")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/backends")
target_link_libraries(${PROJECT_NAME} PUBLIC imgui_impl_d2d)
