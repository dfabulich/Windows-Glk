@echo off

"%ProgramFiles(x86)%\Zip\zip" \Temp\WindowsGlk.zip Glk.c WinGlk.html
"%ProgramFiles(x86)%\Zip\zip" -j \Temp\WindowsGlk.zip Executables\Release\Glk*.*
"%ProgramFiles(x86)%\Zip\zip" -j \Temp\WindowsGlk.zip GlkDll\Glk.def
"%ProgramFiles(x86)%\Zip\zip" -r \Temp\WindowsGlk.zip Examples\* Include\*

"%ProgramFiles(x86)%\Zip\zip" \Temp\WindowsGlkSrc.zip Glk.c WinGlk.html MakeDist.bat
"%ProgramFiles(x86)%\Zip\zip" -r \Temp\WindowsGlkSrc.zip GlkDll\* Include\*
pushd ..\..
"%ProgramFiles(x86)%\Zip\zip" \Temp\WindowsGlkSrc.zip Libraries\libmodplug\*
"%ProgramFiles(x86)%\Zip\zip" \Temp\WindowsGlkSrc.zip Libraries\mfc\*
popd
