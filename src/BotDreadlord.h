/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_DREADLORD_H_
#define _BOT_DREADLORD_H_

#include "BotAI.h"
#include "BotCommon.h"
#include "Creature.h"
#include "Object.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Spell.h"

enum DreadlordGossip
{
    GOSSIP_DREADLORD_SENDER_EXIT_MENU = 0,
    GOSSIP_DREADLORD_SENDER_MAIN_MENU = 1,
    GOSSIP_DREADLORD_SENDER_DISMISS = 2, 
    GOSSIP_DREADLORD_GREET = 9000000
};

enum DreadlordBaseSpells
{
    CARRION_SWARM_1         = SPELL_CARRION_SWARM,
    SLEEP_1                 = SPELL_SLEEP,
    INFERNO_1               = SPELL_INFERNO
};

enum DreadlordPassives
{
    VAMPIRIC_AURA           = SPELL_VAMPIRIC_AURA,
};

enum DreadlordSpecial
{
    MH_ATTACK_ANIM          = SPELL_ATTACK_MELEE_1H,

    CARRION_COST            = 110 * 5,
    CARRION_CD              = 10000,
    SLEEP_COST              = 50 * 5,
    SLEEP_CD                = 6000,
    INFERNAL_COST           = 175 * 5,
    INFERNAL_CD             = 180000,
    INFERNAL_DURATION       = (INFERNAL_CD - 2300),

    DAMAGE_CD_REDUCTION     = 250,  //ms
    INFERNO_SPAWN_DELAY     = 650,  //ms

    IMMOLATION              = 39007
};

static const uint32 dreadlord_spells_damage_arr[] =     { CARRION_SWARM_1, INFERNO_1 };
static const uint32 dreadlord_spells_cc_arr[] =         { SLEEP_1 };
static const uint32 dreadlord_spells_support_arr[] =    { INFERNO_1 };

static const std::vector<uint32> dreadlord_spells_damage(FROM_ARRAY(dreadlord_spells_damage_arr));
static const std::vector<uint32> dreadlord_spells_cc(FROM_ARRAY(dreadlord_spells_cc_arr));
static const std::vector<uint32> dreadlord_spells_support(FROM_ARRAY(dreadlord_spells_support_arr));

class BotDreadlordAI : public BotAI
{
private:
    class DelayedSummonInfernoEvent : public BasicEvent
    {
    public:
        DelayedSummonInfernoEvent(Creature* bot, Position pos) : m_bot(bot), m_pos(pos) { }

    protected:
        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/)
        {
            if (m_bot)
            {
                BotDreadlordAI* ai = (BotDreadlordAI*)m_bot->AI();

                if (ai)
                {
                    ai->SummonBotPet(&m_pos);
                }
            }

            return true;
        }

    private:
        Creature* m_bot;
        Position m_pos;
    };

    class DelayedUnsummonInfernoEvent : public BasicEvent
    {
    public:
        DelayedUnsummonInfernoEvent(Creature const* bot) : m_bot(bot) { }

    protected:
        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/)
        {
            if (m_bot)
            {
                BotDreadlordAI* ai = (BotDreadlordAI*)m_bot->AI();

                if (ai)
                {
                    ai->UnSummonBotPet();
                }
            }

            return true;
        }

    private:
        Creature const* m_bot;
    };

public:
    BotDreadlordAI(Creature* creature);

public:
    virtual ~BotDreadlordAI();

public:
    void SummonBotPet(Position const* pos) override;
    void UnSummonBotPet() override;
    void OnClassSpellGo(SpellInfo const* spellInfo) override;
    void SummonedCreatureDespawn(Creature* summon) override;

protected:
    void UpdateBotAI(uint32 uiDiff) override;
    void UpdateSpellCD(uint32 uiDiff) override;
    void CheckAura(uint32 uiDiff);
    void RefreshAura(uint32 spellId, int8 count = 1, Unit* target = nullptr) const;

    void InitCustomeSpells() override;

private:
    bool DoSummonInfernoIfReady(uint32 uiDiff);
    bool DoCastDreadlordSpellSleep(uint32 uiDiff);
    void DoDreadlordAttackIfReady(uint32 uiDiff);

private:
    uint32 m_checkAuraTimer;
    Position m_infernoSpwanPos;
};

class BotDreadlord : public CreatureScript
{
    friend class BotDreadlordAI;

public:
    BotDreadlord() : CreatureScript("bot_dreadlord") { }

public:
    // Called when a player opens a gossip dialog with the creature.
    bool OnGossipHello(Player* /*player*/, Creature* /*creature*/) override;

    // Called when a player selects a gossip item in the creature's gossip menu.
    bool OnGossipSelect(Player* /*player*/, Creature* /*creature*/, uint32 /*sender*/, uint32 /*action*/) override;

public:
    CreatureAI* GetAI(Creature* creature) const override
    {
        return new BotDreadlordAI(creature);
    }
};

#endif // _BOT_DREADLORD_H_
