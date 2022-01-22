Instructions for building the project
=====================================

Windows
-------
1. You must use Windows 10/11 x64 to build the project. Build in other versions of Windows is not guaranteed.
2. Download and install [Visual Studio Community 2019](https://www.visualstudio.com/downloads).

   2.1. **Desktop development with C++** workload should be selected when installing.

   2.2. **SDK 10.0.18362.0** should be selected when installing.

   2.3. **ATL/MFC** libraries should be selected when installing.

   2.4. **English language pack** (required for vcpkg).

3. Download and install [CMake](https://cmake.org/download) (version >= 3.17.0).
4. Download and install [vcpkg](https://github.com/dchapyshev/vcpkg) (forked from Microsoft repository).
5. In vcpkg, you need to install the following libraries (use triplet x86-windows-static in all cases):
* asio
* gtest
* libvpx
* libyuv
* openssl
* opus
* protobuf
* qt5-base
* qt5-translations
* qt5-winextras
* rapidjson
* sqlite3
* zstd
6. Go to the directory with source code (root directory) and run the following commands:
   **<br/>mkdir build
   <br/>cd build
   <br/>cmake ..\ -G "Visual Studio 16 2019" -A Win32 -DCMAKE_TOOLCHAIN_FILE=<vcpkg_path>\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x86-windows-static**
   <br/>(replace <vcpkg_path> with real path to vcpkg)

   You can also use CMake GUI for these purposes.
   <br/>After these actions, the **aspia.sln** file will be generated in directory "build".
7. Open **aspia.sln** in Visual Studio and build the project.

Linux
-----
The build for Linux was tested only in Ubuntu 20.04 and Debian 10.3. If you have a different distribution kit or its version, then additional steps may be required.
1. Install the following packages in your package manager (**packages must be installed before installing vcpkg and its packages**):
* ninja-build
* autoconf
* autopoint
* python
* libgl1-mesa-dev
* libglu1-mesa-dev
* libharfbuzz-dev
* libfontconfig1-dev
* libfreetype6-dev
* libx11-dev
* libx11-xcb-dev
* libxext-dev
* libxfixes-dev
* libxi-dev
* libxrender-dev
* libxcb1-dev
* libxcb-glx0-dev
* libxcb-keysyms1-dev
* libxcb-image0-dev
* libxcb-shm0-dev
* libxcb-icccm4-dev
* libxcb-sync0-dev
* libxcb-xfixes0-dev
* libxcb-shape0-dev
* libxcb-randr0-dev
* libxcb-render-util0-dev
* libxcb-xinerama-dev
* libxkbcommon-dev
* libxkbcommon-x11-dev
* libatspi2.0-dev
* libprocps-dev
* libxdamage-dev
* libxrandr-dev
* libpulse-dev
* flite1-dev
* libspeechd-dev
* speech-dispatcher
* nasm
* gcc
* g++
* qtcreator
2. In vcpkg, you need to install the following libraries (use triplet x64-linux in all cases):
* asio
* gtest
* icu
* libvpx
* libyuv
* openssl
* opus
* protobuf
* qt5-base
* qt5-translations
* rapidjson
* sqlite3
* zstd
3. Open **QtCreator -> Tools -> Options -> Kits -> Qt Versions**. Click the Add button and specify the path to **<vcpkg_path>/installed/x64-linux/tools/qt5/bin/qmake**.
4. Open **QtCreator -> Tools -> Options -> Kits -> Kits**. Click the Add button. Enter a display name for the profile, specify the compilers (gcc/g++), and the Qt profile you added earlier.
5. Open **CMakeLists.txt** from the Aspia root directory in QtCreator and configure the build using the previously added profile.

Alternative instructions (SW build system)
------------------------------------------
1. Download, unpack and add to PATH `sw` tool from https://github.com/SoftwareNetwork/binaries
2. Run `sw build source` in the project root.
   Binaries will be available under `.sw` directory.
3. To generate VS solution, run `sw generate source`.
