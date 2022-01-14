/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _NPC_BOT_SCRIPTS_LOADER_H_
#define _NPC_BOT_SCRIPTS_LOADER_H_

#include "ACoreHookScript.h"
#include "BotDreadlord.h"
#include "BotGiver.h"
#include "BotInfernal.h"

//***********************
// Add all scripts in one
//***********************

void AddNpcbotsScripts()
{
    //*******************
    // acore hook scripts
    //*******************

    new PlayerHookScript();
    new UnitHookScript();
    new CreatureHookScript();
    new SpellHookScript();
//  new BotCommandsScript();
    new MovementHandlerHookScript();

    //**************
    // bot ai script
    //**************
    new BotGiver();
    new BotDreadlord();
    new BotInfernal();
}

#endif // _NPC_BOT_SCRIPTS_LOADER_H_
