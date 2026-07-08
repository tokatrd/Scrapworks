project(lovense-relay-server)
if(APPLE)
    if(NOT MSVC AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        set(BUILD_ARM64 ON)
    else()
        set(BUILD_ARM64 OFF)
    endif()
endif()

if(BUILD_TOOLSETS AND APPLE AND BUILD_ARM64)
    set(PROJECT_NAME lovense-relay-server)
else()
    set(PROJECT_NAME lovense-relay-server)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_SUFFIX 64)
else()
    set(ARCH_SUFFIX 86)
endif()

add_compile_definitions(LOVENSE_CORE_ENABLE_UNZIP)

########enable http server
option(LOVENSE_ENABLE_HTTP_SERVER "enable http server" OFF)
if(LOVENSE_ENABLE_HTTP_SERVER)
  add_compile_definitions(ENABLE_HTTP_SERVER)
endif()

########enable ipc server
if(WIN32)
option(LOVENSE_ENABLE_IPC_SERVER "enable IPC server" ON)
else()
option(LOVENSE_ENABLE_IPC_SERVER "enable IPC server" OFF)
endif()
if(LOVENSE_ENABLE_IPC_SERVER)
  add_compile_definitions(ENABLE_IPC_SERVER)
endif()
##------------------------


################################################################################################
## 增加获取摄像头列表库
if(OS_WINDOWS)

add_compile_options(/wd4005)

##### toolset
if(BUILD_TOOLSETS)
add_library(libdshowcapture-external INTERFACE)
add_library(libdshowcapture INTERFACE)
add_library(OBS::libdshowcapture-external ALIAS libdshowcapture-external)
add_library(OBS::libdshowcapture ALIAS libdshowcapture)
endif()
##### end toolset

set(LIBDShowCaptureExternalSources
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/external/capture-device-support/Library/EGAVResult.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/external/capture-device-support/Library/ElgatoUVCDevice.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/external/capture-device-support/Library/win/EGAVHIDImplementation.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/external/capture-device-support/SampleCode/DriverInterface.cpp
)

##### toolset
if(BUILD_TOOLSETS)
target_sources(
  libdshowcapture-external
  INTERFACE ${LIBDShowCaptureExternalSources}
)
endif()

set(LIBDShowCaptureSources
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/dshowcapture.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/capture-filter.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/capture-filter.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/output-filter.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/output-filter.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshowcapture.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshowencode.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/device.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/device.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/device-vendor.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/encoder.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/encoder.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-base.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-base.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-demux.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-demux.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-device-defs.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-enum.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-enum.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-formats.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-formats.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-media-type.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-encoded-device.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/dshow-media-type.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/log.cpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/log.hpp
  ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/source/external/IVideoCaptureFilter.h
)

##### toolset
if(BUILD_TOOLSETS)
target_sources(
  libdshowcapture
  INTERFACE ${LIBDShowCaptureSources})
 endif()

