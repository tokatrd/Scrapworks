cmake_minimum_required(VERSION 3.5)

if(APPLE)
    if(NOT MSVC AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        set(BUILD_ARM64 ON)
    else()
        set(BUILD_ARM64 OFF)
    endif()
endif()
if(BUILD_TOOLSETS)
	set(PROJECT_NAME lovense-obs-websocket)
	if(APPLE AND BUILD_ARM64)
		set(PROJECT_NAME lovense-obs-websocket-arm64)
	endif()
else()
	set(PROJECT_NAME obs-websocket)
endif()

project(${PROJECT_NAME} VERSION 4.9.1)

if(NOT BUILD_TOOLSETS)
set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DASIO_STANDALONE)

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm") 
    set(CMAKE_CXX_FLAGS "-mfpu=neon")
endif()


#if (WIN32 OR APPLE)
#	include(external/FindLibObs.cmake)
#endif()

# Find Qt
##---------------------------------------------------
if(NOT BUILD_OBS27)
find_qt(COMPONENTS Core Widgets Svg Network)
else()
##---------------------------------------------------
if(NOT BUILD_TOOLSETS)
find_package(Qt5Widgets ${FIND_MODE})
find_package(Qt5Svg ${FIND_MODE})
find_package(Qt5Xml ${FIND_MODE})
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
endif()

# find_package(Libcurl REQUIRED)
include_directories(${LIBCURL_INCLUDE_DIRS})
add_definitions(${LIBCURL_DEFINITIONS})
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")

if(BUILD_TOOLSETS)
else()
include_directories(
	${Qt5Core_INCLUDES}
	${Qt5Widgets_INCLUDES}
  )
endif()

endif()

##-----------------------------------------------------------------------------
include_directories(
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/asio/asio/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/websocketpp")

include_directories(${CMAKE_SOURCE_DIR}/deps)

#lovense-core
include_directories(${CMAKE_SOURCE_DIR}/liblovense-core)
include_directories(${CMAKE_SOURCE_DIR}/common)
set(LOVENSE_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../liblovense-core)
link_directories("${LOVENSE_LIBRARY_DIR}")

#lovense obs lib
include_directories(${CMAKE_SOURCE_DIR}/liblovense-obs)
set(LOVENSE_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../liblovense-obs)
link_directories("${LOVENSE_LIBRARY_DIR}")

set(obs-websocket_SOURCES
	src/obs-websocket.cpp
	src/WSServer.cpp
	src/ConnectionProperties.cpp
	src/WSRequestHandler.cpp
	src/WSRequestHandler_General.cpp
	src/WSRequestHandler_Profiles.cpp
	src/WSRequestHandler_Recording.cpp
	src/WSRequestHandler_ReplayBuffer.cpp
	src/WSRequestHandler_SceneCollections.cpp
	src/WSRequestHandler_Scenes.cpp
	src/WSRequestHandler_SceneItems.cpp
	src/WSRequestHandler_Sources.cpp
	src/WSRequestHandler_Streaming.cpp
	src/WSRequestHandler_VirtualCam.cpp
	src/WSRequestHandler_StudioMode.cpp
	src/WSRequestHandler_Transitions.cpp
	src/WSRequestHandler_Outputs.cpp
	src/WSRequestHandler_MediaControl.cpp
	src/WSEvents.cpp
	src/Config.cpp
	src/Utils.cpp
	src/rpc/RpcRequest.cpp
	src/rpc/RpcResponse.cpp
	src/rpc/RpcEvent.cpp
	src/protocol/OBSRemoteProtocol.cpp
  src/QtAdapter.cpp
	#src/forms/settings-dialog.cpp
  )

set(obs-websocket_HEADERS
	src/obs-websocket.h
	src/WSServer.h
	src/ConnectionProperties.h
	src/WSRequestHandler.h
	src/WSEvents.h
	src/Config.h
	src/Utils.h
	src/rpc/RpcRequest.h
	src/rpc/RpcResponse.h
	src/rpc/RpcEvent.h
	src/protocol/OBSRemoteProtocol.h
  src/QtAdapter.h
	#src/forms/settings-dialog.h
 )

# --- Platform-independent build settings ---
add_library(${PROJECT_NAME} MODULE
	${obs-websocket_SOURCES}
	${obs-websocket_HEADERS})
add_library(OBS::websocket ALIAS ${PROJECT_NAME})


## obs > 27
if(NOT BUILD_OBS27)

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE OBS::libobs
          OBS::frontend-api
          Qt::Core
          Qt::Widgets
          Qt::Svg
          Qt::Network
          Lovense::lib_lovense_core
          Lovense::lib_lovense_obs
          )
