# Copyright: (C) 2011 RobotCub Consortium
# Authors: Andrea Del Prete & Alessandro Roncone
# CopyPolicy: Released under the terms of the GNU GPL v2.0.

project(skinDynLib)

set (SKIN_NAO OFF CACHE BOOL "Build the library for using the skin of Nao robot")
if (SKIN_NAO)
  add_definitions(-DSKIN_NAO)
endif (SKIN_NAO)

set(folder_source src/skinContact.cpp
                  src/skinContactList.cpp
                  src/dynContact.cpp
                  src/dynContactList.cpp
                  src/common.cpp 
                  src/Taxel.cpp
                  src/skinPart.cpp
                  src/iCubSkin.cpp)
set(folder_header include/iCub/skinDynLib/skinContact.h
                  include/iCub/skinDynLib/skinContactList.h
                  include/iCub/skinDynLib/dynContact.h
                  include/iCub/skinDynLib/dynContactList.h
                  include/iCub/skinDynLib/common.h
                  include/iCub/skinDynLib/rpcSkinManager.h 
                  include/iCub/skinDynLib/Taxel.h
                  include/iCub/skinDynLib/skinPart.h
                  include/iCub/skinDynLib/iCubSkin.h)

add_library(${PROJECT_NAME} ${folder_source} ${folder_header})

target_include_directories(${PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
                                                  "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>")

target_link_libraries(${PROJECT_NAME} ${YARP_LIBRARIES} ctrlLib)

icub_export_library(${PROJECT_NAME} INTERNAL_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include
                                    DEPENDS ctrlLib
                                    DESTINATION include/iCub/skinDynLib
                                    FILES ${folder_header})


