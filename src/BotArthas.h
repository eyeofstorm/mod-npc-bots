/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_ARTHAS_H_
#define _BOT_ARTHAS_H_

#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "BotAI.h"

class BotArthasAI : public BotAI
{
public:
    BotArthasAI(Creature* creature) : BotAI(creature) { }

protected:
    void UpdateBotAI(uint32 uiDiff) override;
};

class BotArthas : public CreatureScript
{
    friend class BotArthasAI;

public:
    BotArthas() : CreatureScript("bot_arthas") { }

public:
    CreatureAI* GetAI(Creature* creature) const
    {
        return new BotArthasAI(creature);
    }
};

#endif // _BOT_ARTHAS_H_
