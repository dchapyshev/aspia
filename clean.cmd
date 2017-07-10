pushd %~dp0
rmdir /S /Q Debug
rmdir /S /Q Release
rmdir /S /Q .vs
del   /S /Q *.user
del   /S /Q *.obj
del   /S /Q *.pdb
del   /S /Q *.res
del   /S /Q *.exe
popd