/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include NPCBots.h

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
    new MovementHandlerHookScript();

    //**************
    // bot ai script
    //**************
    new BotGiver();
    new BotDreadlord();
    new BotInfernal();
}