##### toolset
if(BUILD_TOOLSETS)
target_include_directories(
  libdshowcapture
  INTERFACE
    ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture
    ${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/external/capture-device-support/Library
)
target_compile_definitions(libdshowcapture-external INTERFACE _UP_WINDOWS=1)
target_compile_definitions(libdshowcapture INTERFACE _UP_WINDOWS=1)
target_compile_options(libdshowcapture-external INTERFACE /wd4018)
endif()

source_group(
  "libdshowcapture-external"
  FILES
     ${LIBDShowCaptureExternalSources}
)

source_group(
  "libdshowcapture"
  FILES
   ${LIBDShowCaptureSources}
)

include_directories(${CMAKE_SOURCE_DIR}/plugins/win-dshow)
include_directories(${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture)
include_directories(${CMAKE_SOURCE_DIR}/plugins/win-dshow/libdshowcapture/external/capture-device-support/Library)
endif()
################################################################################################


include_directories("${CMAKE_SOURCE_DIR}/plugins/lovense-relay-server")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")


include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/deps/socket.io-client/src")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/deps/socket.io-client/lib/asio/asio/include")
link_directories(${CMAKE_CURRENT_BINARY_DIR}/../../deps/socket.io-client)

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

#if(WIN32)
#    add_compile_options(/wd4267)
#	add_compile_options(/wd4244)
#	add_compile_options(/wd4312)
#	add_compile_options(/wd4819)
#	add_compile_options(/wd4828)
#    add_compile_options(/wd4101)
#endif()

find_package(CURL REQUIRED)

if(APPLE)
  #link Boost
  set(LOVENSE_OSX_DEP_DIR ${CMAKE_SOURCE_DIR}/../Boost_Openssl)
  SET(BOOST_INCLUDE_DIR ${LOVENSE_OSX_DEP_DIR}/Boost/include)
  SET(BOOST_LIBRARY_DIR ${LOVENSE_OSX_DEP_DIR}/Boost/lib)


  #link OpenSSL
  if(${BUILD_ARM64})
      SET(OPENSSL_INCLUDE_DIR ${LOVENSE_OSX_DEP_DIR}/OpenSSLArm64/include)
      SET(OPENSSL_LIBRARY_DIR ${LOVENSE_OSX_DEP_DIR}/OpenSSLArm64/lib)
  else()
      SET(OPENSSL_INCLUDE_DIR ${LOVENSE_OSX_DEP_DIR}/OpenSSl/include)
      SET(OPENSSL_LIBRARY_DIR ${LOVENSE_OSX_DEP_DIR}/OpenSSl/lib)
  endif()

  include_directories(SYSTEM "${BOOST_INCLUDE_DIR}")
  link_directories("${BOOST_LIBRARY_DIR}")
  include_directories(SYSTEM "${OPENSSL_INCLUDE_DIR}")
  link_directories("${OPENSSL_LIBRARY_DIR}")
else()
  #zip warn
  add_compile_options(/wd5033)
  add_compile_options(/wd4311)
  add_compile_options(/wd4302)

  include_directories("${BOOST_INCLUDE_DIR}")
  link_directories("${BOOST_LIBRARY_DIR}")
  include_directories("${OPENSSL_INCLUDE_DIR}")
  link_directories("${OPENSSL_LIBRARY_DIR}")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DWIN32_LEAN_AND_MEAN /bigobj")
  
endif()


if(MSVC)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:LIBCMT")
endif()


add_library(${PROJECT_NAME} MODULE)

file(GLOB LOVENSE_REPLAY_SERVER_SOURCE *.h *.hpp *.cpp)
file(GLOB LOVENSE_REPLAY_SERVER_ws_server_SOURCE server/*.hpp server/*.h server/*.cpp)
file(GLOB LOVENSE_REPLAY_SERVER_clients_SOURCE client/*.hpp client/*.h client/*.cpp)

if(BUILD_TOOLSETS AND NOT APPLE)
file(GLOB LOVENSE_REPLAY_SERVER_toolsets_SOURCE toolset/*.hpp toolset/*.h toolset/*.cpp)
file(GLOB LOVENSE_REPLAY_SERVER_dep_zip_SOURCE ${CMAKE_SOURCE_DIR}/deps/zip/*)
endif()

if(LOVENSE_ENABLE_HTTP_SERVER)
file(GLOB LOVENSE_REPLAY_SERVER_http_server_SOURCE http/*.hpp http/*.h http/*.cpp)
endif()

if(LOVENSE_ENABLE_IPC_SERVER)
file(GLOB LOVENSE_REPLAY_SERVER_ipc_server_SOURCE ipc/*.hpp ipc/*.h ipc/*.cpp)
endif()

set(MODULE_DESCRIPTION "lovense relay server")


target_compile_definitions(
    ${PROJECT_NAME} PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
                    _CRT_NONSTDC_NO_WARNINGS)
                  

if(NOT BUILD_TOOLSETS)
# build stream master
target_compile_definitions(
    ${PROJECT_NAME} PRIVATE DEF_STREAM_MASTER
)
endif()


if(APPLE AND BUILD_TOOLSETS)
set (LOVENSE_mac_toolsets_SOURCE
    toolset/mac/SCPluginConfig.h
    toolset/mac/SCPluginConfig.mm
)
endif()

target_sources(${PROJECT_NAME} PRIVATE
  ${LOVENSE_REPLAY_SERVER_SOURCE}
  ${LOVENSE_REPLAY_SERVER_ws_server_SOURCE}
  ${LOVENSE_REPLAY_SERVER_clients_SOURCE}
  ${LOVENSE_REPLAY_SERVER_toolsets_SOURCE}
  ${LOVENSE_REPLAY_SERVER_dep_zip_SOURCE}
  ${LOVENSE_mac_toolsets_SOURCE}
  ${LOVENSE_REPLAY_SERVER_http_server_SOURCE}
  ${LOVENSE_REPLAY_SERVER_ipc_server_SOURCE}
)

add_dependencies(${PROJECT_NAME} sioclient)

source_group(
    "server"
    FILES 
    ${LOVENSE_REPLAY_SERVER_ws_server_SOURCE})
    
source_group(
    "client"
    FILES 
    ${LOVENSE_REPLAY_SERVER_clients_SOURCE})
   
source_group(
    "toolset"
    FILES 
    ${LOVENSE_REPLAY_SERVER_toolsets_SOURCE})

source_group(
    "http"
    FILES 
    ${LOVENSE_REPLAY_SERVER_http_server_SOURCE})

source_group(
    "ipc"
    FILES 
    ${LOVENSE_REPLAY_SERVER_ipc_server_SOURCE})
   
source_group(
    "deps/zip"
    FILES 
    ${LOVENSE_REPLAY_SERVER_dep_zip_SOURCE})

if(MSVC)
#	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /NODEFAULTLIB:LIBCMT")
 target_link_options(${PROJECT_NAME} PRIVATE /WX:NO)
endif()

add_definitions(-D_HAS_STD_BYTE=0)

set(CMAKE_PREFIX_PATH "${QTDIR}")


### 用于获取摄像头列表
if(OS_WINDOWS)
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE
    setupapi
    strmiids
    ksuser
    winmm
    wmcodecdspuuid
    OBS::libdshowcapture-external
    OBS::libdshowcapture      
)
endif()
## end

##########################################################################
## toolsets->obs27
if (BUILD_OBS27)

target_link_libraries(${PROJECT_NAME} PRIVATE
    obs
    obs-frontend-api
#    w32-pthreads
    )
else()
##########################################################################

# obs 28 29 ....
target_link_libraries(${PROJECT_NAME} PRIVATE
    OBS::libobs    
    OBS::frontend-api
#    OBS::w32-pthreads
    )
endif()
#end
##########################################################################

if(WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE
		libcrypto
		libssl
    ${CMAKE_CURRENT_BINARY_DIR}/../../deps/dtx/windows/lib/x${ARCH_SUFFIX}/$<CONFIGURATION>/DXDtxUtils.lib
  )

IF(CMAKE_CL_64)
    set(BITS 64)
ELSE(CMAKE_CL_64)
    set(BITS 32)
ENDIF(CMAKE_CL_64)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		"${OPENSSL_LIBRARY_DIR}/libcrypto-1_1-x64.dll"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/bin/${BITS}bit/libcrypto-1_1-x64.dll"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${OPENSSL_LIBRARY_DIR}/libssl-1_1-x64.dll"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/bin/${BITS}bit/libssl-1_1-x64.dll"
	)


if(BUILD_TOOLSETS)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD		
  COMMAND ${CMAKE_COMMAND} -E copy		
  "${CMAKE_BINARY_DIR}/plugins/lovense-relay-server/$<CONFIGURATION>/lovense-relay-server.dll"
        "C:\\ProgramData\\obs-studio\\plugins\\lovense-relay-server\\bin\\${BITS}bit\\lovense-relay-server.dll"
 )

  
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD		
  COMMAND ${CMAKE_COMMAND} -E copy		
	"${CMAKE_BINARY_DIR}/plugins/lovense-relay-server/$<CONFIGURATION>/lovense-relay-server.pdb"
      "C:\\ProgramData\\obs-studio\\plugins\\lovense-relay-server\\bin\\${BITS}bit\\lovense-relay-server.pdb"
  )

endif()

endif()
##----------------------------------------------------------------
if(APPLE)
    if(BUILD_ARM64)
        target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/deps/dtx/mac/lib/arm64/libdtx.a)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/deps/dtx/mac/lib/x86_64/libdtx.a)
    endif()
  target_link_libraries(${PROJECT_NAME} PRIVATE
    #${OPENSSL_LIBRARY_DIR}/libcrypto.a
    #${OPENSSL_LIBRARY_DIR}/libssl.a
    libsioclient.a
)

find_library(AVFOUNDATION AVFoundation)
    find_library(SECURITY Security)
    find_library(IOKIT IOKit)
    find_library(COREMEDIA CoreMedia)
    find_library(COREFOUNDATION CoreFoundation)
        find_library(COCOA CoCoa)
    mark_as_advanced(AVFOUNDATION SECURITY IOKIT COREMEDIA COREFOUNDATION COCOA)

    target_link_libraries(${PROJECT_NAME} PRIVATE ${SECURITY} ${AVFOUNDATION}
                                        ${IOKIT} ${COREMEDIA} ${COREFOUNDATION} ${COCOA})
endif()

target_link_libraries(
    ${PROJECT_NAME}
  PRIVATE
          Lovense::lib_lovense_core
          Lovense::lib_lovense_obs
          Lovense::lib_lovense_websocket
          CURL::libcurl
)

if(WIN32)
target_link_libraries(
    ${PROJECT_NAME}
  PRIVATE
          sioclient.lib
)
endif()


set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "plugins")

setup_plugin_target(${PROJECT_NAME})


