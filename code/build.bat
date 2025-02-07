@echo off

:: TODO - Can we build both x86 and x64 with one exe?

:: Create the build directory if it doesn't exist and enter it.
IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

:: Store various categories of compiler flags into separate variables, for organization.
set HM_BASEFILE=..\code\win32_handmade.cpp
set HM_WARNINGS=-WX -W4 -wd4201 -wd4100 -wd4189
set HM_DEFINES=-DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1
set HM_OPTIMIZATIONS=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -FC -Z7 -Fmwin32_handmade.map
set HM_LINK=-opt:ref user32.lib gdi32.lib winmm.lib

:: Compile x86.
:: call cl %HM_WARNINGS% %HM_DEFINES% %HM_OPTIMIZATIONS% %HM_BASEFILE% /link -subsystem:windows,5.1 %HM_LINK%

:: Compile x64.
call cl %HM_WARNINGS% %HM_DEFINES% %HM_OPTIMIZATIONS% %HM_BASEFILE% /link -subsystem:windows %HM_LINK%

:: Return to the previous directory.
popd
