pushd %~dp0
del   /S /Q *.user
rmdir /S /Q Debug
rmdir /S /Q Release
rmdir /S /Q .vs
popd