# PDFNarrator

An interactive Qt6/QML desktop and mobile document reader that extracts and reads PDF content aloud using Text-to-Speech (TTS) while dynamically presenting embedded document images in sync with narration.

---

## Features

* **Synchronized Narration:** Reads PDF text aloud with real-time audio playback.
* **Dynamic Image Flow:** Automatically renders and advances visual figures alongside text narration.
* **Cross-Platform:** Single codebase targeting Linux (Desktop) and Android (`arm64-v8a`).
* **Fast Rendering:** Powered by Qt 6 QML and native hardware-accelerated processing.

---

## Prerequisites

* **Qt 6.x** (with QML and CMake modules)
* **Ninja** build system & **CMake** ($\ge 3.20$)
* **For Android:** Android NDK (`r28+`), Android SDK, and `adb`

---

## Build & Run

All build configurations, packaging, and device monitoring are managed via the root `./build.sh` script:

```bash
chmod +x build.sh
./build.sh [OPTIONS]

```

### Build Commands

| Target / Action | Command | Output Artifact |
| --- | --- | --- |
| **Linux Debug Build** | `./build.sh DEBUG` | Symlink `./exec` & `compile_commands.json` |
| **Linux Release Build** | `./build.sh RELEASE` | Symlink `./exec` |
| **Debian Package (`.deb`)** | `./build.sh RELEASE DEB` | `build/linux-release/*.deb` |
| **Android APK** | `./build.sh ANDROID` | `build/android-release/android-build/pdfnarrator.apk` |
| **Clean Rebuild** | `./build.sh CLEAN [DEBUG|RELEASE|ANDROID]` | Reconfigures build directory from scratch |
| **Android Logcat Monitor** | `./build.sh MONITOR` | Auto-attaching, process-filtered Android logger |

---

## Running the Application

**Linux Desktop:**

```bash
./exec

```

**Android Device:**

```bash
# Build, prompt to install via adb, and monitor live output
./build.sh ANDROID MONITOR

```
