string(TOLOWER ${PROJECT_NAME} _lower_project_name)
set(TARGET_NAME ${_lower_project_name}_sound_display)

set(${TARGET_NAME}_HEADERS
)

set(${TARGET_NAME}_SOURCES
)

set(${TARGET_NAME}_IMGUI_BACKENDS
    ${PROJECT_BINARY_DIR}/conan_imports/imgui_backends/imgui_impl_glfw.cpp
    ${PROJECT_BINARY_DIR}/conan_imports/imgui_backends/imgui_impl_opengl3.cpp
    ${PROJECT_BINARY_DIR}/conan_imports/imgui_backends/imgui_impl_glfw.h
    ${PROJECT_BINARY_DIR}/conan_imports/imgui_backends/imgui_impl_opengl3.h
    ${PROJECT_BINARY_DIR}/conan_imports/imgui_backends/imgui_impl_opengl3_loader.h
)

add_executable(${TARGET_NAME}
    main.cpp
    ${${TARGET_NAME}_SOURCES}
    ${${TARGET_NAME}_HEADERS}
    ${${TARGET_NAME}_IMGUI_BACKENDS}
)

target_include_directories(${TARGET_NAME}
    PRIVATE
        ${PROJECT_BINARY_DIR}/conan_imports
)

find_package(imgui REQUIRED)
find_package(implot REQUIRED)

find_package(Graphics CONFIG REQUIRED COMPONENTS graphics)
find_package(Math CONFIG REQUIRED COMPONENTS math)


#target_compile_options(${TARGET_NAME}
#    PRIVATE "-O2")

 target_compile_options(${TARGET_NAME}
     PRIVATE "-g" "-fsanitize=leak" "-Wall" "-Wextra" "-fsanitize=address" "-fsanitize=undefined")
 target_link_options(${TARGET_NAME}
     PRIVATE "-fsanitize=address" "-fsanitize=leak" "-fsanitize=undefined")

find_package(spdlog REQUIRED)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        ad::sounds
        ad::math
        ad::graphics

        spdlog::spdlog
        imgui::imgui
        implot::implot
)

set_target_properties(${TARGET_NAME} PROPERTIES
                      VERSION "${${PROJECT_NAME}_VERSION}"
)


##
## Install
##

install(TARGETS ${TARGET_NAME})