else()
##--------obs <= 27
if(BUILD_TOOLSETS)
target_link_libraries(${PROJECT_NAME}
  ${LIBCURL_LIBRARIES}
  obs
  obs-frontend-api
  Lovense::lib_lovense_core
  Lovense::lib_lovense_obs
)
else()
target_link_libraries(${PROJECT_NAME}
  ${LIBCURL_LIBRARIES}
  libobs
  obs-frontend-api
  Qt5::Core
  Qt5::Widgets
  Lovense::lib_lovense_core
  Lovense::lib_lovense_obs
)
endif()

endif()

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(WIN32)
    
#	set(OBS_FRONTEND_LIB "NOTFOUND" CACHE FILEPATH "OBS frontend library")
#	if(OBS_FRONTEND_LIB STREQUAL "NOTFOUND")
#		message(FATAL_ERROR "OBS Frontend API library not found!")
#	endif()
#
    if(MSVC)
        # Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL)
	    add_definitions(/MP /d2FH4-)
        add_definitions(-DWIN32_LEAN_AND_MEAN)
    endif()

	add_definitions(-D_WEBSOCKETPP_CPP11_STL_)

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(ARCH_NAME "64bit")
		set(OBS_BUILDDIR_ARCH "build64")
	else()
		set(ARCH_NAME "32bit")
		set(OBS_BUILDDIR_ARCH "build32")
	endif()

	include_directories(
		"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/UI"
	)

#	target_link_libraries(${PROJECT_NAME}
#		"${OBS_FRONTEND_LIB}")
#
	# --- Release package helper ---
	# The "release" folder has a structure similar OBS' one on Windows
#if(BUILD_OBS27)
#	set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")
#	
#	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#		# If config is Release or RelWithDebInfo, package release files
#		COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
#			"${CMAKE_COMMAND}" -E make_directory
#			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
#		)
#	
#		COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
#			"${CMAKE_COMMAND}" -E copy_directory
#			"${PROJECT_SOURCE_DIR}/data"
#			"${RELEASE_DIR}/data/obs-plugins/${PROJECT_NAME}"
#		)
#	
#		COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
#			"${CMAKE_COMMAND}" -E copy
#			"$<TARGET_FILE:${PROJECT_NAME}>"
#			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
#		)
#	
#		# In Release or RelWithDebInfo mode, copy Qt image format plugins
#		COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
#			"${CMAKE_COMMAND}" -E make_directory
#			"${RELEASE_DIR}/bin/${ARCH_NAME}/imageformats"
#		)
#		COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
#			"${CMAKE_COMMAND}" -E copy
#			"${QTDIR}/plugins/imageformats/qicns.dll"
#			"${QTDIR}/plugins/imageformats/qico.dll"
#			"${QTDIR}/plugins/imageformats/qjpeg.dll"
#			"${QTDIR}/plugins/imageformats/qtiff.dll"
#			"${QTDIR}/plugins/imageformats/qwbmp.dll"
#			"${QTDIR}/plugins/imageformats/qwebp.dll"
#			"${RELEASE_DIR}/bin/${ARCH_NAME}/imageformats"
#		)
#	
#		# If config is RelWithDebInfo, package PDB file for target
#		COMMAND if $<CONFIG:RelWithDebInfo>==1 (
#			"${CMAKE_COMMAND}" -E copy
#			"$<TARGET_PDB_FILE:${PROJECT_NAME}>"
#			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
#		)
#	
#		# In the Debug configuration, copy to obs-studio dev environment for immediate testing
#		COMMAND if $<CONFIG:Debug>==1 (
#			"${CMAKE_COMMAND}" -E copy
#				"$<TARGET_FILE:${PROJECT_NAME}>"
#				"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
#		)
#	
#		COMMAND if $<CONFIG:Debug>==1 (
#			"${CMAKE_COMMAND}" -E copy
#			"$<TARGET_PDB_FILE:${PROJECT_NAME}>"
#			"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
#		)
#	
#		COMMAND if $<CONFIG:Debug>==1 (
#			"${CMAKE_COMMAND}" -E make_directory
#			"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${PROJECT_NAME}"
#		)
#	
#		COMMAND if $<CONFIG:Debug>==1 (
#			"${CMAKE_COMMAND}" -E copy_directory
#			"${PROJECT_SOURCE_DIR}/data"
#			"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${PROJECT_NAME}"
#		)
#	)
#endif()
	# --- End of sub-section ---

