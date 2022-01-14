/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BotInfernal.h"
#include "Creature.h"

BotInfernalAI::BotInfernalAI(Creature* creature) : BotAI(creature)
{
    LOG_INFO("npcbots", "↓↓↓↓↓↓ BotInfernalAI::BotInfernalAI (this: 0X%016llX, name: %s)", (unsigned long long)this, creature->GetName().c_str());

    LOG_INFO("npcbots", "↑↑↑↑↑↑ BotInfernalAI::BotInfernalAI (this: 0X%016llX, name: %s)", (unsigned long long)this, creature->GetName().c_str());
}

BotInfernalAI::~BotInfernalAI()
{
    LOG_INFO("npcbots", "↓↓↓↓↓↓ BotInfernalAI::~BotInfernalAI (this: 0X%016llX, name: %s)", (unsigned long long)this, m_bot->GetName().c_str());

    LOG_INFO("npcbots", "↑↑↑↑↑↑ BotInfernalAI::~BotInfernalAI (this: 0X%016llX, name: %s)", (unsigned long long)this, m_bot->GetName().c_str());
}

void BotInfernalAI::UpdateBotAI(uint32 /*uiDiff*/)
{
    if (!UpdateVictim())
    {
        return;
    }

    DoMeleeAttackIfReady();
}
