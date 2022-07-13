string(TOLOWER ${PROJECT_NAME} _lower_project_name)
set(TARGET_NAME ${_lower_project_name}_sound_display)

set(${TARGET_NAME}_HEADERS
)

set(${TARGET_NAME}_SOURCES
)

add_executable(${TARGET_NAME}
    main.cpp
    ${${TARGET_NAME}_SOURCES}
    ${${TARGET_NAME}_HEADERS}
)

find_package(Sounds CONFIG REQUIRED COMPONENTS sounds soundui)
find_package(Math CONFIG REQUIRED COMPONENTS math)
find_package(Graphics CONFIG REQUIRED COMPONENTS graphics imguiui)
find_package(implot REQUIRED)
find_package(spdlog REQUIRED)


#target_compile_options(${TARGET_NAME}
#    PRIVATE "-O2")

#target_compile_options(${TARGET_NAME}
#     PRIVATE "-O2" "--save-temps")

# target_compile_options(${TARGET_NAME}
#      PRIVATE "-g" "-fsanitize=leak" "-Wall" "-Wextra" "-fsanitize=address" "-fsanitize=undefined" "--target=x86_64-unknown-unknown" "-S")
# target_link_options(${TARGET_NAME}
#      PRIVATE "-fsanitize=address" "-fsanitize=leak" "-fsanitize=undefined")


target_link_libraries(${TARGET_NAME}
    PUBLIC
        ad::sounds
        ad::soundui
        ad::math
        ad::graphics
        ad::imguiui

        spdlog::spdlog
        implot::implot
)

set_target_properties(${TARGET_NAME} PROPERTIES
                      VERSION "${${PROJECT_NAME}_VERSION}"
)


##
## Install
##

install(TARGETS ${TARGET_NAME})

