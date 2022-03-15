/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BotInfernal.h"
#include "Creature.h"

BotInfernalAI::BotInfernalAI(Creature* creature) : BotAI(creature)
{
    LOG_INFO("npcbots", "BotInfernalAI::BotInfernalAI (this: 0X{:016x}, name: {})", (unsigned long long)this, creature->GetName().c_str());

    m_bot->SetReactState(REACT_AGGRESSIVE);

    m_botClass = BOT_CLASS_WARRIOR;
    m_classLevelInfo = sObjectMgr->GetCreatureBaseStats(std::min<uint8>(m_bot->getLevel(), 80), GetBotClass());

    LOG_DEBUG(
          "npcbots",
          "bot [%s] sObjectMgr->GetCreatureBaseStats({}, {}) => [ basemana: {} ]",
          m_bot->GetName().c_str(),
          std::min<uint8>(m_bot->getLevel(), 80),
          GetBotClass(),
          m_classLevelInfo->BaseMana);

    LOG_INFO("npcbots", "BotInfernalAI::BotInfernalAI (this: 0X{:016x} name: {})", (unsigned long long)this, creature->GetName().c_str());
}

BotInfernalAI::~BotInfernalAI()
{
    LOG_INFO("npcbots", "BotInfernalAI::~BotInfernalAI (this: 0X{:016x}, name: {})", (unsigned long long)this, m_bot->GetName().c_str());

    LOG_INFO("npcbots", "BotInfernalAI::~BotInfernalAI (this: 0X{:016x}, name: {})", (unsigned long long)this, m_bot->GetName().c_str());
}

void BotInfernalAI::UpdateBotCombatAI(uint32 /*uiDiff*/)
{
    if (!UpdateVictim())
    {
        return;
    }

    if (m_bot->HasReactState(REACT_PASSIVE))
    {
        return;
    }

    DoMeleeAttackIfReady();
}

bool BotInfernalAI::CanEat() const
{
    return false;
}

bool BotInfernalAI::CanDrink() const
{
    return false;
}

bool BotInfernalAI::CanSit() const
{
    return false;
}
