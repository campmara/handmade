@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd "C:\work\cpp\learning\handmade\"
call cls
call pwsh
set path=c:\work\cpp\learning\handmade\misc;%path%