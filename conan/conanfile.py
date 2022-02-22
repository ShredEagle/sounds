from conans import ConanFile, tools
from conan.tools.cmake import CMake

from os import path


class SoundsConan(ConanFile):
    name = "sounds"
    license = "MIT"
    author = "FranzPoize"
    url = "https://github.com/Shreadeagle/sounds"
    description = "Sound manager and player using OpenAL and OggVorbis"
    topics = ("openAL", "sounds", "ogg", "vorbis")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "build_tests": [True, False],
    }
    default_options = {
        "shared": False,
        "build_tests": False,
    }

    requires = (
        ("boost/1.77.0"),
        ("spdlog/1.9.2"),
        ("openal/1.21.1"),
        ("vorbis/1.3.7"),

        ("graphics/092da6bc60@adnn/develop"),
        ("math/fd9b30cce0@adnn/develop"),
    )

    build_policy = "missing"
    generators = "cmake_paths", "cmake_find_package_multi", "CMakeToolchain"

    scm = {
        "type": "git",
        "url": "auto",
        "revision": "auto",
        "submodule": "recursive",
    }


    def _generate_cmake_configfile(self):
        """ Generates a conanuser_config.cmake file which includes the file generated by """
        """ cmake_paths generator, and forward the remaining options to CMake. """
        with open("conanuser_config.cmake", "w") as config:
            config.write("message(STATUS \"Including user generated conan config.\")\n")
            # avoid path.join, on Windows it outputs '\', which is a string escape sequence.
            config.write("include(\"{}\")\n".format("${CMAKE_CURRENT_LIST_DIR}/conan_paths.cmake"))
            config.write("set({} {})\n".format("BUILD_tests", self.options.build_tests))


    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.configure()
        return cmake


    def configure(self):
        tools.check_min_cppstd(self, "17")


    def generate(self):
           self._generate_cmake_configfile()


    def build(self):
        cmake = self._configure_cmake()
        cmake.build()


    def package(self):
        cmake = self._configure_cmake()
        cmake.install()


    def package_info(self):
        pass
