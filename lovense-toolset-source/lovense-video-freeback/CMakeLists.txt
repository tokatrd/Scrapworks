project(lovense-video-freedback)


if(APPLE)
    if(NOT MSVC AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        set(BUILD_ARM64 ON)
    else()
        set(BUILD_ARM64 OFF)
    endif()
endif()

if(BUILD_TOOLSETS AND APPLE AND BUILD_ARM64)
  set(PROJECT_NAME lovense-video-freedback)
endif()

#
#option(
#  ENABLE_BROWSER
#  "Enable building OBS with browser source plugin (required Chromium Embedded Framework)"
#  ${OS_LINUX})
#
#if(NOT ENABLE_BROWSER OR NOT ENABLE_UI)
#  message(STATUS "OBS:  DISABLED   obs-browser")
#  message(
#    WARNING
#      "Browser source support is not enabled by default - please switch ENABLE_BROWSER to ON and specify CEF_ROOT_DIR to enable this functionality."
#  )
#  return()
#endif()
#

if(OS_WINDOWS)
  option(ENABLE_BROWSER_LEGACY
         "Use legacy CEF version 3770 for browser source plugin" OFF)
endif()

if(OS_MACOS OR OS_WINDOWS)
  option(ENABLE_BROWSER_SHARED_TEXTURE
         "Enable shared texture support for browser source plugin" ON)
endif()

if(WIN32)
IF(CMAKE_CL_64)
    set(BITS_LABEL 64)
ELSE(CMAKE_CL_64)
    set(BITS_LABEL 86)
ENDIF(CMAKE_CL_64)
endif()

if (NOT BUILD_OBS27)
option(ENABLE_BROWSER_PANELS "Enable Qt web browser panel support" ON)
option(ENABLE_BROWSER_QT_LOOP
       "Enable running CEF on the main UI thread alongside Qt" ${OS_MACOS})
endif()

if (NOT BUILD_OBS27)
mark_as_advanced(ENABLE_BROWSER_LEGACY ENABLE_BROWSER_SHARED_TEXTURE
                 ENABLE_BROWSER_PANELS ENABLE_BROWSER_QT_LOOP)
else()
mark_as_advanced(ENABLE_BROWSER_LEGACY ENABLE_BROWSER_SHARED_TEXTURE)
endif()

find_package(CEF REQUIRED)

if(NOT TARGET CEF::Wrapper)
  message(
    FATAL_ERROR
      "OBS:    -        Unable to find CEF Libraries - set CEF_ROOT_DIR or configure with ENABLE_BROWSER=OFF"
  )
endif()

add_library(${PROJECT_NAME} MODULE)
# add_library(OBS::browser ALIAS lovense-video-freedback)

if(ENABLE_BROWSER_LEGACY)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_BROWSER_LEGACY)
endif()

if(ENABLE_BROWSER_SHARED_TEXTURE)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_BROWSER_SHARED_TEXTURE)
endif()

if(ENABLE_BROWSER_QT_LOOP)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_BROWSER_QT_LOOP)
endif()

#lovense-core
include_directories(${CMAKE_SOURCE_DIR}/deps)
include_directories(${CMAKE_SOURCE_DIR}/liblovense-core)
set(LOVENSE_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../liblovense-core)
link_directories("${LOVENSE_LIBRARY_DIR}")

#lovense obs lib
include_directories(${CMAKE_SOURCE_DIR}/liblovense-obs)
set(LOVENSE_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../liblovense-obs)
link_directories("${LOVENSE_LIBRARY_DIR}")


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/browser-config.h.in
               ${CMAKE_BINARY_DIR}/config/browser-config.h)

