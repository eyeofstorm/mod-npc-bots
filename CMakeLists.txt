#
# Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
# Released under GNU AGPL v3 
# License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
#

CU_SET_PATH("CMAKE_MOD_NPCBOTS_DIR" "${CMAKE_CURRENT_LIST_DIR}")
CU_ADD_HOOK(AFTER_LOAD_CMAKE_MODULES "${CMAKE_CURRENT_LIST_DIR}/cmake/after_load_cmake_modules.cmake")
