#!/bin/bash

SYMLINK_NAME="exec"
EXECUTABLE_NAME="pdfnarrator"
BUILD_TYPE="Debug"
IS_ANDROID=false
PLATFORM="PC"
FORCE_RECONFIGURE=false

# ---- Parse args ----
for arg in "$@"; do
    case "$arg" in
        DEBUG|debug|Debug)       BUILD_TYPE="Debug" ;;
        RELEASE|release|Release) BUILD_TYPE="Release" ;;
        ANDROID|android|Android) IS_ANDROID=true; PLATFORM="Android"; BUILD_TYPE="Release" ;;
        CLEAN|clean)             FORCE_RECONFIGURE=true ;;
        *)
            echo "Usage: ./build.sh [DEBUG|RELEASE] [ANDROID] [CLEAN]"
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

# ---- Set Qt base path ----
if [ -z "$QT_BASE" ]; then export QT_BASE="$HOME/Qt/6.10.2"; fi
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
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR" || exit 1

# ---- Configure (only if not already configured) ----
if [ ! -f "CMakeCache.txt" ]; then
    echo -e "${YELLOW}Configuring...${NC}"

    if [ "$IS_ANDROID" = true ]; then
        if [ -z "$QT_HOST_PATH" ]; then export QT_HOST_PATH="$HOME/Qt/6.10.2/gcc_64"; fi
        if [ -z "$ANDROID_NDK" ];   then export ANDROID_NDK="$HOME/Android/Sdk/ndk/28.2.13676358"; fi
        if [ -z "$ANDROID_SDK_ROOT" ]; then export ANDROID_SDK_ROOT="$HOME/Android/Sdk"; fi

        if [ ! -d "$ANDROID_NDK" ]; then echo -e "${RED}NDK not found: $ANDROID_NDK${NC}"; exit 1; fi

        if [ -z "$QT_ANDROID_KEYSTORE_PATH" ];      then export QT_ANDROID_KEYSTORE_PATH="$HOME/.android/debug.keystore"; fi
        if [ -z "$QT_ANDROID_KEYSTORE_ALIAS" ];     then export QT_ANDROID_KEYSTORE_ALIAS="androiddebugkey"; fi
        if [ -z "$QT_ANDROID_KEYSTORE_STORE_PASS" ]; then export QT_ANDROID_KEYSTORE_STORE_PASS="android"; fi
        if [ -z "$QT_ANDROID_KEYSTORE_KEY_PASS" ];  then export QT_ANDROID_KEYSTORE_KEY_PASS="android"; fi

        cmake \
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
        cmake \
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
cd - > /dev/null

# ---- Output ----
if [ "$IS_ANDROID" = true ]; then
    EXEC_PATH="${BUILD_DIR}/android-build/${EXECUTABLE_NAME}.apk"
    echo -e "${GREEN}APK: $EXEC_PATH${NC}"
    read -p "Install to device? (y/N): " -n 1 -r; echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then adb install -r "$EXEC_PATH"; fi
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
