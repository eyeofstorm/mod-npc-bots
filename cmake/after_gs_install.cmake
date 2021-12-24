#
# Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
# Released under GNU AGPL v3 
# License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
#

set(PUBLIC_INCLUDES
    ${PUBLIC_INCLUDES}
    ${CMAKE_MOD_NPCBOTS_DIR}/src
)

target_include_directories(game-interface
  INTERFACE
    ${PUBLIC_INCLUDES})

add_dependencies(game npcbots)

target_link_libraries(game
  PUBLIC
    npcbots)