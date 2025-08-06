@echo off
call "...\vcvars64.bat"

cl ^
  /std:c++17 ^
  /EHsc ^
  /MT ^
  /W3 ^
  /O2 ^
  /D_CRT_SECURE_NO_WARNINGS ^
  /wd4819 ^
  /I . ^
  /I "...\tobii\64\include" ^
  /I "...\realsense\include" ^
  /I "...\realsense\third-party" ^
  syncorder\main.cpp ^
  syncorder\gonfig\gonfig.cpp ^
  /Fe:bin\syncorder.exe ^
  /link ^
  /LIBPATH:"...\tobii\64\lib" ^
  /LIBPATH:"...\realsense\lib\x64" ^
  mf.lib ^
  mfplat.lib ^
  mfreadwrite.lib ^
  mfuuid.lib ^
  ole32.lib ^
  tobii_research.lib ^
  realsense2.lib