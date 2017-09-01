Aspia Remote Desktop
====================
Remote desktop implementation (client and server) based on Google Protocol Buffers.

|Build Status|Coverity Scan Status|
|:--:|:--:|
|[![Build Status](https://ci.appveyor.com/api/projects/status/yq0cvje9atuj0o4v/branch/master?svg=true)](https://ci.appveyor.com/project/dchapyshev/remote-desktop/branch/master)|[![Coverity Scan Status](https://scan.coverity.com/projects/13117/badge.svg)](https://scan.coverity.com/projects/aspia-remote-desktop)|

Currently supported
-------------------
   - Remote desktop management
   - Remote desktop view
   - Remote power management
   - File transfer
   - Encryption (all transmitted information is encrypted using the algorithms XSalsa20 + Poly1305)
   - Authorization (it is possible to add users with different access rights)
   - Work in service or application mode

It is planned to implement
--------------------------
   - System information (for local and remote computers)
   - Audio transfer
   - Address book with encryption and master-password
   - Authorization with Windows credentials
   - NAT traversal

Thrid-party components
----------------------
In the project partially uses code from WebRTC and Chromium Remoting.
Also uses:
   - [libvpx](https://chromium.googlesource.com/webm/libvpx "libvpx") (encoding and decoding vp8/vp9 video formats).
   - [zlib-ng](https://github.com/Dead2/zlib-ng "zlib-ng") (compressing and decompressing. Different from the
     original zlib presence of patches from Intel and Cloudflare for faster
     work on modern processors)
   - [libglog](https://github.com/google/glog "libglog") (logging)
   - [gflags](https://github.com/gflags/gflags "gflags") (command line arguments parsing)
   - [libyuv](https://chromium.googlesource.com/libyuv/libyuv "libyuv") (image formats convertation)
   - [gtest](https://github.com/google/googletest "gtest") (unit testing)
   - [libsodium](https://github.com/jedisct1/libsodium/releases "libsodium") (encryption)
   - [WTL](https://sourceforge.net/projects/wtl "WTL") (for UI in Windows)
   - [asio](https://github.com/chriskohlhoff/asio "asio") (for networking)
   - [FatCow Icons](http://www.fatcow.com/free-icons "FatCow Icons")
   - [Fugue Icons](https://github.com/yusukekamiyamane/fugue-icons "Fugue Icons")

Building
--------
For building the project requires:
   - [Visual Studio 2017](http://visualstudio.com "Visual Studio 2017")
   - [YASM compiller](http://yasm.tortall.net/Download.html "YASM compiller") (version for "general use")

All thrid party dependencies are included in the project.

System requirements
-------------------
x86 or x86_64 CPU with SSE2 support, Windows XP SP3 or higher.

Other information
-----------------
At the moment the project in development stage and is not intended for use.
If you have any questions, you can email me: dmitry@aspia.ru

Licensing
---------
Project code is available under the Mozilla Public License Version 2.0.