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
        ("spdlog/1.9.2"),
        ("openal/1.22.2"),
        ("zlib/1.2.12"),
        ("stb/cci.20210910"),
        ("implot/0.13"),

        ("graphics/8a162f896f@adnn/develop"),
        ("handy/35f0a507af@adnn/develop"),
        ("math/662cc47fad@adnn/develop"),
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
