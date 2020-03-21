OpenSSL Building (for Windows)
=========================

*WARNING!* This guide does not claim to be complete.
His goal is to show with what parameters we build third-party projects.
To build third-party components, refer to the assembly guide for these projects.

Sequence
--------
1. You must have installed:
   - [Visual Studio 2017 (with SDK 10.0.17763.0)](https://visualstudio.microsoft.com)
   - [Perl](http://www.activestate.com/activeperl)
   - [NASM](https://www.nasm.us).
2. Open "Command Prompt" and go to the directory with "openssl_build_x86.cmd".
3. Command to build:

   **openssl_build_x86.cmd [openssl_src_dir] [openssl_out_dir]**

   **openssl_src_dir**  - The directory containing unpacked OpenSSL files.

   **openssl_out_dir**  - The directory in which the compiled files will be placed.

   *Example:* openssl_build_x86.cmd "D:\openssl-src-1.1.1" "D:\openssl-out-1.1.1"