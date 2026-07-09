cmake_minimum_required(VERSION 3.16...3.21)

set(CMAKE_CXX_STANDARD 20)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_PROJECT_NAME "lovense-smart-object")

project(${CMAKE_PROJECT_NAME} VERSION 1.0.0)

if(WIN32)
  add_compile_options(/wd4100)
  add_compile_options(/wd5040)
  add_compile_options(/wd5054)
  add_compile_options(/wd4505)
  add_compile_options(/wd4245)
endif()


set(PLUGIN_AUTHOR "lovense")
set(MACOS_BUNDLEID ${CMAKE_PROJECT_NAME})


#lovense-core
include_directories(${CMAKE_SOURCE_DIR}/deps)
include_directories(${CMAKE_SOURCE_DIR}/liblovense-core)
set(LOVENSE_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../liblovense-core)
link_directories("${LOVENSE_LIBRARY_DIR}")

#lovense obs lib
include_directories(${CMAKE_SOURCE_DIR}/liblovense-obs)
set(LOVENSE_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../liblovense-obs)
link_directories("${LOVENSE_LIBRARY_DIR}")

#lovense websocket lib
include_directories(${CMAKE_SOURCE_DIR}/liblovense-websocket)
set(LOVENSE_WEBSERVER_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../liblovense-websocket)
link_directories("${LOVENSE_WEBSERVER_LIBRARY_DIR}")

if(OS_WINDOWS)
    set(DLIB_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rd/dlib-19.24/win)
    set(DLIB_SDK_INC_DIR ${DLIB_SDK_DIR}/include)    
    include_directories(${DLIB_SDK_INC_DIR})    
endif()

add_library(${CMAKE_PROJECT_NAME} MODULE)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
    main.cpp 
    so-common.hpp
    so-smart-filter.cpp
    so-server-handler.hpp
    so-server-handler.cpp
    so-dlib-impl.hpp
    so-dlib-impl.cpp
    so-detection-center.hpp
    so-detection-center.cpp
    so-helper.hpp
    so-helper.cpp
    so-proc-handler.hpp
    so-proc-handler.cpp    
)

IF(CMAKE_CL_64)
    set(BITS 64)
ELSE(CMAKE_CL_64)
    set(BITS 86)
ENDIF(CMAKE_CL_64)

if(OS_WINDOWS)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
                        debug ${DLIB_SDK_DIR}/lib/x${BITS}/dlib19.24.0_debug_64bit_msvc1929.lib
                        optimized ${DLIB_SDK_DIR}/lib/x${BITS}/dlib19.24.0_release_64bit_msvc1929.lib )

    set(OPENCV_DIR  ${CMAKE_SOURCE_DIR}/deps/opencv/win)
    include_directories(${OPENCV_DIR}/include)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
                        debug ${OPENCV_DIR}/x${BITS}/Debug/opencv_world460d.lib
                        optimized ${OPENCV_DIR}/x${BITS}/Release/opencv_world460.lib )

    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
                        debug ${OPENCV_DIR}/x${BITS}/Debug/libpngd.lib
                        optimized ${OPENCV_DIR}/x${BITS}/Release/libpng.lib )

    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
                        debug ${OPENCV_DIR}/x${BITS}/Debug/libjpeg-turbod.lib
                        optimized ${OPENCV_DIR}/x${BITS}/Release/libjpeg-turbo.lib )
else()

  if(NOT MSVC AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
      set(BUILD_ARM64_FIX "arm64")
  else()
      set(BUILD_ARM64_FIX "x86_64")
  endif()

  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/agora_sdk/mac/cmake")
  find_package(AgoraSDK REQUIRED)
  foreach(ITEM IN LISTS AGO_FRAMEWORKS_LIST)
    get_filename_component(FILE_COMPNENT ${ITEM} NAME_WE)
    find_library(FRAMEWORK ${FILE_COMPNENT} PATHS ${AGORA_SDK_DIR} NO_CACHE NO_DEFAULT_PATH) 
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${FRAMEWORK})
    unset(FRAMEWORK)
  endforeach(ITEM AGO_FRAMEWORKS_LIST)
  
  set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/open_cv/mac/${BUILD_ARM64_FIX}/lib/cmake/opencv4")
  find_package(OpenCV REQUIRED)
  include_directories(${OpenCV_INCLUDE_DIRS}) # Not needed for CMake >= 2.8.11
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/open_cv/mac/${BUILD_ARM64_FIX}/lib/libopencv_world.a")
 
  file(GLOB OPENCV_LIB_LIST "${CMAKE_CURRENT_SOURCE_DIR}/open_cv/mac/${BUILD_ARM64_FIX}/lib/3rdparty/*.a")
  foreach(PARTYLIB IN LISTS OPENCV_LIB_LIST)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "${PARTYLIB}")
  endforeach(PARTYLIB IN LISTS OPENCV_LIB_LIST)
  
  # Open CV should depend OpenCL.frameworks Accelerate.frameworks Foundation
  find_library(OPENCL OpenCL)
  find_library(ACCELERATE Accelerate)
  find_library(FOUNDATION Foundation)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        ${OPENCL}
        ${ACCELERATE}
        ${FOUNDATION})

endif()


#if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin|iOS")
#  foreach(flags CMAKE_CXX_FLAGS CMAKE_OBJC_FLAGS CMAKE_OBJCXX_FLAGS)
#    string(APPEND ${flags} " -fvisibility=hidden -fvisibility-inlines-hidden")
#  endforeach()
#endif()

#add_definitions(-DUSE_UMAT)

# --- Platform-independent build settings ---
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)
# --- End of section ---

# --- Windows-specific build settings and tasks ---
#if(OS_WINDOWS)
#elseif(OS_MACOS)
#  configure_file(cmake/bundle/macos/installer-macos.pkgproj.in
#                 ${CMAKE_BINARY_DIR}/installer-macos.generated.pkgproj)
#
#  set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
#  set(MACOSX_PLUGIN_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}")
#  set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")
#
#  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall)
#  # --- End of section ---
#
#  # --- Linux-specific build settings and tasks ---
#else()
#  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall)
#endif()
# --- End of section ---

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  OBS::frontend-api
 )

 target_link_libraries(
    ${CMAKE_PROJECT_NAME}
  PRIVATE
    Lovense::lib_lovense_core
    Lovense::lib_lovense_obs
    Lovense::lib_lovense_websocket
)
  

set_target_properties(
    ${CMAKE_PROJECT_NAME} PROPERTIES FOLDER "plugins")


setup_plugin_target(${CMAKE_PROJECT_NAME})
