Instructions for building the project
=====================================
1. You must use Windows 10 x64 to build the project. Build in other versions of Windows is not guaranteed.
2. Download and install [Visual Studio Community 2017](https://www.visualstudio.com/downloads).

   2.1. **Desktop development with C++** workload should be selected when installing.

   2.2. **SDK 10.0.17763.0 and 8.1** should be selected when installing.

3. Download and install [CMake](https://cmake.org/download).
4. Download and install [vcpkg](https://github.com/dchapyshev/vcpkg).
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
* qt5-winextras (only for Windows)
* rapidjson
* sqlite3
* libwebm
* zstd
6. Go to the source directory and run the following commands:
   **<br/>mkdir build
   <br/>cd build
   <br/>cmake ..\ -G "Visual Studio 15 2017" -DCMAKE_TOOLCHAIN_FILE=<vcpkg_path>\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x86-windows-static**
   <br/>(replace <vcpkg_path> with real path to vcpkg)

   You can also use CMake GUI for these purposes.
   <br/>After these actions, the **aspia.sln** file will be generated in directory "build".
7. Open **aspia.sln** in Visual Studio and build the project.

Alternative instructions (SW build system)
==========================================
1. Download, unpack and add to PATH `sw` tool from https://github.com/SoftwareNetwork/binaries
2. Run `sw build source` in the project root.
   Binaries will be available under `.sw` directory.
3. To generate VS solution, run `sw generate source`.
