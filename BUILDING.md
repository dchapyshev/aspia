Instructions for building the project
=====================================
1. Download and install [Visual Studio Community 2017](https://www.visualstudio.com/downloads).
   **Desktop development with C++** workload should be selected when installing.
2. Download and install [CMake](https://cmake.org/download).
3. Download pre-compiled [third-party libraries](https://aspia.org/files/third_party_small.7z).
   If you want to compile these libraries yourself, then you must follow the instructions for compiling these libraries.
4. Extract the archive with third-party libraries (for example to "C:\third_party")
5. Add an environment variable named **ASPIA_THIRD_PARTY_DIR**. For the value, specify a directory with third-party libraries.
6. Download Aspia source code (for example to "C:\aspia").
7. Go to the source directory and run the following commands:
   **<br/>mkdir build
   <br/>cd build
   <br/>cmake ..\source -G "Visual Studio 15 2017"**

   You can also use CMake GUI for these purposes.
   <br/>After these actions, the **aspia.sln** file will be generated in directory "build".
8. Open **aspia.sln** in Visual Studio and build the project.

Alternative instructions (SW build system)
=====================================
1. Download, unpack and add to PATH `sw` tool from https://github.com/SoftwareNetwork/binaries
2. Run `sw build` in the project root.
   Binaries will be available under `.sw` directory.
3. To generate VS solution, run `sw generate`.