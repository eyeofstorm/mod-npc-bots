#
# Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
# Released under GNU AGPL v3 
# License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
#

CollectSourceFiles(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE_SOURCES
  # Exclude
  ${CMAKE_CURRENT_SOURCE_DIR}/Debugging
  ${CMAKE_CURRENT_SOURCE_DIR}/PrecompiledHeaders)

add_library(npcbots STATIC
  ${PRIVATE_SOURCES})

CollectIncludeDirectories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC_INCLUDES)

target_include_directories(npcbots
  PUBLIC
    ${PUBLIC_INCLUDES}
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(npcbots
  PRIVATE
    game-interface)
