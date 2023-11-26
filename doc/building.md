Instructions for building the project
=====================================

Windows
-------
1. You must use Windows 10/11 x64 to build the project. Build in other versions of Windows is not guaranteed.
2. Download and install [Visual Studio Community 2019](https://www.visualstudio.com/downloads).

   2.1. **Desktop development with C++** workload should be selected when installing.

   2.2. **SDK 10.0.18362.0** should be selected when installing.

   2.3. **ATL/MFC** libraries should be selected when installing.

   2.4. **English language pack** (Required for vcpkg; **Only English language should be installed, without any other**).

3. Download and install [CMake](https://cmake.org/download) (version >= 3.21.0).
4. Download and install [Git](https://git-scm.com/downloads).
5. Download and install [vcpkg4aspia](https://github.com/dchapyshev/vcpkg4aspia) (forked from Microsoft repository). To do this, go to the directory where you want to install and run the commands:
```bash
git clone https://github.com/dchapyshev/vcpkg4aspia.git
cd vcpkg4aspia
./bootstrap-vcpkg.sh
```
6. In vcpkg, you need to install the following libraries (use triplet **x86-windows-static** or **x64-windows-static** in all cases; for example: **./vcpkg install asio:x86-windows-static**):
* asio
* curl
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
7. Go to the directory with source code (root directory) and run the following commands:
```bash
mkdir build
cd build
cmake ..\ -G "Visual Studio 16 2019" -A Win32 -DCMAKE_TOOLCHAIN_FILE=<vcpkg_path>\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=<triplet_name>
```
(replace **<vcpkg_path>** with real path to vcpkg; replace **<triplet_name>** to x86-windows-static or x64-windows-static)

You can also use CMake GUI for these purposes.
<br/>After these actions, the **aspia.sln** file will be generated in directory "build".
8. Open **aspia.sln** in Visual Studio and build the project.

Linux
-----
The build for Linux was tested only in Ubuntu 20.04. If you have a different distribution kit or its version, then additional steps may be required.
1. Install [QtCreator](https://download.qt.io/official_releases/online_installers/).
2. Download and install [vcpkg4aspia](https://github.com/dchapyshev/vcpkg4aspia) (forked from Microsoft repository). To do this, go to the directory where you want to install and run the commands:
```bash
git clone https://github.com/dchapyshev/vcpkg4aspia.git
cd vcpkg4aspia
./bootstrap-vcpkg.sh
```
3. Install the following packages in your package manager (**packages must be installed before installing vcpkg and its packages**):
```
ninja-build
autoconf
autoconf-archive
autopoint
python
bison
gperf
dpkg-dev
libgl1-mesa-dev
libglu1-mesa-dev
libharfbuzz-dev
libfontconfig1-dev
libfreetype6-dev
libx11-dev
libx11-xcb-dev
libxext-dev
libxfixes-dev
libxi-dev
libxrender-dev
libxcb1-dev
libxcb-glx0-dev
libxcb-keysyms1-dev
libxcb-image0-dev
libxcb-shm0-dev
libxcb-icccm4-dev
libxcb-sync-dev
libxcb-xfixes0-dev
libxcb-shape0-dev
libxcb-randr0-dev
libxcb-render-util0-dev
libxcb-xinerama0-dev
libxcb-util-dev
libxcb-cursor0
libxcb-cursor-dev
libxkbcommon-dev
libxkbcommon-x11-dev
libatspi2.0-dev
libprocps-dev
libxdamage-dev
libxrandr-dev
libpulse-dev
flite1-dev
libspeechd-dev
speech-dispatcher
nasm
gcc
g++
git
cmake
curl
```
4. Make sure that the version of CMake in your Linux is greater than or equal to 3.21.0. To do this, run the command:
```
cmake --version
```
If the version does not match, then remove the package. Run the following commands to build the required version of CMake:
```bash
sudo apt-get install libcrypt-dev
sudo apt-get install libcrypt-dev
git clone https://github.com/Kitware/CMake
git checkout tags/v3.27.7
cd CMake
./configure
make -j4
sudo make install
```
5. In vcpkg, you need to install the following libraries (for example: **./vcpkg install asio**):
```
asio
curl
gtest
icu
libvpx
libyuv
openssl
opus
protobuf
qt5-base
qt5-translations
rapidjson
sqlite3
zstd
```
6. Open **QtCreator -> Tools -> Options -> Kits -> Qt Versions**. Click the Add button and specify the path to **<vcpkg_path>/installed/x64-linux/tools/qt5/bin/qmake**.
7. Open **QtCreator -> Tools -> Options -> Kits -> Kits**. Click the Add button. Enter a display name for the profile, specify the compilers (gcc/g++), and the Qt profile you added earlier.
8. Open **CMakeLists.txt** from the Aspia root directory in QtCreator and configure the build using the previously added profile.

MacOS
-----
1. Install **Xcode** from AppStore.
2. Execute command in terminal: **xcode-select --install**
3. Open **Xcode** and agree to install **additional components**.
4. Install [brew](https://brew.sh).
5. Install packages in **brew**:
```bash
brew install cmake
brew install autoconf
brew install autoconf-archive
brew install automake
brew install nasm
brew install pkg-config
brew install wget
brew install curl
brew install ninja
```
6. Install [QtCreator](https://download.qt.io/official_releases/online_installers/).
7. Download and install [vcpkg4aspia](https://github.com/dchapyshev/vcpkg4aspia) (forked from Microsoft repository). To do this, go to the directory where you want to install and run the commands:
```bash
git clone https://github.com/dchapyshev/vcpkg4aspia.git
cd vcpkg4aspia
./bootstrap-vcpkg.sh
```
8. Install libraries in vcpkg.

If you are building on an **Intel**-based version of MacOS for **x86_64**, then the installation command looks like:
```bash
./vcpkg install <library_name>:x64-osx
```
If you are building on an **ARM**-based version of MacOS for **ARM**, then the installation command looks like:
```bash
./vcpkg install <library_name>:arm64-osx
```
If you are building on an **ARM**-based version of MacOS for **x86_64**, then the installation command looks like:
```bash
arch -arch x86_64 ./vcpkg install <library_name> --triplet=x64-osx --host-triplet=x64-osx
```
List of libraries to install:
```
asio
curl
gtest
icu
libvpx
libyuv
openssl
opus
protobuf
qt5-base
qt5-translations
rapidjson
sqlite3
zstd
```
9. Open **QtCreator -> Tools -> Options -> Kits -> Qt Versions**. Click the Add button and specify the path to **<vcpkg_path>/installed/<arch>/tools/qt5/bin/qmake**.
10. Open **QtCreator -> Tools -> Options -> Kits -> Kits**. Click the Add button. Enter a display name for the profile, specify the compilers, and the Qt profile you added earlier.
11. Open **CMakeLists.txt** from the Aspia root directory in QtCreator and configure the build using the previously added profile.

Alternative instructions (SW build system)
------------------------------------------
1. Download, unpack and add to PATH `sw` tool from https://github.com/SoftwareNetwork/binaries
2. Run `sw build source` in the project root ([example](https://github.com/dchapyshev/aspia/blob/master/.github/workflows/sw.yml)).
   Binaries will be available under `.sw/out` directory.
3. To generate VS solution, run `sw generate source`.
