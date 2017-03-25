Aspia Remote Desktop

Remote desktop implementation (client and server) based on Google Protocol Buffers.
In the project partially uses code from WebRTC and Chromium Remoting.
Also uses:
   - libvpx (Library for encoding and decoding vp8/vp9 video formats).
   - zlib-ng (Library for compressing and decompressing. Different from the
     original zlib presence of patches from Intel and Cloudflare for faster
     work on modern processors)
   - libglog (Logging library from Google)
   - gflags (Command line arguments parsing)
   - libyuv (For image formats convertation)
   - WTL (For GUI in Windows)
   - gtest (Library from Google for unit testing)

For building the project requires:
   - Visual Studio 2015: http://visualstudio.com/
   - YASM compiller: http://yasm.tortall.net/Download.html (version for "general use")

All dependencies are included in the project.
Many of the comments in the code now on Russian language. Later they will be
translated into English.

Project code is available under the GNU Lesser General Public License version 3.

At the moment the project in development stage and is not intended for use.

If you have any questions, you can email me: dmitry@aspia.ru
