set(TARGET_NAME sounds)

set(${TARGET_NAME}_HEADERS
    SoundManager.h
    SoundUtilities.h
)

set(${TARGET_NAME}_SOURCES
    SoundManager.cpp
    SoundUtilities.cpp
)

add_library(${TARGET_NAME}
    ${${TARGET_NAME}_SOURCES}
    ${${TARGET_NAME}_HEADERS}
)

source_group(TREE ${CMAKE_CURRENT_LIST_DIR}
             FILES ${${TARGET_NAME}_HEADERS} ${${TARGET_NAME}_SOURCES}
)

add_library(ad::${TARGET_NAME} ALIAS ${TARGET_NAME})

cmc_target_current_include_directory(${TARGET_NAME})


##
## Dependencies
##

find_package(Graphics CONFIG REQUIRED COMPONENTS arte graphics resource)
find_package(Math CONFIG REQUIRED COMPONENTS math)

find_package(spdlog REQUIRED COMPONENTS spdlog)
find_package(OpenAL REQUIRED CONFIG)
find_package(Vorbis REQUIRED CONFIG)
find_package(Ogg REQUIRED CONFIG)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        ad::math
        ad::graphics
        ad::resource

        spdlog::spdlog
        OpenAL::OpenAL
        Vorbis::vorbisfile
)

##
## Install
##

install(TARGETS ${TARGET_NAME} EXPORT ${TARGET_NAME}Targets)

include(cmc-install)
# Install the header files
cmc_install_with_folders(DESTINATION include/${TARGET_NAME}/${TARGET_NAME}
                         FILES ${${TARGET_NAME}_HEADERS})
# Prepare CMake package in both build and install trees
cmc_install_packageconfig(${TARGET_NAME} ${TARGET_NAME}Targets ${PROJECT_NAME}
                          FIND_FILE CMakeFinds.cmake.in
                          NAMESPACE ad::)
