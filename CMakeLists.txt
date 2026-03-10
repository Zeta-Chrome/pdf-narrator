cmake_minimum_required(VERSION 3.21)
project(PDFNarrator VERSION 1.0.0 LANGUAGES CXX)

set(EXECUTABLE_NAME "pdfnarrator")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "" FORCE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Output directories
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

target_include_directories(${EXECUTABLE_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/include/pdf_narrator
)

set_source_files_properties(ui/Style.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt6_add_qml_module(${EXECUTABLE_NAME}
    URI "PDFNarrator"
    VERSION 1.0
    QML_FILES ${QML_FILES}
    SOURCES ${SOURCES} ${HEADERS}
    RESOURCES ${IMAGE_ASSETS}
)

# Link Qt libraries
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Multimedia
    Qt6::Concurrent
)

# ==================== MUPDF ====================
if(ANDROID)
    # Android: Use prebuilt libraries
    set(MUPDF_LIB_DIR ${CMAKE_SOURCE_DIR}/android/mupdf)
    
    target_include_directories(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/external/mupdf/include
    )
    
    add_library(mupdf SHARED IMPORTED)
    set_target_properties(mupdf PROPERTIES
        IMPORTED_LOCATION ${MUPDF_LIB_DIR}/libmupdf.so
    )
    
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE 
        mupdf
    )
    message("mupdf link successful")
else()
    # Linux/Windows: Use system-installed MuPDF
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MUPDF REQUIRED mupdf)
    
    target_include_directories(${EXECUTABLE_NAME} PUBLIC
        ${MUPDF_INCLUDE_DIRS}
    )
    
    target_link_libraries(${EXECUTABLE_NAME} PUBLIC
        ${MUPDF_LIBRARIES}
    )
endif()

# ==================== SHERPA-ONNX ====================
if(ANDROID)
    # Android: Use prebuilt libraries
    set(SHERPA_LIB_DIR ${CMAKE_SOURCE_DIR}/android/sherpa-onnx)
    
    target_include_directories(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/external/sherpa-onnx
    )
    
    add_library(onnxruntime SHARED IMPORTED)
    set_target_properties(onnxruntime PROPERTIES 
        IMPORTED_LOCATION ${SHERPA_LIB_DIR}/libonnxruntime.so
    )

    add_library(sherpa-onnx-c-api SHARED IMPORTED)
    set_target_properties(sherpa-onnx-c-api PROPERTIES 
        IMPORTED_LOCATION ${SHERPA_LIB_DIR}/libsherpa-onnx-c-api.so
    )
    
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        onnxruntime
        sherpa-onnx-c-api
    )
    message("sherpa-onnx link successful")
else()
    # Linux/Windows: Use prebuilt ONNX Runtime + build Sherpa-ONNX
    include(FetchContent)
    
    set(ONNXRUNTIME_VERSION "1.16.3")
    set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz")
    
    FetchContent_Declare(
        onnxruntime
        URL ${ONNXRUNTIME_URL}
    )
    FetchContent_MakeAvailable(onnxruntime)
    
    # Configure Sherpa-ONNX to use our ONNX Runtime
    set(ONNXRUNTIME_DIR "${onnxruntime_SOURCE_DIR}" CACHE PATH "" FORCE)
    set(SHERPA_ONNX_ENABLE_PYTHON OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_CHECK OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_PORTAUDIO OFF CACHE BOOL "" FORCE)
    
    add_subdirectory(${CMAKE_SOURCE_DIR}/external/sherpa-onnx)
    
    target_include_directories(${EXECUTABLE_NAME} PRIVATE
        ${onnxruntime_SOURCE_DIR}/include
    )
    
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE 
        sherpa-onnx-c-api
        ${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so.${ONNXRUNTIME_VERSION}
    )
    
    # Copy ONNX Runtime shared library to output directory
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so.${ONNXRUNTIME_VERSION}
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            libonnxruntime.so.${ONNXRUNTIME_VERSION}
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libonnxruntime.so
        COMMENT "Copying ONNX Runtime library"
    )
endif()

# Copy model files to build directory
add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/model
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/model
    COMMENT "Copying TTS models to build directory"
)

# Android specific configuration
if(ANDROID)
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        QT_ANDROID_APP_NAME "PDFNarrator"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/android
    )
    
    # Deploy native libraries to APK
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        QT_ANDROID_EXTRA_LIBS
            "${MUPDF_LIB_DIR}/libmupdf.so;${SHERPA_LIB_DIR}/libsherpa-onnx-c-api.so;${SHERPA_LIB_DIR}/libonnxruntime.so"
    )
    
    add_dependencies(${EXECUTABLE_NAME} mupdf sherpa-onnx-c-api)

    install(TARGETS ${EXECUTABLE_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
    )
endif()
