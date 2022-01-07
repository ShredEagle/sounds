# Sounds

Sounds library with C++, liboggvorbis and openAL

## Development

Build environment setup:

    git clone --recurse-submodules ...
    cd sounds
    mkdir build && cd build
    conan install ../conan --build=missing
    cmake ../
    # Actual build command
    cmake --build ./