if (BUILD_TOOLSETS AND OS_WINDOWS)
file(GLOB LOVENSE_TOOLSET_SOURCE toolset/*.h toolset/*.hpp toolset/*.cpp)
endif()

target_sources(
  ${PROJECT_NAME}
  PRIVATE obs-browser-plugin.cpp
          obs-browser-source.cpp
          obs-browser-source.hpp
          obs-browser-source-audio.cpp
          browser-app.cpp
          browser-app.hpp
          browser-client.cpp
          browser-client.hpp
          browser-scheme.cpp
          browser-scheme.hpp
          browser-version.h
          browser-proc-handler.hpp
          browser-proc-handler.cpp
          cef-headers.hpp
          deps/json11/json11.cpp
          deps/json11/json11.hpp
          deps/base64/base64.cpp
          deps/base64/base64.hpp
          deps/wide-string.cpp
          deps/wide-string.hpp
          deps/signal-restore.cpp
          deps/signal-restore.hpp
          deps/obs-websocket-api/obs-websocket-api.h
          ${CMAKE_BINARY_DIR}/config/browser-config.h
          ${LOVENSE_TOOLSET_SOURCE}
)

if(APPLE AND BUILD_TOOLSETS)
  target_sources(
    ${PROJECT_NAME}
    PRIVATE 
    mac/UserDefine.h
    mac/CPluginsUpdate.hpp
    mac/CPluginsUpdate.mm
  )
endif()
source_group(
    "toolset"
    FILES 
    ${LOVENSE_TOOLSET_SOURCE})
target_include_directories(
  ${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps
    ${CMAKE_SOURCE_DIR}/UI/lovense
    ${CMAKE_BINARY_DIR}/config
   )

#lovense-core
include_directories(${CMAKE_SOURCE_DIR}/liblovense-core)
include_directories(${CMAKE_SOURCE_DIR}/common)
#set(LOVENSE_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../liblovense-core)
#link_directories("${LOVENSE_LIBRARY_DIR}")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})


if (BUILD_OBS27)
target_link_libraries(${PROJECT_NAME} PRIVATE
    obs
	  obs-frontend-api
    )
else()
target_link_libraries(${PROJECT_NAME} PRIVATE OBS::libobs OBS::frontend-api)
endif()

if (BUILD_TOOLSETS)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "lovense-obs-toolset")
endif()


target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

if (NOT BUILD_OBS27)
if(ENABLE_BROWSER_PANELS OR ENABLE_BROWSER_QT_LOOP)
  find_qt(COMPONENTS Widgets)

  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)

  target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Widgets)
endif()
endif()

#windows toolset
if (OS_WINDOWS)
  if (BUILD_TOOLSETS)
    set(BRAOWSER_PAGE_NAME lovense-video-page)
    add_executable(${BRAOWSER_PAGE_NAME} WIN32)
    set_target_properties(${BRAOWSER_PAGE_NAME} PROPERTIES OUTPUT_NAME "Lovense-browser-page")
  else()
    set(BRAOWSER_PAGE_NAME obs-browser-page)
    add_executable(${BRAOWSER_PAGE_NAME})
  endif()

  target_sources(
    ${BRAOWSER_PAGE_NAME}
    PRIVATE cef-headers.hpp obs-browser-page/obs-browser-page-main.cpp
            browser-app.cpp browser-app.hpp deps/json11/json11.cpp
            deps/json11/json11.hpp)

  target_link_libraries(${BRAOWSER_PAGE_NAME} PRIVATE CEF::Library)

  target_include_directories(
    ${BRAOWSER_PAGE_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/deps
            ${CMAKE_CURRENT_SOURCE_DIR}/${BRAOWSER_PAGE_NAME})

  target_compile_features(${BRAOWSER_PAGE_NAME} PRIVATE cxx_std_17)

  if(OS_WINDOWS)
    if (BUILD_TOOLSETS)
      target_sources(${BRAOWSER_PAGE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}\\manifest\\x${BITS_LABEL}\\Lovense-browser-page.exe1.manifest)
    else()
      target_sources(${BRAOWSER_PAGE_NAME} PRIVATE obs-browser-page.manifest)
    endif()

    target_compile_options(${BRAOWSER_PAGE_NAME}
                           PRIVATE $<IF:$<CONFIG:DEBUG>,/MDd,/MD>)

    if(TARGET CEF::Wrapper_Debug)
      target_link_libraries(${BRAOWSER_PAGE_NAME} PRIVATE optimized CEF::Wrapper)
      target_link_libraries(${BRAOWSER_PAGE_NAME} PRIVATE debug CEF::Wrapper_Debug)
    else()
      target_link_libraries(${BRAOWSER_PAGE_NAME} PRIVATE CEF::Wrapper)
    endif()
  else()
    target_link_libraries(${BRAOWSER_PAGE_NAME} PRIVATE CEF::Wrapper)
  endif()

  if(ENABLE_BROWSER_LEGACY)
    target_compile_definitions(${BRAOWSER_PAGE_NAME} PRIVATE ENABLE_BROWSER_LEGACY)
  endif()

  if(ENABLE_BROWSER_SHARED_TEXTURE)
    target_compile_definitions(${BRAOWSER_PAGE_NAME}
                               PRIVATE ENABLE_BROWSER_SHARED_TEXTURE)
  endif()

  if(ENABLE_BROWSER_QT_LOOP)
    target_compile_definitions(${BRAOWSER_PAGE_NAME} PRIVATE ENABLE_BROWSER_QT_LOOP)
  endif()

  set_target_properties(${BRAOWSER_PAGE_NAME} PROPERTIES FOLDER
                                                    "plugins/lovense-video-freedback")

  setup_plugin_target(${BRAOWSER_PAGE_NAME})
endif()

if(OS_WINDOWS)
if (BUILD_TOOLSETS)
  target_compile_options(${PROJECT_NAME} PRIVATE $<IF:$<CONFIG:DEBUG>,/MDd,/MD>)
  target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}\\manifest\\x${BITS_LABEL}\\lovese_plugin_x${BITS_LABEL}.dll1.manifest)
else()
  #target_compile_options(${PROJECT_NAME} PRIVATE $<IF:$<CONFIG:DEBUG>,/MTd,/MT>)
  target_compile_options(${PROJECT_NAME} PRIVATE $<IF:$<CONFIG:DEBUG>,/MDd,/MD>)
endif()

  target_link_libraries(${PROJECT_NAME} PRIVATE CEF::Library d3d11 dxgi)

  if(TARGET CEF::Wrapper_Debug)
    target_link_libraries(${PROJECT_NAME} PRIVATE optimized CEF::Wrapper)
    target_link_libraries(${PROJECT_NAME} PRIVATE debug CEF::Wrapper_Debug)
  else()
    target_link_libraries(${PROJECT_NAME} PRIVATE CEF::Wrapper)
  endif()

  target_link_options(${PROJECT_NAME} PRIVATE "LINKER:/IGNORE:4099")

if (NOT BUILD_TOOLSETS)
  target_link_options(${BRAOWSER_PAGE_NAME} PRIVATE "LINKER:/IGNORE:4099"
                      "LINKER:/SUBSYSTEM:WINDOWS")
endif()

  list(APPEND obs-browser_LIBRARIES d3d11 dxgi)

elseif(OS_MACOS)
  find_library(COREFOUNDATION CoreFoundation)
  find_library(APPKIT AppKit)
  mark_as_advanced(COREFOUNDATION APPKIT)

  target_link_libraries(${PROJECT_NAME} PRIVATE ${COREFOUNDATION} ${APPKIT}
                                            CEF::Wrapper)

  target_sources(${PROJECT_NAME} PRIVATE macutil.mm)

  set(CEF_HELPER_TARGET "obs-browser-helper")
  set(CEF_HELPER_OUTPUT_NAME "OBS Helper")
  set(CEF_HELPER_APP_SUFFIXES
      "::" " (GPU):_gpu:.gpu" " (Plugin):_plugin:.plugin"
      " (Renderer):_renderer:.renderer")

  foreach(_SUFFIXES ${CEF_HELPER_APP_SUFFIXES})
    string(REPLACE ":" ";" _SUFFIXES ${_SUFFIXES})
    list(GET _SUFFIXES 0 _NAME_SUFFIX)
    list(GET _SUFFIXES 1 _TARGET_SUFFIX)
    list(GET _SUFFIXES 2 _PLIST_SUFFIX)

    set(_HELPER_TARGET "${CEF_HELPER_TARGET}${_TARGET_SUFFIX}")
    set(_HELPER_OUTPUT_NAME "${CEF_HELPER_OUTPUT_NAME}${_NAME_SUFFIX}")

    set(_HELPER_INFO_PLIST
        "${CMAKE_CURRENT_BINARY_DIR}/helper-info${_PLIST_SUFFIX}.plist")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/helper-info.plist" _PLIST_CONTENTS)
    string(REPLACE "\${EXECUTABLE_NAME}" "${_HELPER_OUTPUT_NAME}"
                   _PLIST_CONTENTS ${_PLIST_CONTENTS})
    string(REPLACE "\${PRODUCT_NAME}" "${_HELPER_OUTPUT_NAME}" _PLIST_CONTENTS
                   ${_PLIST_CONTENTS})
    string(REPLACE "\${BUNDLE_ID_SUFFIX}" "${_PLIST_SUFFIX}" _PLIST_CONTENTS
                   ${_PLIST_CONTENTS})
    string(REPLACE "\${MINIMUM_VERSION}" "${CMAKE_OSX_DEPLOYMENT_TARGET}"
                   _PLIST_CONTENTS ${_PLIST_CONTENTS})
    string(REPLACE "\${CURRENT_YEAR}" "${CURRENT_YEAR}" _PLIST_CONTENTS
                   ${_PLIST_CONTENTS})
    file(WRITE ${_HELPER_INFO_PLIST} ${_PLIST_CONTENTS})

    set(MACOSX_BUNDLE_GUI_IDENTIFIER
        "${MACOSX_BUNDLE_GUI_IDENTIFIER}.helper${_PLIST_SUFFIX}")

    add_executable(${_HELPER_TARGET} MACOSX_BUNDLE)
    add_executable(OBS::browser-helper${_TARGET_SUFFIX} ALIAS ${_HELPER_TARGET})
    target_sources(
      ${_HELPER_TARGET}
      PRIVATE browser-app.cpp browser-app.hpp
              obs-browser-page/obs-browser-page-main.cpp cef-headers.hpp
              deps/json11/json11.cpp deps/json11/json11.hpp)

    target_link_libraries(${_HELPER_TARGET} PRIVATE CEF::Wrapper)

    target_include_directories(
      ${_HELPER_TARGET}
      PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/deps
              ${CMAKE_CURRENT_SOURCE_DIR}/obs-browser-page)

    target_compile_features(${_HELPER_TARGET} PRIVATE cxx_std_17)

    if(ENABLE_BROWSER_SHARED_TEXTURE)
      target_compile_definitions(${_HELPER_TARGET}
                                 PRIVATE ENABLE_BROWSER_SHARED_TEXTURE)
    endif()

    set_target_properties(
      ${_HELPER_TARGET}
      PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${_HELPER_INFO_PLIST}
        OUTPUT_NAME ${_HELPER_OUTPUT_NAME}
        FOLDER "plugins/obs-browser/helpers/"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
        "com.obsproject.obs-studio.helper${_PLIST_SUFFIX}"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
        "${CMAKE_SOURCE_DIR}/cmake/bundle/macOS/entitlements-helper${_PLIST_SUFFIX}.plist"
    )
  endforeach()

elseif(OS_POSIX)
  find_package(X11 REQUIRED)

  target_link_libraries(${PROJECT_NAME} PRIVATE CEF::Wrapper CEF::Library X11::X11)

  get_target_property(_CEF_DIRECTORY CEF::Library INTERFACE_LINK_DIRECTORIES)

  set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_RPATH "$ORIGIN/")

#  set_target_properties(obs-browser-page PROPERTIES BUILD_RPATH "$ORIGIN/")
  set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/")
#  set_target_properties(obs-browser-page PROPERTIES INSTALL_RPATH "$ORIGIN/")
endif()

if (NOT BUILD_TOOLSETS)
if(ENABLE_BROWSER_PANELS)
  add_library(obs-browser-panels INTERFACE)
  add_library(OBS::browser-panels ALIAS obs-browser-panels)
  target_sources(
    obs-browser-panels
    INTERFACE panel/browser-panel.cpp panel/browser-panel.hpp
              panel/browser-panel-client.cpp panel/browser-panel-client.hpp
              panel/browser-panel-internal.hpp)

  target_include_directories(
    obs-browser-panels INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
                                 ${CMAKE_CURRENT_SOURCE_DIR}/panel)

  target_link_libraries(obs-browser-panels INTERFACE CEF::Wrapper)

  if(OS_MACOS)
    target_link_libraries(obs-browser-panels INTERFACE objc)
  endif()

  target_link_libraries(${PROJECT_NAME} PRIVATE obs-browser-panels)

  target_compile_definitions(obs-browser-panels INTERFACE BROWSER_AVAILABLE)

  if(ENABLE_BROWSER_QT_LOOP)
    target_compile_definitions(obs-browser-panels
                               INTERFACE ENABLE_BROWSER_QT_LOOP)
  endif()
  set_target_properties(obs-browser-panels PROPERTIES FOLDER "plugins/${PROJECT_NAME}")
endif()
endif()

target_link_libraries(
    ${PROJECT_NAME}
  PRIVATE
    Lovense::lib_lovense_core
    Lovense::lib_lovense_obs        
)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "plugins/${PROJECT_NAME}" PREFIX
                                                                          "")

##############################################################################################################
##拷贝
if(OS_WINDOWS AND BUILD_TOOLSETS)
IF(CMAKE_CL_64)
    set(BITS 64)
ELSE(CMAKE_CL_64)
    set(BITS 32)
ENDIF(CMAKE_CL_64)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD		
  COMMAND ${CMAKE_COMMAND} -E copy		
  "${CMAKE_BINARY_DIR}/plugins/lovense-video-freeback/$<CONFIGURATION>/lovense-obs-toolset.dll"
        "C:\\ProgramData\\obs-studio\\plugins\\lovense-obs-toolset\\bin\\${BITS}bit\\lovense-obs-toolset.dll"
 )

  
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD		
  COMMAND ${CMAKE_COMMAND} -E copy		
	"${CMAKE_BINARY_DIR}/plugins/lovense-video-freeback/$<CONFIGURATION>/lovense-obs-toolset.pdb"
      "C:\\ProgramData\\obs-studio\\plugins\\lovense-obs-toolset\\bin\\${BITS}bit\\lovense-obs-toolset.pdb"
  )

endif()
##############################################################################################################

setup_plugin_target(${PROJECT_NAME})


