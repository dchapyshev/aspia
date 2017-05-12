Aspia Remote Desktop

Remote desktop implementation (client and server) based on Google Protocol Buffers.
In the project partially uses code from WebRTC and Chromium Remoting.
Also uses:
   - libvpx (library for encoding and decoding vp8/vp9 video formats).
   - zlib-ng (library for compressing and decompressing. Different from the
     original zlib presence of patches from Intel and Cloudflare for faster
     work on modern processors)
   - libglog (logging library)
   - gflags (command line arguments parsing)
   - libyuv (image formats convertation)
   - gtest (unit testing)

For building the project requires:
   - Visual Studio 2017: http://visualstudio.com/
   - YASM compiller: http://yasm.tortall.net/Download.html (version for "general use")

All thrid party dependencies are included in the project.

Project code is available under the GNU Lesser General Public License version 3.

System requirements: x86 or x86_64 CPU, Windows XP SP3 or higher.

At the moment the project in development stage and is not intended for use.

If you have any questions, you can email me: dmitry@aspia.ru
