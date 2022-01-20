/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_GIVER_H_
#define _BOT_GIVER_H_

#include "BotAI.h"
#include "BotCommon.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"

enum BotGiverGossip
{
    GOSSIP_BOT_GIVER_SENDER_EXIT_MENU       = 0,
    GOSSIP_BOT_GIVER_SENDER_MAIN_MENU       = 1,
    GOSSIP_BOT_GIVER_SENDER_HIRE            = 2,
    GOSSIP_BOT_GIVER_SENDER_HIRE_CLASS      = 3,

    GOSSIP_BOT_GIVER_GREET                  = 9000000,
    GOSSIP_BOT_GIVER_HIRE                   = 9000001,
    GOSSIP_BOT_GIVER_HIRE_CLASS             = 9000002
};

enum BotClasses : uint8
{
    BOT_CLASS_NONE                      = 0,
    BOT_CLASS_DREADLORD                 = 1,
    BOT_CLASS_DEATH_KNIGHT              = 2,
    BOT_CLASS_LICH                      = 3,

    BOT_CLASS_END
};

class BotGiverAI : public BotAI
{
public:
    BotGiverAI(Creature* creature);

protected:
    void UpdateBotCombatAI(uint32 uiDiff) override;
};

class BotGiver : public CreatureScript
{
    friend class BotGiverAI;

public:
    BotGiver() : CreatureScript("bot_giver") { }

public:
    // Called when a player opens a gossip dialog with the creature.
    bool OnGossipHello(Player* /*player*/, Creature* /*creature*/) override;

    // Called when a player selects a gossip item in the creature's gossip menu.
    bool OnGossipSelect(Player* /*player*/, Creature* /*creature*/, uint32 /*sender*/, uint32 /*action*/) override;

public:
    CreatureAI* GetAI(Creature* creature) const override
    {
        return new BotGiverAI(creature);
    }
};

#endif // _BOT_GIVER_H_
