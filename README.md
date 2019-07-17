Cloning
-------

To clone this repository with full history and external libraries (~350 MB):

    git clone --recursive https://github.com/iTzMeRafa/Quadrace-mod

To clone this repository with full history when you have the necessary libraries on your system already (~220 MB):

    git clone https://github.com/iTzMeRafa/Quadrace-mod

To clone this repository with history since we moved the libraries to https://github.com/iTzMeRafa/Quadrace-mod (~40 MB):

    git clone --shallow-exclude=included-libs https://github.com/iTzMeRafa/Quadrace-mod

To clone the libraries if you have previously cloned Quadrace without them:

    git submodule update --init --recursive

Building on Linux and macOS
---------------------------

To compile Quadrace yourself, execute the following commands in the source root:

    mkdir build
    cd build
    cmake ..
    make

Quadrace requires additional libraries, that are bundled for the most common platforms (Windows, Mac, Linux, all x86 and x86\_64). The bundled libraries are now in the quadrace-libs submodule.

You can install the required libraries on your system, `touch CMakeLists.txt` and CMake will use the system-wide libraries by default. You can install all required dependencies and CMake on Debian or Ubuntu like this:

    sudo apt install cmake git libcurl4-openssl-dev libfreetype6-dev libglew-dev libogg-dev libopus-dev libopusfile-dev libpnglite-dev libsdl2-dev libwavpack-dev python

Or on Arch Linux like this (Arch Linux does not package `pnglite`, not even in AUR):

    sudo pacman -S --needed cmake curl freetype2 git glew opusfile sdl2 wavpack python

If you have the libraries installed, but still want to use the bundled ones instead, you can do so by removing your build directory and re-running CMake with `-DPREFER_BUNDLED_LIBS=ON`, e.g. `cmake -DPREFER_BUNDLED_LIBS=ON ..`.

MySQL (or MariaDB) support in the server is not included in the binary releases but it can be built by specifying `-DMYSQL=ON`, like `cmake -DMYSQL=ON ..`. It requires `libmariadbclient-dev`, `libmysqlcppconn-dev` and `libboost-dev`, which are also bundled for the common platforms.

Note that the bundled MySQL libraries might not work properly on your system. If you run into connection problems with the MySQL server, for example that it connects as root while you chose another user, make sure to install your system libraries for the MySQL client and C++ connector. Make sure that the CMake configuration summary says that it found MySQL libs that were not bundled (no "using bundled libs").

Running tests (Debian/Ubuntu)
-----------------------------

In order to run the tests, you need to install the following library `libgtest-dev`.

This library isn't compiled, so you have to do it:
```bash
sudo apt install libgtest-dev
cd /usr/src/gtest
sudo cmake CMakeLists.txt
sudo make
 
# copy or symlink libgtest.a and libgtest_main.a to your /usr/lib folder
sudo cp *.a /usr/lib
```

To run the tests you must target `run_tests` with make:
`make run_tests`
