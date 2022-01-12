/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BotInfernal.h"
#include "Creature.h"

BotInfernalAI::BotInfernalAI(Creature* creature) : BotAI(creature)
{
}

void BotInfernalAI::UpdateBotAI(uint32 /*uiDiff*/)
{
    if (!UpdateVictim())
    {
        return;
    }

    DoMeleeAttackIfReady();
}
