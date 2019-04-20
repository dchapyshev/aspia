Qt Building (for Windows)
=========================

*WARNING!* This guide does not claim to be complete.
His goal is to show with what parameters we build third-party projects.
To build third-party components, refer to the assembly guide for these projects.

Sequence
--------
1. You must have built OpenSSL to build Qt.
2. You must have installed:
   - [Visual Studio 2019 (with SDK 10.0.17763.0)](https://visualstudio.microsoft.com)
   - [Python](http://www.python.org/download)
   - [Perl](http://www.activestate.com/activeperl)
   - [jom](https://wiki.qt.io/Jom).
3. Open "Command Prompt" and go to the directory with "qt_build_x86_*.cmd".
4. Use "qt_build_x86_rel.cmd" for releases, use "qt_build_x86_dev.cmd" for developement.
   Command to build:

   **qt_build_x86_*.cmd [qt_src_dir] [qt_out_dir] [openssl_dir]**

   **qt_src_dir**  - The directory containing unpacked Qt files.

   **qt_out_dir**  - The directory in which the compiled files will be placed.

   **openssl_dir** - The directory containing compilled OpenSSL (must contain within directories "include" and "lib").

   *Example:* qt_build_x86_rel.cmd "D:\qt\src-5.12.1" "D:\qt\out-x86-5.12.1" "D:\openssl"
