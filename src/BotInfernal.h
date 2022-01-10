/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_INFERNAL_H_
#define _BOT_INFERNAL_H_

#include "BotAI.h"
#include "BotCommon.h"
#include "Creature.h"
#include "Object.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Spell.h"

class BotInfernalAI : public BotAI
{
public:
    BotInfernalAI(Creature* creature);

protected:
    void UpdateBotAI(uint32 uiDiff) override;
};

class BotInfernal : public CreatureScript
{
    friend class BotInfernalAI;

public:
    BotInfernal() : CreatureScript("bot_infernal") { }

public:
    CreatureAI* GetAI(Creature* creature) const
    {
        return new BotInfernalAI(creature);
    }
};

#endif // _BOT_INFERNAL_H_
