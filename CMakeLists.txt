cmake_minimum_required(VERSION 3.21)
project(PDFNarrator VERSION 1.0.0 LANGUAGES CXX)

set(EXECUTABLE_NAME "pdfnarrator")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "" FORCE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

set(SHERPA_ONNX_PREBUILT_VERSION "1.13.4")

set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.deps_cache" CACHE PATH "" FORCE)
include(FetchContent)

if (ANDROID)
    set(PLATFORM_NAME "android")
    set(ARCH_NAME "arm64_v8a")
else()
    set(PLATFORM_NAME "linux")
    set(ARCH_NAME "x86_64")
endif()

set(QT_DIR "$ENV{QT_BASE}/$ENV{ABI}")
list(APPEND CMAKE_PREFIX_PATH "${QT_DIR}/lib/cmake")
list(APPEND CMAKE_FIND_ROOT_PATH "${QT_DIR}")

find_package(Qt6Core)
find_package(Qt6Gui)
find_package(Qt6Qml)
find_package(Qt6Quick)
find_package(Qt6Multimedia)
find_package(Qt6Concurrent)

# Recursively find all source files
file(GLOB_RECURSE SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

# Recursively find all header files
file(GLOB_RECURSE HEADERS
    ${CMAKE_SOURCE_DIR}/include/pdf_narrator/*.h
)

# Recursively find all QML files
file(GLOB_RECURSE QML_FILES RELATIVE ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ui/*.qml
)

# Recursively find all image assets
file(GLOB_RECURSE IMAGE_ASSETS RELATIVE ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/assets/images/*.svg
)

# Create executable and QML module together
qt_add_executable(${EXECUTABLE_NAME} ${SOURCES} ${HEADERS})

target_compile_options(${EXECUTABLE_NAME} PRIVATE
    -Wall
    -Wextra
    -Wshadow
)

target_include_directories(${EXECUTABLE_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/include/pdf_narrator
)

set_source_files_properties(ui/Style.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt6_add_qml_module(${EXECUTABLE_NAME}
    URI "PDFNarrator"
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES ${QML_FILES}
    SOURCES ${SOURCES} ${HEADERS}
    RESOURCES ${IMAGE_ASSETS}
)

# Android specific configuration
if(ANDROID)
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        QT_ANDROID_APP_NAME "PDFNarrator"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/android
    )

    install(TARGETS ${EXECUTABLE_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
    )
endif()

# Link Qt libraries
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Multimedia
    Qt6::Concurrent
    z
)

# ================= Networking (Android TLS) ==================
if(ANDROID)
    FetchContent_Declare(
        android_openssl
        DOWNLOAD_EXTRACT_TIMESTAMP true
        URL https://github.com/KDAB/android_openssl/archive/refs/heads/master.zip
    )
    FetchContent_MakeAvailable(android_openssl)
    include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)

    add_android_openssl_libraries(${EXECUTABLE_NAME})

    file(GLOB TLS_PLUGINS "${QT_DIR}/plugins/tls/*.so")
    set_property(TARGET ${EXECUTABLE_NAME} APPEND PROPERTY
        QT_ANDROID_EXTRA_LIBS ${TLS_PLUGINS}
    )
endif()

# ==================== MUPDF ====================
if(ANDROID)
    set(MUPDF_SRC_DIR "${CMAKE_SOURCE_DIR}/external/mupdf")
    set(MUPDF_JAVA_DIR "${MUPDF_SRC_DIR}/platform/java")
    set(MUPDF_ANDROID_CACHE_DIR "${CMAKE_SOURCE_DIR}/.deps_cache/mupdf_android")
    set(MUPDF_ANDROID_LIB_DIR "${MUPDF_ANDROID_CACHE_DIR}/lib")
    set(MUPDF_ANDROID_INCLUDE "${MUPDF_ANDROID_CACHE_DIR}/include")
    set(MUPDF_ANDROID_ABI "arm64-v8a")
    set(MUPDF_ANDROID_API "27")
    set(MUPDF_ANDROID_STAMP "${MUPDF_ANDROID_CACHE_DIR}/.build_stamp")

    if(NOT EXISTS "${MUPDF_SRC_DIR}/include/mupdf/fitz.h")
        message(FATAL_ERROR
            "Expected MuPDF headers at ${MUPDF_SRC_DIR}/include/mupdf/fitz.h. "
            "Run: git submodule update --init --recursive inside external/mupdf")
    endif()

    if(NOT EXISTS "${MUPDF_JAVA_DIR}/Android.mk")
        message(FATAL_ERROR "Expected ${MUPDF_JAVA_DIR}/Android.mk but it wasn't found.")
    endif()

    if(NOT DEFINED ENV{ANDROID_NDK})
        message(FATAL_ERROR "ANDROID_NDK environment variable must be set.")
    endif()

    if(NOT EXISTS "$ENV{ANDROID_NDK}/ndk-build")
        message(FATAL_ERROR
            "ndk-build not found at $ENV{ANDROID_NDK}/ndk-build. "
            "Check that ANDROID_NDK points at the NDK root, not a subfolder.")
    endif()

    if(NOT EXISTS "${MUPDF_ANDROID_LIB_DIR}/libmupdf_core.a" OR NOT EXISTS "${MUPDF_ANDROID_STAMP}")
        message(STATUS "Building MuPDF static libs for Android via ndk-build (first time only, several minutes)...")

        execute_process(
            COMMAND $ENV{ANDROID_NDK}/ndk-build
                NDK_PROJECT_PATH=null
                APP_BUILD_SCRIPT=${MUPDF_JAVA_DIR}/Android.mk
                NDK_APPLICATION_MK=${MUPDF_JAVA_DIR}/Application.mk
                APP_ABI=${MUPDF_ANDROID_ABI}
                APP_PLATFORM=android-${MUPDF_ANDROID_API}
                NDK_LIBS_OUT=${MUPDF_JAVA_DIR}/libs
                NDK_OUT=${MUPDF_JAVA_DIR}/obj
                -j
            WORKING_DIRECTORY "${MUPDF_JAVA_DIR}"
            RESULT_VARIABLE MUPDF_BUILD_RESULT
            OUTPUT_VARIABLE MUPDF_BUILD_STDOUT
            ERROR_VARIABLE MUPDF_BUILD_STDERR
        )

        message(STATUS "---- ndk-build stdout ----\n${MUPDF_BUILD_STDOUT}")
        message(STATUS "---- ndk-build stderr ----\n${MUPDF_BUILD_STDERR}")

        if(NOT MUPDF_BUILD_RESULT EQUAL 0)
            message(FATAL_ERROR "MuPDF ndk-build failed (exit ${MUPDF_BUILD_RESULT}). See output above.")
        endif()

        set(MUPDF_BUILT_OBJ_DIR "${MUPDF_JAVA_DIR}/obj/local/${MUPDF_ANDROID_ABI}")
        file(GLOB MUPDF_BUILT_STATIC_LIBS "${MUPDF_BUILT_OBJ_DIR}/*.a")
        list(LENGTH MUPDF_BUILT_STATIC_LIBS MUPDF_BUILT_LIB_COUNT)

        if(MUPDF_BUILT_LIB_COUNT EQUAL 0)
            message(FATAL_ERROR
                "ndk-build reported success but no .a files were found under "
                "${MUPDF_BUILT_OBJ_DIR}. Contents of ${MUPDF_JAVA_DIR}/obj:")
        endif()

        list(FILTER MUPDF_BUILT_STATIC_LIBS EXCLUDE REGEX ".*mupdf_java.*")

        file(MAKE_DIRECTORY "${MUPDF_ANDROID_LIB_DIR}")
        file(COPY ${MUPDF_BUILT_STATIC_LIBS} DESTINATION "${MUPDF_ANDROID_LIB_DIR}")

        file(MAKE_DIRECTORY "${MUPDF_ANDROID_INCLUDE}")
        file(COPY "${MUPDF_SRC_DIR}/include/" DESTINATION "${MUPDF_ANDROID_INCLUDE}")

        file(WRITE "${MUPDF_ANDROID_STAMP}" "built")

        list(LENGTH MUPDF_BUILT_STATIC_LIBS MUPDF_CACHED_LIB_COUNT)
        message(STATUS "MuPDF Android build cached: ${MUPDF_CACHED_LIB_COUNT} static libs at ${MUPDF_ANDROID_LIB_DIR}")
    else()
        message(STATUS "MuPDF android libs already cached — skipping build")
    endif()

    target_include_directories(${EXECUTABLE_NAME} PRIVATE
        ${MUPDF_ANDROID_INCLUDE}
    )

    file(GLOB MUPDF_ANDROID_STATIC_LIBS "${MUPDF_ANDROID_LIB_DIR}/*.a")

    if(NOT MUPDF_ANDROID_STATIC_LIBS)
        message(FATAL_ERROR
            "No cached MuPDF static libs at ${MUPDF_ANDROID_LIB_DIR}. "
            "Delete ${MUPDF_ANDROID_CACHE_DIR} and re-run cmake.")
    endif()

    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        -Wl,--start-group
        ${MUPDF_ANDROID_STATIC_LIBS}
        -Wl,--end-group
    )

    message("mupdf link successful (android, static libs via ndk-build, cached at ${MUPDF_ANDROID_CACHE_DIR})")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MUPDF REQUIRED mupdf)

    target_include_directories(${EXECUTABLE_NAME} PUBLIC ${MUPDF_INCLUDE_DIRS})
    target_link_libraries(${EXECUTABLE_NAME} PUBLIC ${MUPDF_LIBRARIES})
endif()

# ==================== SHERPA-ONNX ====================
set(SHERPA_ONNX_SRC_DIR ${CMAKE_SOURCE_DIR}/external/sherpa-onnx)

if(NOT EXISTS "${SHERPA_ONNX_SRC_DIR}/sherpa-onnx/c-api/c-api.h")
    message(FATAL_ERROR
        "Expected sherpa-onnx headers at "
        "${SHERPA_ONNX_SRC_DIR}/sherpa-onnx/c-api/c-api.h but they weren't "
        "found. Make sure external/sherpa-onnx is a full clone of "
        "https://github.com/k2-fsa/sherpa-onnx (tag v${SHERPA_ONNX_PREBUILT_VERSION} recommended).")
endif()

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${SHERPA_ONNX_SRC_DIR}
)

if(ANDROID)
    set(SHERPA_ANDROID_RAW_LIB   "${SHERPA_ONNX_SRC_DIR}/build-android-arm64-v8a/install/lib")
    set(SHERPA_ANDROID_CACHE_LIB "${CMAKE_SOURCE_DIR}/.deps_cache/sherpa_onnx_android/lib")

    if(NOT EXISTS "${SHERPA_ANDROID_CACHE_LIB}/libonnxruntime.so")

        if(NOT EXISTS "${SHERPA_ANDROID_RAW_LIB}/libonnxruntime.so")
            message(STATUS "Building sherpa-onnx for Android (first time only, several minutes)...")

            execute_process(
                COMMAND ${CMAKE_COMMAND} -E env
                    ANDROID_NDK=$ENV{ANDROID_NDK}
                    SHERPA_ONNX_ANDROID_PLATFORM=android-27
                    SHERPA_ONNX_ENABLE_C_API=ON
                    SHERPA_ONNX_ENABLE_JNI=OFF
                    bash build-android-arm64-v8a.sh
                WORKING_DIRECTORY "${SHERPA_ONNX_SRC_DIR}"
                RESULT_VARIABLE SHERPA_BUILD_RESULT
            )

            if(NOT SHERPA_BUILD_RESULT EQUAL 0)
                message(FATAL_ERROR
                    "sherpa-onnx android build failed. Try: "
                    "rm -rf ${SHERPA_ONNX_SRC_DIR}/build-android-arm64-v8a and re-run cmake.")
            endif()
        endif()

        # Copy into .deps_cache so this survives being tucked away from the repo tree
        file(MAKE_DIRECTORY "${SHERPA_ANDROID_CACHE_LIB}")
        file(GLOB SHERPA_ANDROID_RAW_LIBS "${SHERPA_ANDROID_RAW_LIB}/*.so")
        file(COPY ${SHERPA_ANDROID_RAW_LIBS} DESTINATION "${SHERPA_ANDROID_CACHE_LIB}")
    else()
        message(STATUS "sherpa-onnx android libs already cached — skipping build")
    endif()

    file(GLOB SHERPA_ANDROID_LIBS "${SHERPA_ANDROID_CACHE_LIB}/*.so")

    target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${SHERPA_ANDROID_LIBS})
    set_property(TARGET ${EXECUTABLE_NAME} APPEND PROPERTY
        QT_ANDROID_EXTRA_LIBS ${SHERPA_ANDROID_LIBS})

    message("sherpa-onnx link successful (android, built from source, cached at ${SHERPA_ANDROID_CACHE_LIB})")
else()
    # ---- Linux: unchanged ----
    set(SHERPA_ONNX_PREBUILT_URL
        "https://github.com/k2-fsa/sherpa-onnx/releases/download/v${SHERPA_ONNX_PREBUILT_VERSION}/sherpa-onnx-v${SHERPA_ONNX_PREBUILT_VERSION}-linux-x64-shared.tar.bz2"
    )

    FetchContent_Declare(
        sherpa_onnx_prebuilt
        URL ${SHERPA_ONNX_PREBUILT_URL}
    )
    FetchContent_MakeAvailable(sherpa_onnx_prebuilt)

    set(SHERPA_PREBUILT_DIR "${sherpa_onnx_prebuilt_SOURCE_DIR}")

    if(NOT EXISTS "${SHERPA_PREBUILT_DIR}/lib")
        message(FATAL_ERROR
            "sherpa-onnx prebuilt archive did not extract as expected — "
            "check ${SHERPA_PREBUILT_DIR} and verify SHERPA_ONNX_PREBUILT_URL "
            "still points at a valid release asset.")
    endif()

    file(GLOB SHERPA_PREBUILT_LIBS "${SHERPA_PREBUILT_DIR}/lib/*.so*")

    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        ${SHERPA_PREBUILT_LIBS}
    )
    message("sherpa-onnx link successful (linux, prebuilt)")

    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHERPA_PREBUILT_LIBS}
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
        COMMENT "Copying prebuilt sherpa-onnx/onnxruntime shared libraries"
    )
endif()

# ==================== Debian Packaging ====================
if(NOT ANDROID)
    install(TARGETS ${EXECUTABLE_NAME}
        RUNTIME DESTINATION bin
    )

    install(FILES ${SHERPA_PREBUILT_LIBS}
        DESTINATION lib/${EXECUTABLE_NAME}
    )

    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib/${EXECUTABLE_NAME}"
    )

    set(CPACK_PACKAGE_NAME "pdfnarrator")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_VENDOR "YourName")
    set(CPACK_PACKAGE_CONTACT "you@example.com")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PDF reader with text-to-speech narration")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/${EXECUTABLE_NAME}")

    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "you@example.com")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON) 
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT) 

    include(CPack)
endif()
