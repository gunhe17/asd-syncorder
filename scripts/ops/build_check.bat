@echo off
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"

cl ^
  /std:c++17 ^
  /EHsc ^
  /MT ^
  /W3 ^
  /O2 ^
  /D_CRT_SECURE_NO_WARNINGS ^
  /wd4819 ^
  /I . ^
  /I "C:\Users\insighter\workspace\sdk\tobii\64\include" ^
  /I "C:\Users\insighter\workspace\sdk\realsense\include" ^
  /I "C:\Users\insighter\workspace\sdk\realsense\third-party" ^
  syncorder\check.cpp ^
  syncorder\gonfig\gonfig.cpp ^
  /Fe:bin\checker.exe ^
  /link ^
  /LIBPATH:"C:\Users\insighter\workspace\sdk\tobii\64\lib" ^
  /LIBPATH:"C:\Users\insighter\workspace\sdk\realsense\lib\x64" ^
  mf.lib ^
  mfplat.lib ^
  mfreadwrite.lib ^
  mfuuid.lib ^
  ole32.lib ^
  tobii_research.lib ^
  realsense2.lib