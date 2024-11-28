from conans import ConanFile, tools
from conan.tools.cmake import CMake
from conan.tools.files import copy

from os import path
from os import getcwd


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
        ("spdlog/1.13.0"),
        ("openal/1.22.2"),
        ("zlib/1.3"),
        ("stb/cci.20230920"),
        ("implot/0.16"),
        ("imgui/1.89.8"),

        ("graphics/826ea9d282@adnn/develop"),
        ("handy/e2b164a804@adnn/develop"),
        ("math/8c49b882e7@adnn/develop"),
    )

    build_policy = "missing"
    generators = "CMakeDeps", "CMakeToolchain"

    scm = {
        "type": "git",
        "url": "auto",
        "revision": "auto",
        "submodule": "recursive",
    }

    python_requires="shred_conan_base/0.0.5@adnn/stable"
    python_requires_extend="shred_conan_base.ShredBaseConanFile"