endif()
# --- End of section ---

# --- Linux-specific build settings and tasks ---
if(UNIX AND NOT APPLE)
	include(GNUInstallDirs)

	file(GLOB locale_files data/locale/*.ini)

    set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
		OWNER_READ OWNER_WRITE OWNER_EXECUTE
		GROUP_READ GROUP_WRITE GROUP_EXECUTE
		WORLD_READ WORLD_EXECUTE
	)

	if(${USE_UBUNTU_FIX})
		install(TARGETS ${PROJECT_NAME} LIBRARY
			DESTINATION "/usr/lib/obs-plugins"
			PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
	endif()
	install(TARGETS ${PROJECT_NAME} LIBRARY
		DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/obs-plugins"
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

	install(FILES ${locale_files}
		DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/obs/obs-plugins/${PROJECT_NAME}/locale")
endif()
# --- End of section ---

# -- OS X specific build settings and tasks --
if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

	set(CMAKE_SKIP_RPATH TRUE)
#	set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
#	target_link_libraries(${PROJECT_NAME} "${OBS_FRONTEND_LIB}")
endif()
# -- End of section --

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "plugins")


if(NOT BUILD_OBS27)
setup_plugin_target(${PROJECT_NAME})
setup_target_resources(${PROJECT_NAME} "obs-plugins/${PROJECT_NAME}")
else()

install_obs_plugin_with_data(${PROJECT_NAME} data)

  if(BUILD_TOOLSETS)
    if(WIN32)
    IF(CMAKE_CL_64)
        set(BITS 64)
    ELSE(CMAKE_CL_64)
        set(BITS 32)
    ENDIF(CMAKE_CL_64)
      add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD		
		      COMMAND ${CMAKE_COMMAND} -E copy		
		      "${CMAKE_BINARY_DIR}/plugins/obs-websocket/$<CONFIGURATION>/${PROJECT_NAME}.dll"
              "C:\\ProgramData\\obs-studio\\plugins\\${PROJECT_NAME}\\bin\\${BITS}bit\\${PROJECT_NAME}.dll"
	      )

      add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD		
        COMMAND ${CMAKE_COMMAND} -E copy		
		    "${CMAKE_BINARY_DIR}/plugins/obs-websocket/$<CONFIGURATION>/${PROJECT_NAME}.pdb"
            "C:\\ProgramData\\obs-studio\\plugins\\${PROJECT_NAME}\\bin\\${BITS}bit\\${PROJECT_NAME}.pdb"
	    )
    endif()

  endif()

endif()
