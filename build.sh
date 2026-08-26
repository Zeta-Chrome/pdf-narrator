#!/bin/bash

SYMLINK_NAME="exec"
EXECUTABLE_NAME="pdfnarrator"
PACKAGE_NAME="org.qtproject.example.pdfnarrator"
BUILD_TYPE="Debug"
IS_ANDROID=false
PLATFORM="PC"
FORCE_RECONFIGURE=false
HAS_BUILD_ARG=false
DO_MONITOR=false
DO_PACKAGE=false

# ---- Parse args ----
for arg in "$@"; do
    case "$arg" in
        DEBUG|debug|Debug)       BUILD_TYPE="Debug"; HAS_BUILD_ARG=true ;;
        RELEASE|release|Release) BUILD_TYPE="Release"; HAS_BUILD_ARG=true ;;
        ANDROID|android|Android) IS_ANDROID=true; PLATFORM="Android"; BUILD_TYPE="Release"; HAS_BUILD_ARG=true ;;
        CLEAN|clean)             FORCE_RECONFIGURE=true ;;
        MONITOR|monitor|Monitor) DO_MONITOR=true ;;
        DEB|deb)                 DO_PACKAGE=true ;;
        *)
            echo "Usage: ./build.sh [DEBUG|RELEASE] [ANDROID] [CLEAN] [MONITOR] [DEB]"
            exit 1
            ;;
    esac
done

# ---- Colors ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# ---- Monitor: clear old logs, wait for the app, stream its logs ----
# Standalone mode: `./build.sh MONITOR` just watches logs, skipping build
# entirely. Re-attaches automatically if the app is closed and relaunched,
# since the PID changes every launch.
monitor_logs() {
    echo -e "${YELLOW}Clearing old logcat buffer...${NC}"
    adb logcat -c

    trap 'echo; echo -e "${YELLOW}Stopping monitor...${NC}"; [ -n "$LOGCAT_PID" ] && kill "$LOGCAT_PID" 2>/dev/null; exit 0' INT

    while true; do
        echo -e "${YELLOW}Waiting for ${PACKAGE_NAME} to launch...${NC}"
        APP_PID=""
        while [ -z "$APP_PID" ]; do
            APP_PID=$(adb shell ps -A 2>/dev/null | grep "$PACKAGE_NAME" | awk '{print $2}' | head -n1)
            [ -z "$APP_PID" ] && sleep 1
        done

        echo -e "${GREEN}${PACKAGE_NAME} running (pid ${APP_PID}) — streaming logs ${NC}"
        adb logcat --pid="$APP_PID" -v color -v time &
        LOGCAT_PID=$!

        while kill -0 "$LOGCAT_PID" 2>/dev/null; do
            STILL_ALIVE=$(adb shell ps -A 2>/dev/null | grep "$PACKAGE_NAME" | awk '{print $2}' | head -n1)
            if [ "$STILL_ALIVE" != "$APP_PID" ]; then
                kill "$LOGCAT_PID" 2>/dev/null
                break
            fi
            sleep 1
        done
        echo -e "${YELLOW}${PACKAGE_NAME} exited.${NC}"
    done
}

if [ "$DO_MONITOR" = true ] && [ "$HAS_BUILD_ARG" = false ] && [ "$FORCE_RECONFIGURE" = false ]; then
    monitor_logs
    exit 0
fi

# ---- Set Qt base path ----
if [ -z "$QT_BASE" ]; then export QT_BASE="$HOME/Qt/6.11.1"; fi
if [ ! -d "$QT_BASE" ]; then
    echo -e "${RED}Qt not found at: $QT_BASE${NC}"
    exit 1
fi

# ---- Set ABI and build directory ----
if [ "$IS_ANDROID" = true ]; then
    export ABI="android_arm64_v8a"
    BUILD_DIR="build/android-release"
else
    export ABI="gcc_64"
    BUILD_DIR="build/linux-$(echo $BUILD_TYPE | tr '[:upper:]' '[:lower:]')"
fi

echo -e "${YELLOW}Platform: ${PLATFORM} | Build: ${BUILD_TYPE} | Dir: ${BUILD_DIR}${NC}"

# ---- Clean if requested ----
if [ "$FORCE_RECONFIGURE" = true ]; then
    echo -e "${YELLOW}Cleaning ${BUILD_DIR}...${NC}"
    rm -rf "$BUILD_DIR"

    if [ "$HAS_BUILD_ARG" = false ]; then
        echo -e "${GREEN}Cleaned ${BUILD_DIR}. Not rebuilding (no DEBUG/RELEASE/ANDROID arg given).${NC}"
        exit 0
    fi
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR" || exit 1

