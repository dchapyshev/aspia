Aspia Remote Desktop
====================
Remote desktop implementation (client and server) based on Google Protocol Buffers.

Currently supported:
   - Remote desktop management
   - Remote desktop view
   - Remote power management
   - Encryption (all transmitted information is encrypted using the algorithms XSalsa20 + Poly1305)
   - Authorization (it is possible to add users with different access rights)

It is planned to implement:
   - File transfer
   - System information (for local and remote computers)
   - Audio transfer
   - Address book with encryption and master-password
   - Authorization with Windows credentials
   - NAT traversal

Thrid-party components
======================
In the project partially uses code from WebRTC and Chromium Remoting.
Also uses:
   - libvpx (encoding and decoding vp8/vp9 video formats).
   - zlib-ng (compressing and decompressing. Different from the
     original zlib presence of patches from Intel and Cloudflare for faster
     work on modern processors)
   - libglog (logging)
   - gflags (command line arguments parsing)
   - libyuv (image formats convertation)
   - gtest (unit testing)
   - libsodium (encryption)

Building
========
For building the project requires:
   - Visual Studio 2017: http://visualstudio.com
   - YASM compiller: http://yasm.tortall.net/Download.html (version for "general use")

All thrid party dependencies are included in the project.

System requirements
===================
x86 or x86_64 CPU with SSE2 support, Windows XP SP3 or higher.

Licensing
=========
Project code is available under the Mozilla Public License Version 2.0.

Other information
=================
At the moment the project in development stage and is not intended for use.
If you have any questions, you can email me: dmitry@aspia.ru
