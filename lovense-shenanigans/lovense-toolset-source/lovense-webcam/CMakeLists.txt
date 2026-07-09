cmake_minimum_required(VERSION 3.5)

if(BUILD_OBS27)
set(PROJECT_NAME lovense-webcam-qt5)
else()
set(PROJECT_NAME lovense-webcam)
endif()


project(${PROJECT_NAME} VERSION 0.2.8.0)
####################################################
#common handle
set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add code source 
set(MT_RTMP_SOURCE
obs-multi-rtmp.cpp
UntilHelp.cpp
form/MainControlDockWidget.cpp
form/PanControlWidget.cpp
form/CLink360Adapter.cpp
form/SwitchButton.cpp
form/PositionWidget.cpp
form/RemoteControlDialog.cpp
form/WSProtocol.cpp
form/NewPositionDialog.cpp
QrCode.cpp
)

set(MT_RTMP_HEADS
obs-multi-rtmp.h
PositionTokensData.h
pch.h
UntilHelp.h
form/MainControlDockWidget.h
form/PanControlWidget.h
form/CLink360Adapter.h
form/SwitchButton.h
form/PositionWidget.h
form/RemoteControlDialog.h
form/WSProtocol.h
form/NewPositionDialog.h
QrCode.hpp
)

set(FORM_UI_FILES
form/RemoteControlDialog.ui
form/NewPositionDialog.ui
)

if (BUILD_OBS27)
list(APPEND 
    FORM_UI_FILES 
    form/x27/MainControlDockWidget.ui
)
else()
list(APPEND FORM_UI_FILES
    form/MainControlDockWidget.ui
)
endif()

if(APPLE)
list(APPEND MT_RTMP_HEADS
macResouce.h
)
list(APPEND MT_RTMP_SOURCE
macResouce.mm
)
endif()

## depends include link
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/lovense")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/deps")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")
include_directories(SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/SDK/includes")
include_directories(SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/form")
if(${BUILD_ARM64})
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/SDK/lib/mac/arm64/")
else()
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/SDK/lib/mac/x64/")
endif()
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/SDK/lib/x64/")

#lovense-core
include_directories(${CMAKE_SOURCE_DIR}/liblovense-core)
include_directories(${CMAKE_SOURCE_DIR}/common)
set(LOVENSE_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../liblovense-core)
link_directories("${LOVENSE_LIBRARY_DIR}")

#lovense obs lib
include_directories(${CMAKE_SOURCE_DIR}/liblovense-obs)
set(LOVENSE_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../liblovense-obs)
link_directories("${LOVENSE_LIBRARY_DIR}")

# lovense websocket lib
include_directories(${CMAKE_SOURCE_DIR}/liblovense-websocket)
set(LOVENSE_WEBSOCKET_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../liblovense-websocket)
link_directories("${LOVENSE_WEBSOCKET_LIBRARY_DIR}")
#end

add_library(${PROJECT_NAME} MODULE
${MT_RTMP_SOURCE}
${MT_RTMP_HEADS}
${FORM_UI_FILES}
)


set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "plugins/lovense-webcam" PREFIX
                                                                          "")
                                                                          
target_link_libraries(${PROJECT_NAME} PRIVATE
    UVCCamera
    Qt::Core
    Qt::Network
    Qt::Widgets
    Lovense::lib_lovense_core
    Lovense::lib_lovense_obs
    Lovense::lib_lovense_websocket
    )


####################################################

#################################################################################################
#build obs > 27 plugins obs 28,29.....
if(NOT BUILD_OBS27)

if(NOT MSVC AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
set(BUILD_ARM64 ON)
else()
set(BUILD_ARM64 OFF)
endif()

if(WIN32)
    add_compile_options(/wd4267)
	add_compile_options(/wd4244)
	add_compile_options(/wd4312)
	add_compile_options(/wd4819)	
	add_compile_options(/wd4828)
endif()

find_qt(COMPONENTS Widgets Network)

target_link_libraries(${PROJECT_NAME} PRIVATE
    OBS::libobs
    OBS::frontend-api
    )

setup_plugin_target(${PROJECT_NAME})

else()
#################################################################################################
# toolset obs 27 mode
find_package(Qt5Widgets ${FIND_MODE})
find_package(Qt5Svg ${FIND_MODE})
find_package(Qt5Xml ${FIND_MODE})
find_package(Qt5Network ${FIND_MODE})

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)

qt5_wrap_ui(FORM_UI_HEADERS ${FORM_UI_FILES})

#set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "lovense-webcam-qt5")

if(BUILD_TOOLSETS)
target_link_libraries(${PROJECT_NAME} PRIVATE
    obs
    obs-frontend-api
    )
setup_plugin_target(${PROJECT_NAME})
else()
target_link_libraries(${PROJECT_NAME} PRIVATE
    libobs
    obs-frontend-api
    )
install_obs_plugin_with_data(${PROJECT_NAME} data)
endif()

endif()
