# Skyrim Load Accelerator

[![Build](https://github.com/servaltullius/skyrim-load-accelerator/actions/workflows/build.yml/badge.svg)](https://github.com/servaltullius/skyrim-load-accelerator/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

스카이림 스페셜 에디션(AE 1.6.x)의 로딩 시간을 **30~60% 단축**하는 SKSE 플러그인입니다.

압축 해제, 메모리 할당, 파일 I/O, 캐싱, 멀티스레딩 등 8개 최적화 모듈을 통해 게임 시작, 세이브 로드, 셀 전환 시 체감 로딩 속도를 개선합니다.

## 최적화 모듈

| 모듈 | 카테고리 | 설명 | 기본값 |
|------|----------|------|--------|
| **ZlibReplacement** | 압축 해제 | zlib `uncompress`를 libdeflate로 대체 (약 3배 빠른 zlib 압축 해제) | ON |
| **LZ4Upgrade** | 압축 해제 | 스카이림 내장 LZ4를 AVX2/SSE4.2 최적화된 최신 LZ4 라이브러리로 교체 | ON |
| **AllocatorReplacement** | 메모리 | CRT malloc/calloc/realloc/free를 mimalloc으로 대체 | **OFF** |
| **INICaching** | 파일 I/O | `GetPrivateProfileString` 계열 API 호출 결과를 메모리에 캐싱 | ON |
| **MaxStdio** | 파일 I/O | CRT 파일 핸들 제한을 512에서 8192로 확장 | ON |
| **SequentialScan** | 파일 I/O | BSA/BA2 파일 열기 시 `FILE_FLAG_SEQUENTIAL_SCAN` 힌트 추가 | ON |
| **FormCache** | 캐싱 | `TESForm::LookupByID` 결과를 TBB concurrent_hash_map으로 캐싱 | **OFF** |
| **ParallelBSA** | 스레딩 | BSA 블록 압축 해제를 스레드 풀에서 병렬 처리 | ON |

> **AllocatorReplacement**와 **FormCache**는 SSE Engine Fixes와의 충돌 방지를 위해 기본 OFF입니다.

## 요구 사항

- **Skyrim Special Edition / Anniversary Edition** 1.6.x
- **SKSE64** (게임 버전에 맞는 최신 빌드)
- **Address Library for SKSE Plugins**

## 설치

### 수동 설치

1. [Releases](https://github.com/servaltullius/skyrim-load-accelerator/releases) 페이지에서 ZIP 파일 다운로드
2. ZIP 내용을 스카이림 `Data` 폴더에 압축 해제

설치 후 파일 구조:

```
Data/
  SKSE/
    Plugins/
      SkyrimLoadAccelerator.dll
      SkyrimLoadAccelerator.ini
```

### Mod Organizer 2 (MO2)

1. ZIP 파일을 MO2에 드래그하거나 "Install a new mod from an archive" 사용
2. 왼쪽 패널에서 활성화

## 설정

설정 파일 위치: `Data/SKSE/Plugins/SkyrimLoadAccelerator.ini`

```ini
[General]
; 로그 활성화 (Documents/My Games/Skyrim Special Edition/SKSE/SkyrimLoadAccelerator.log)
bEnableLogging = 1
; 각 모듈의 성능 벤치마크를 로그에 기록
bEnableBenchmark = 0

[Decompression]
; zlib uncompress를 libdeflate로 대체
bReplaceZlib = 1
; LZ4 압축 해제를 최신 최적화 버전으로 업그레이드
bUpgradeLZ4 = 1

[Memory]
; CRT malloc을 mimalloc으로 대체 (SSE Engine Fixes TBBMalloc 사용 시 비활성화)
bReplaceMalloc = 0

[FileIO]
; INI 파일 읽기 결과를 메모리에 캐싱 (PrivateProfileRedirector 사용 시 비활성화)
bCacheINI = 1
; 최대 파일 핸들 수 증가
bMaxStdio = 1
; 파일 핸들 제한값 (512~8192)
iMaxStdioLimit = 8192
; BSA/BA2 파일에 FILE_FLAG_SEQUENTIAL_SCAN 힌트 추가
bSequentialScan = 1

[Caching]
; TESForm 조회 결과 캐싱 (SSE Engine Fixes FormCaching 사용 시 비활성화)
bFormCache = 0

[Threading]
; BSA 블록 압축 해제 병렬 처리
bParallelBSA = 1
; 스레드 풀 크기 (0 = 자동, hardware_concurrency - 1)
iThreadPoolSize = 0
```

## 호환성

### SSE Engine Fixes

런타임에 `EngineFixes.dll` 로드 여부를 자동 감지합니다.

- **AllocatorReplacement** (`bReplaceMalloc`): Engine Fixes 감지 시 자동 비활성화. 두 모드 모두 CRT 힙 함수를 교체하므로 동시 사용 시 충돌합니다.
- **FormCache** (`bFormCache`): Engine Fixes 감지 시 자동 비활성화. Engine Fixes도 FormCache 기능을 포함합니다.
- **MaxStdio** (`bMaxStdio`): 안전하게 함께 사용 가능. 더 높은 값이 적용됩니다.
- 나머지 모듈(ZlibReplacement, LZ4Upgrade, INICaching, SequentialScan, ParallelBSA)은 충돌 없이 사용 가능합니다.

### PrivateProfileRedirector

런타임에 `PrivateProfileRedirector.dll` 로드 여부를 자동 감지합니다.

- **INICaching** (`bCacheINI`): PrivateProfileRedirector 감지 시 자동 비활성화. 두 모드 모두 `GetPrivateProfileString` API를 후킹하므로 동시 사용 시 충돌합니다.

### 기타 모드

IAT(Import Address Table) 후킹 방식을 사용하므로, 동일 API를 후킹하지 않는 대부분의 SKSE 플러그인과 호환됩니다.

## 소스 빌드

### 요구 사항

- **Visual Studio 2022** 이상 (C++23 지원, MSVC v143+)
- **CMake 3.25** 이상
- **vcpkg** (의존성 관리)

### 의존성 (vcpkg 자동 설치)

- [CommonLibSSE-NG](https://gitlab.com/colorglass/vcpkg-colorglass) -- SKSE 플러그인 프레임워크
- [libdeflate](https://github.com/ebiggers/libdeflate) -- 고속 zlib 호환 압축 해제
- [lz4](https://github.com/lz4/lz4) -- 최적화된 LZ4 압축 해제
- [mimalloc](https://github.com/microsoft/mimalloc) -- 고성능 메모리 할당기
- [spdlog](https://github.com/gabime/spdlog) -- 로깅
- [SimpleIni](https://github.com/brofield/simpleini) -- INI 파일 파싱
- [oneTBB](https://github.com/oneapi-src/oneTBB) -- concurrent_hash_map, 스레딩

### 빌드 명령

```bash
# vcpkg가 설치되어 있고 VCPKG_ROOT 환경 변수가 설정되어 있어야 합니다
cmake --preset release
cmake --build --preset release
```

빌드 결과물:

- `build/release/Release/SkyrimLoadAccelerator.dll`

`SKYRIM_MODS_PATH` 환경 변수를 설정하면 빌드 후 자동으로 해당 경로에 DLL과 INI 파일이 복사됩니다.

```bash
# MO2 mods 폴더 예시
set SKYRIM_MODS_PATH=C:\MO2\mods
cmake --preset release
cmake --build --preset release
```

## 동작 원리

플러그인은 SKSE 로드 시점(`SKSEPluginLoad`)에 초기화되며, IAT(Import Address Table) 후킹과 SKSE 트램펄린을 통해 게임 함수를 교체합니다.

- **ZlibReplacement**: `zlibx64.dll`의 `uncompress` IAT 엔트리를 libdeflate 구현으로 대체. 실패 시 원본 zlib로 폴백.
- **LZ4Upgrade**: `lz4.dll`의 `LZ4_decompress_safe` IAT 엔트리를 최신 LZ4 라이브러리로 대체.
- **AllocatorReplacement**: CRT 힙 DLL(`api-ms-win-crt-heap-l1-1-0.dll`, `ucrtbase.dll`, `msvcrt.dll`)의 `malloc`, `calloc`, `realloc`, `free`, `_aligned_malloc`, `_aligned_free`, `_msize`를 mimalloc으로 대체.
- **INICaching**: `kernel32.dll`의 `GetPrivateProfileStringA`, `GetPrivateProfileIntA`, `WritePrivateProfileStringA`를 후킹하여 INI 파일 내용을 메모리에 캐싱. `shared_mutex`로 스레드 안전 보장. 쓰기 시 캐시 무효화.
- **MaxStdio**: `_setmaxstdio()` 호출로 CRT 파일 핸들 제한 확장 (512~8192 범위).
- **SequentialScan**: `kernel32.dll`의 `CreateFileW`를 후킹하여 `.bsa`/`.ba2` 파일 읽기 시 `FILE_FLAG_SEQUENTIAL_SCAN` 플래그 추가, `FILE_FLAG_RANDOM_ACCESS` 플래그 제거.
- **FormCache**: SKSE 트램펄린으로 `TESForm::LookupByID`(REL ID: SE 14461, AE 14602)를 후킹. TBB `concurrent_hash_map`으로 조회 결과 캐싱. 데이터 로드 완료(`kDataLoaded`) 시 캐시 초기화.
- **ParallelBSA**: `std::latch` 기반 스레드 풀로 BSA 블록을 병렬 압축 해제. 워커 스레드별 `thread_local` libdeflate 디컴프레서 사용.

## 프로젝트 구조

```
src/
  Main.cpp                              # SKSE 플러그인 진입점
  Plugin.h                              # 플러그인 메타데이터 (이름, 버전)
  Settings.h / Settings.cpp             # INI 설정 로드
  Hooks.h / Hooks.cpp                   # 모듈 설치 오케스트레이션, 호환성 감지
  Utils/
    IATHook.h / IATHook.cpp             # IAT 후킹 유틸리티
    Benchmark.h / Benchmark.cpp         # 성능 타이머
  Optimizations/
    Decompression/
      ZlibReplacement.h / .cpp          # zlib -> libdeflate
      LZ4Upgrade.h / .cpp               # LZ4 업그레이드
    Memory/
      AllocatorReplacement.h / .cpp     # CRT malloc -> mimalloc
    FileIO/
      INICaching.h / .cpp               # INI 읽기 캐싱
      MaxStdio.h / .cpp                 # 파일 핸들 제한 확장
      SequentialScan.h / .cpp           # BSA/BA2 순차 읽기 힌트
    Caching/
      FormCache.h / .cpp                # TESForm 조회 캐싱
    Threading/
      ParallelBSA.h / .cpp             # 병렬 BSA 압축 해제
```

## 라이선스

[MIT License](LICENSE)

Copyright (c) 2025 SkyrimLoadAccelerator Team
