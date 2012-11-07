# LANGMUIR #

This is the source code for the "Langmuir" engine for charge transfer
simulations in molecular transistors.


# BUILD INSTRUCTIONS #

In order to build the Langmuir engine the following dependencies are required,

Qt4
Boost
CMake

The following are optional:
OpenCL 1.1
OpenGL
Doxygen

To build the engine go to the source directory and run the following commands,

# SIMPLE BUILD #

mkdir build
cd build
cmake ../
make -j 4
make install
make doc

# ON MAC #

cmake -DCMAKE_OSX_ARCHITECTURES=i386 ../

# CLANG SCAN-BUILD #

mkdir build
cd build
scan-build -v cmake ..
scan-build -v -k -analyze-headers -stats -o . make -j 4
scan-view scan-build-output-dir

notes:
    scan-build-output-dir will be in the current directory and have the current date for its name