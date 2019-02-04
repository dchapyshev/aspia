Instructions for translators
============================
You need to translate the application and installer.

To translate the application interface you need to download [Qt Linguist](https://files.aspia.org/dev/linguist.7z).

To translate the installer interface you need to download a text editor. For example, [Notepad++](https://notepad-plus-plus.org/download).

For translation, it is recommended to download the source code from the [git repository](https://github.com/dchapyshev/aspia).

Translation of the application interface
----------------------------------------
The application is divided into modules. Each module has its own files for translation.

You need to translate the following modules:
- [client](https://github.com/dchapyshev/aspia/tree/master/source/client)
- [common](https://github.com/dchapyshev/aspia/tree/master/source/common)
- [console](https://github.com/dchapyshev/aspia/tree/master/source/console)
- [host](https://github.com/dchapyshev/aspia/tree/master/source/host)
- [updater](https://github.com/dchapyshev/aspia/tree/master/source/updater)

These modules contain directory "translations".
Open your language file from this directory in Qt Linguist and perform the translation.

For assistance with Qt Linguist, refer to the [documentation](http://doc.qt.io/qt-5/qtlinguist-index.html).

If there is no translation file for your language, create a [issue](https://github.com/dchapyshev/aspia/issues) on GitHub.

Translation of the installer interface
--------------------------------------
Change directory to "[installer/translations](https://github.com/dchapyshev/aspia/tree/master/installer/translations)".

This directory contains installer translations for Aspia Console and Aspia Host.

File to translate Aspia Console: **console.<language_code>.wxl**

File to translate Aspia Host: **host.<language_code>.wxl**

You need to translate both files.

Open your language files in a text editor and complete the translation.

If there is no translation file for your language, create a [issue](https://github.com/dchapyshev/aspia/issues) on GitHub.
