cmake_minimum_required(VERSION 3.5)

####################################################
# add code source 
set(MT_RTMP_SOURCE
    obs-multi-rtmp.cpp
    edit-widget.cpp
    push-widget.cpp
)

set(MT_RTMP_HEADS
    obs-multi-rtmp.h
    edit-widget.h
    push-widget.h
    pch.h
)
####################################################


#################################################################################################
#build obs > 27 plugins
if(NOT BUILD_OBS27)
project(obs-multi-rtmp-qt6)

find_qt(COMPONENTS Widgets)

add_library(obs-multi-rtmp-qt6 MODULE)
add_library(OBS::multi-rtmp-qt6 ALIAS obs-multi-rtmp-qt6)
#target_link_libraries(obs-multi-rtmp PRIVATE Qt::Widgets Qt::Svg Qt::Xml Qt::Network)

if(WIN32 AND MSVC)
    add_compile_options(/WX-)
endif()

set_target_properties(
  obs-multi-rtmp-qt6	
  PROPERTIES FOLDER "plugins" 
             AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

target_sources(obs-multi-rtmp-qt6 PRIVATE 
${MT_RTMP_SOURCE}
${MT_RTMP_HEADS}
)

target_link_libraries(obs-multi-rtmp-qt6 PRIVATE
	OBS::libobs
    OBS::frontend-api
	Qt::Core
	Qt::Widgets)
setup_plugin_target(obs-multi-rtmp-qt6)

else() 
#################################################################################################
# build obs 27 plugin 
project(obs-multi-rtmp VERSION 0.2.8.0)

if(WIN32 AND MSVC)
  add_compile_options(/wd4996)
endif()

set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt5Widgets ${FIND_MODE})
find_package(Qt5Svg ${FIND_MODE})
find_package(Qt5Xml ${FIND_MODE})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")

find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

add_library(obs-multi-rtmp MODULE
${MT_RTMP_SOURCE}
${MT_RTMP_HEADS}
)

set_target_properties(
  obs-multi-rtmp
  PROPERTIES FOLDER "plugins")


target_link_libraries(obs-multi-rtmp
	obs
	obs-frontend-api
	Qt5::Core
	Qt5::Widgets)

if(BUILD_TOOLSETS)
setup_plugin_target(obs-multi-rtmp)
else()
install_obs_plugin_with_data(obs-multi-rtmp data)
endif()

endif()
