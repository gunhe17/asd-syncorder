# Windows 개발환경 세팅 가이드

개발 환경: Windows 10/11  
대상 프로젝트: C++ 기반 실시간 센서/카메라 통합 시스템

<br>


## 0. template file

- .vscode\c_cpp_properties.json, local 환경에 맞춰서 작성하기
- scripts\ops\build.bat, local 환경에 맞춰서 작성하기

<br>

## 1. Visual C++ Build Tools 설치

- 설치 URL:  
  [https://visualstudio.microsoft.com/ko/visual-cpp-build-tools/](https://visualstudio.microsoft.com/ko/visual-cpp-build-tools/)

- 설치 시 선택한 구성:

  - **Visual Studio 핵심 편집기**
  - **개별 구성 요소:**
    - MSVC v143 - VS 2022 C++ x64/x86 빌드 도구
    - 최신 v143 빌드 도구용 C++ ATL (x86 및 x64)
    - Windows 11 SDK (10.0.22621.0)
    - Microsoft .NET Framework 4.6.2 SDK

<br>

## 2. Tobii SDK 설치

- 다운로드 URL:  
  [https://connect.tobii.com/s/sdk-downloads?language=en_US](https://connect.tobii.com/s/sdk-downloads?language=en_US)

- 설치 경로 구조:
  ```
  C:\Users\insighter\workspace\sdk\tobii\
  └─ 64\...
  ```

- 환경변수 설정:

  **시스템 Path에 추가**
  ```
  C:\Users\insighter\workspace\sdk\tobii\64\include
  C:\Users\insighter\workspace\sdk\tobii\64\lib
  ```

<br>

## 3. Intel RealSense SDK 설치

- 다운로드 URL:  
  [https://github.com/IntelRealSense/librealsense/releases](https://github.com/IntelRealSense/librealsense/releases)
  - Intel.RealSense.SDK-WIN10-2.56.4.9191.exe #resource
  - Source code (zip) #resource-example

- 공식 설치 가이드:  
  [installation_windows.md](https://github.com/IntelRealSense/librealsense/blob/master/doc/installation_windows.md)

### Metadata 활성화 (Windows 전용)

```powershell
# 관리자 권한 PowerShell 실행
Set-ExecutionPolicy Unrestricted

cd path\to\librealsense\scripts
.\realsense_metadata_win10.ps1 -op install_all

Set-ExecutionPolicy RemoteSigned
```

> 새로운 RealSense 디바이스 연결 시 스크립트 재실행 필요

<br>

## 4. build

```powershell
.\scripts\ops\build.bat
```