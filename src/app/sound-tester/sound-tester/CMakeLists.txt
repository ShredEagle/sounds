string(TOLOWER ${PROJECT_NAME} _lower_project_name)
set(TARGET_NAME ${_lower_project_name}_sound_tester)

set(${TARGET_NAME}_HEADERS
)

set(${TARGET_NAME}_SOURCES
)

add_executable(${TARGET_NAME}
    main.cpp
    ${${TARGET_NAME}_SOURCES}
    ${${TARGET_NAME}_HEADERS})



#target_compile_options(${TARGET_NAME}
#    PRIVATE "-O2")
target_compile_options(${TARGET_NAME}
    PRIVATE "-g" "-Wall")

find_package(spdlog REQUIRED)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        ad::sounds

        spdlog::spdlog
)

set_target_properties(${TARGET_NAME} PROPERTIES
                      VERSION "${${PROJECT_NAME}_VERSION}"
)


##
## Install
##

install(TARGETS ${TARGET_NAME})

