add_subdirectory(lib/sounds)

option(BUILD_tests "Build the test applications" ON)
if(BUILD_tests)
    add_subdirectory(app/sound-tester/sound-tester)
    add_subdirectory(app/sound-display/sound-display)
endif()