# ---- Keystore signing env vars ----
if [ "$IS_ANDROID" = true ]; then
    if [ -z "$QT_ANDROID_KEYSTORE_PATH" ];       then export QT_ANDROID_KEYSTORE_PATH="$HOME/.android/debug.keystore"; fi
    if [ -z "$QT_ANDROID_KEYSTORE_ALIAS" ];      then export QT_ANDROID_KEYSTORE_ALIAS="androiddebugkey"; fi
    if [ -z "$QT_ANDROID_KEYSTORE_STORE_PASS" ]; then export QT_ANDROID_KEYSTORE_STORE_PASS="android"; fi
    if [ -z "$QT_ANDROID_KEYSTORE_KEY_PASS" ];   then export QT_ANDROID_KEYSTORE_KEY_PASS="android"; fi
fi

# ---- Android env vars (must always be set — ninja can trigger an
# ---- automatic reconfigure mid-build whenever CMakeLists.txt changes,
# ---- even on runs that skip our own explicit cmake configure step below.
# ---- If these aren't exported unconditionally, that auto-reconfigure
# ---- fails with "ANDROID_NDK environment variable must be set.") ----
if [ "$IS_ANDROID" = true ]; then
    if [ -z "$QT_HOST_PATH" ]; then export QT_HOST_PATH="$HOME/Qt/6.11.1/gcc_64"; fi
    if [ -z "$ANDROID_NDK" ];   then export ANDROID_NDK="/opt/android-sdk/ndk/28.2.13676358"; fi
    if [ -z "$ANDROID_SDK_ROOT" ]; then export ANDROID_SDK_ROOT="/opt/android-sdk"; fi

    if [ ! -d "$ANDROID_NDK" ]; then echo -e "${RED}NDK not found: $ANDROID_NDK${NC}"; exit 1; fi
fi

# ---- Configure (only if not already configured) ----
if [ ! -f "build.ninja" ]; then
    echo -e "${YELLOW}Configuring...${NC}"

    if [ "$IS_ANDROID" = true ]; then
        cmake -GNinja \
            -DANDROID=ON \
            -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
            -DANDROID_ABI=arm64-v8a \
            -DANDROID_PLATFORM=android-24 \
            -DCMAKE_BUILD_TYPE=Release \
            -DQT_HOST_PATH="$QT_HOST_PATH" \
            -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
            -DQT_ANDROID_SIGN_APK=ON \
            ../.. || exit 1
    else
        cmake -GNinja \
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
            ../.. || exit 1
    fi
else
    echo -e "${GREEN}Already configured — skipping cmake configure${NC}"
    echo -e "${GREEN}(CMakeLists.txt changes are picked up automatically by cmake --build)${NC}"
fi

# ---- Build ----
echo -e "${YELLOW}Building...${NC}"
cmake --build . -j$(nproc) || exit 1

# ---- Package (.deb, Linux only) ----
if [ "$DO_PACKAGE" = true ] && [ "$IS_ANDROID" = false ]; then
    echo -e "${YELLOW}Packaging .deb...${NC}"
    cpack -G DEB || exit 1
    DEB_FILE=$(ls -t ./*.deb 2>/dev/null | head -n1)
    echo -e "${GREEN}.deb written: ${BUILD_DIR}/${DEB_FILE}${NC}"
fi

cd - > /dev/null

# ---- Output ----
if [ "$IS_ANDROID" = true ]; then
    EXEC_PATH="${BUILD_DIR}/android-build/${EXECUTABLE_NAME}.apk"
    echo -e "${GREEN}APK: $EXEC_PATH${NC}"
    read -p "Install to device? (y/N): " -n 1 -r; echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        adb install -r "$EXEC_PATH"
        if [ "$DO_MONITOR" = true ]; then
            echo -e "${YELLOW}Launch the app on the device — monitor will attach automatically.${NC}"
            monitor_logs
        fi
    fi
else
    EXEC_PATH="${BUILD_DIR}/${EXECUTABLE_NAME}"
    if [ -f "$EXEC_PATH" ]; then
        ln -sf "$EXEC_PATH" "${SYMLINK_NAME}"
    else
        echo -e "${RED}Executable not found: $EXEC_PATH${NC}"; exit 1
    fi

    COMPDB_PATH="${BUILD_DIR}/compile_commands.json"
    if [ -f "$COMPDB_PATH" ]; then
        ln -sf "$COMPDB_PATH" "compile_commands.json"
    fi
fi

echo -e "${GREEN}Done — ${PLATFORM} ${BUILD_TYPE}${NC}"
if [ "$IS_ANDROID" = false ]; then echo -e "${BLUE}Run: ./${SYMLINK_NAME}${NC}"; fi
