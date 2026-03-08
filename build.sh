#!/bin/bash

SYMLINK_NAME="exec"
EXECUTABLE_NAME="pdfnarrator"
BUILD_TYPE="Debug"
IS_ANDROID=false
PLATFORM="PC"

# ---- Parse args ----
for arg in "$@"; do
    case "$arg" in
        DEBUG|debug|Debug)
            BUILD_TYPE="Debug"
            ;;
        RELEASE|release|Release)
            BUILD_TYPE="Release"
            ;;
        ANDROID|android|Android)
            IS_ANDROID=true
            PLATFORM="Android"
            BUILD_TYPE="Release"  # Android always uses Release
            ;;
        *)
            echo "Usage: ./build.sh [DEBUG|RELEASE] [ANDROID]"
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

echo -e "${YELLOW}Platform: ${PLATFORM}${NC}"
echo -e "${YELLOW}Build Type: ${BUILD_TYPE}${NC}\n"

# ---- Set Qt base path ----
if [ -z "$QT_BASE" ]; then
    export QT_BASE="$HOME/Qt/6.10.2"
fi

if [ ! -d "$QT_BASE" ]; then
    echo -e "${RED}Qt installation not found at: $QT_BASE${NC}"
    echo "Please install Qt or set QT_BASE environment variable"
    exit 1
fi

echo -e "${GREEN}Using Qt: $QT_BASE${NC}"

# ---- Set ABI and build directory ----
if [ "$IS_ANDROID" = true ]; then
    export ABI="android_arm64_v8a"
    BUILD_DIR="build/android-release"
else
    export ABI="gcc_64"
    if [ "$BUILD_TYPE" = "Release" ]; then
        BUILD_DIR="build/linux-release"
    else
        BUILD_DIR="build/linux-debug"
    fi
fi

echo -e "${GREEN}Using ABI: $ABI${NC}"
echo -e "${GREEN}Build Directory: $BUILD_DIR${NC}"

# Verify Qt directory exists
if [ ! -d "$QT_BASE/$ABI" ]; then
    echo -e "${RED}Qt directory not found: $QT_BASE/$ABI${NC}"
    echo "Available directories in $QT_BASE:"
    ls -1 "$QT_BASE" | grep -v "sha1s.txt"
    exit 1
fi

# ---- Create build directory ----
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR" || exit 1

# ---- Configure ----
echo -e "${YELLOW}Configuring CMake...${NC}"

if [ "$IS_ANDROID" = true ]; then
    # Android build
    if [ -z "$QT_HOST_PATH" ]; then
        export QT_HOST_PATH="$HOME/Qt/6.10.2/gcc_64"
    fi

    if [ -z "$ANDROID_NDK" ]; then
        export ANDROID_NDK="$HOME/Android/Sdk/ndk/28.2.13676358"
    fi 
    
    if [ -z "$ANDROID_SDK_ROOT" ]; then
        export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
    fi
    
    # Verify paths exist
    if [ ! -d "$ANDROID_NDK" ]; then
        echo -e "${RED}Android NDK not found at: $ANDROID_NDK${NC}"
        echo "Please install Android NDK or set ANDROID_NDK environment variable"
        exit 1
    fi
    
    if [ ! -d "$QT_HOST_PATH" ]; then
        echo -e "${RED}Qt host tools not found at: $QT_HOST_PATH${NC}"
        echo "Please install Qt or set QT_HOST_PATH environment variable"
        exit 1
    fi
    
    echo -e "${GREEN}Using Android NDK: $ANDROID_NDK${NC}"
    echo -e "${GREEN}Using Qt Host: $QT_HOST_PATH${NC}"
    echo -e "${GREEN}Using Android SDK: $ANDROID_SDK_ROOT${NC}"
    
    # ---- Android Keystore Configuration ----
    # For debug builds, use Android's default debug keystore
    if [ -z "$QT_ANDROID_KEYSTORE_PATH" ]; then
        export QT_ANDROID_KEYSTORE_PATH="$HOME/.android/debug.keystore"
    fi
    
    if [ -z "$QT_ANDROID_KEYSTORE_ALIAS" ]; then
        export QT_ANDROID_KEYSTORE_ALIAS="androiddebugkey"
    fi
    
    if [ -z "$QT_ANDROID_KEYSTORE_STORE_PASS" ]; then
        export QT_ANDROID_KEYSTORE_STORE_PASS="android"
    fi
    
    if [ -z "$QT_ANDROID_KEYSTORE_KEY_PASS" ]; then
        export QT_ANDROID_KEYSTORE_KEY_PASS="android"
    fi
    
    echo -e "${GREEN}Using Keystore: $QT_ANDROID_KEYSTORE_PATH${NC}"
    echo -e "${GREEN}Keystore Alias: $QT_ANDROID_KEYSTORE_ALIAS${NC}"
    
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
    # PC build (Linux/Windows)
    cmake \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        ../.. || exit 1
fi

# ---- Build ----
echo -e "${YELLOW}Building...${NC}"
cmake --build . -j$(nproc) || exit 1
cd - > /dev/null

# ---- Paths and output handling ----
if [ "$IS_ANDROID" = true ]; then
    # Android output (APK file)
    EXEC_PATH="${BUILD_DIR}/android-build/${EXECUTABLE_NAME}.apk"
    echo -e "${GREEN}Android APK built:${NC} $EXEC_PATH"
    
    # Install APK to connected device (optional)
    read -p "Install APK to connected device? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        adb install -r "$EXEC_PATH"
    fi
else
    # PC output (executable)
    EXEC_PATH="${BUILD_DIR}/${EXECUTABLE_NAME}"
    if [ -f "$EXEC_PATH" ]; then
        ln -sf "$EXEC_PATH" "${SYMLINK_NAME}"
        echo -e "${GREEN}Symlinked executable:${NC} ${SYMLINK_NAME} -> $EXEC_PATH"
    else
        echo -e "${RED}Executable not found:${NC} $EXEC_PATH"
        echo -e "${YELLOW}Check if build succeeded${NC}"
    fi
    
    # Symlink compile_commands.json for IDE support
    COMPDB_PATH="${BUILD_DIR}/compile_commands.json"
    if [ -f "$COMPDB_PATH" ]; then
        ln -sf "$COMPDB_PATH" "compile_commands.json"
        echo -e "${GREEN}Symlinked compile_commands.json${NC}"
    else
        echo -e "${YELLOW}compile_commands.json not found${NC}"
    fi
fi

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Build Successful${NC}"
echo -e "${GREEN}  Platform: ${PLATFORM}${NC}"
echo -e "${GREEN}  Build Type: ${BUILD_TYPE}${NC}"
echo -e "${GREEN}  Build Directory: ${BUILD_DIR}${NC}"
echo -e "${GREEN}========================================${NC}"

if [ "$IS_ANDROID" = false ]; then
    echo -e "${BLUE}Run:${NC} ./${SYMLINK_NAME}"
fi
echo ""
