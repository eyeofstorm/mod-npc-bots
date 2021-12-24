#
# Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
# Released under GNU AGPL v3 
# License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
#

add_subdirectory(${CMAKE_MOD_NPCBOTS_DIR}/src)

CU_ADD_HOOK(AFTER_GAME_LIBRARY "${CMAKE_MOD_NPCBOTS_DIR}/cmake/after_gs_install.cmake")

AC_ADD_SCRIPT_LOADER("Npcbots" "${CMAKE_MOD_NPCBOTS_DIR}/src/ACoreHookScriptLoader.h")

AC_ADD_CONFIG_FILE("${CMAKE_MOD_NPCBOTS_DIR}/conf/npcbots.conf.dist")
