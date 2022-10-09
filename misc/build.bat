@echo off
mkdir ..\build
pushd ..\build
cd
cl -FC -Zi ..\code\64_handmade.cpp user32.lib gdi32.lib
popd