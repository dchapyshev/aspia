pushd %~dp0
del   /S /Q *.user
rmdir /S /Q Debug
rmdir /S /Q Release
rmdir /S /Q ipch
del /Q /F /A:H *.suo
del /Q *.ncb
del /Q *.sdf
del /Q /F /A:H *.opensdf
del /Q /F /A:H *.suo
popd