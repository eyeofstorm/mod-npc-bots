/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_GRIDNOTIFIERS_H
#define _BOT_GRIDNOTIFIERS_H

#include "BotMgr.h"
#include "CreatureAI.h"
#include "DynamicObject.h"
#include "Player.h"
#include "Spell.h"
#include "Unit.h"
#include <iostream>

class Player;

namespace Acore
{
    class CastingUnitCheck
    {
    public:
        explicit CastingUnitCheck(Unit* unit, float mindist = 0.f, float maxdist = 30, uint32 spell = 0, uint8 minHpPct = 0) : me(unit), min_range(mindist), max_range(maxdist), m_spell(spell), m_minHpPct(minHpPct)
        {
        }

        bool operator()(Unit* u) const
        {
            if (!u->IsAlive())
                return false;

            if (!u->InSamePhase(me))
                return false;

            if (!u->IsVisible())
                return false;

            if (!u->GetTarget() && !u->IsInCombat())
                return false;

            if (u->IsTotem())
                return false;

            Creature *cre = me->ToCreature();

            if (!(cre && BotMgr::GetBotAI(cre) && BotMgr::GetBotAI(cre)->IAmFree()) && u->IsControlledByPlayer())
                return false;

            if (u->HealthBelowPct(m_minHpPct))
                return false;

            if (min_range > 0.1f && me->GetDistance(u) < min_range)
                return false;

            if (!u->isTargetableForAttack(false))
                return false;

            if (!u->IsNonMeleeSpellCast(false, false, true))
                return false;

            if (!me->IsWithinDistInMap(u, max_range))
                return false;

            if (u->GetReactionTo(me) >= REP_FRIENDLY)
                return false;

            if (m_spell)
            {
                if ((m_spell == 5782 || //fear (warlock)
                    m_spell == 64044 || //fear (priest)
                    m_spell == SPELL_SLEEP) &&
                    u->GetCreatureType() == CREATURE_TYPE_UNDEAD)
                    return false;

                if (m_spell == 10326 && //turn evil
                    !(u->GetCreatureType() == CREATURE_TYPE_UNDEAD ||
                    u->GetCreatureType() == CREATURE_TYPE_DEMON))
                    return false;

                if (m_spell == 20066 && //repentance
                    !(u->GetCreatureType() == CREATURE_TYPE_HUMANOID ||
                    u->GetCreatureType() == CREATURE_TYPE_DEMON ||
                    u->GetCreatureType() == CREATURE_TYPE_DRAGONKIN ||
                    u->GetCreatureType() == CREATURE_TYPE_GIANT ||
                    u->GetCreatureType() == CREATURE_TYPE_UNDEAD))
                    return false;

                if (m_spell == 2637 && //hibernate
                    !(u->GetCreatureType() == CREATURE_TYPE_BEAST ||
                    u->GetCreatureType() == CREATURE_TYPE_DRAGONKIN))
                    return false;

                if (m_spell == 9484 && //shackle undead (priest)
                    u->GetCreatureType() != CREATURE_TYPE_UNDEAD)
                    return false;

                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(m_spell);

                if (u->IsImmunedToSpell(spellInfo))
                    return false;

                if (!CastInterruptionCheck(u, spellInfo))
                    return false;
            }

            return true;
        }

        static bool CastInterruptionCheck(Unit const* u, SpellInfo const* spellInfo)
        {
            if (spellInfo->HasEffect(SPELL_EFFECT_INTERRUPT_CAST) && spellInfo->GetFirstRankSpell()->Id != 853) //hammer of justice
            {
                if (u->GetTypeId() == TYPEID_UNIT &&
                    (u->ToCreature()->GetCreatureTemplate()->MechanicImmuneMask & (1 << (MECHANIC_INTERRUPT - 1))))
                {
                    return false;
                }

                Spell* curSpell;

                for (uint8 i = CURRENT_FIRST_NON_MELEE_SPELL; i != CURRENT_AUTOREPEAT_SPELL; ++i)
                {
                    curSpell = u->GetCurrentSpell(i);

                    if (!curSpell)
                        continue;

                    // copied conditions from Spell::EffectInterruptCast
                    if (!((curSpell->getState() == SPELL_STATE_CASTING ||
                        (curSpell->getState() == SPELL_STATE_PREPARING && curSpell->GetCastTime() > 0.0f)) &&
                        curSpell->GetSpellInfo()->PreventionType == SPELL_PREVENTION_TYPE_SILENCE &&
                        ((i == CURRENT_GENERIC_SPELL && curSpell->GetSpellInfo()->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT) ||
                        (i == CURRENT_CHANNELED_SPELL && curSpell->GetSpellInfo()->ChannelInterruptFlags & CHANNEL_INTERRUPT_FLAG_INTERRUPT))))
                    {
                        return false;
                    }
                }
            }

            bool silenceSpell = false;

            for (uint8 i = 0; i != MAX_SPELL_EFFECTS; ++i)
            {
                if (spellInfo->Effects[i].Effect == SPELL_EFFECT_APPLY_AURA &&
                    spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_SILENCE)
                {
                    silenceSpell = true;
                    break;
                }
            }

            if (silenceSpell)
            {
                if (u->GetTypeId() == TYPEID_UNIT &&
                    (u->ToCreature()->GetCreatureTemplate()->MechanicImmuneMask & (1 << (MECHANIC_SILENCE - 1))))
                    return false;

                Spell* curSpell;

                for (uint8 i = CURRENT_FIRST_NON_MELEE_SPELL; i != CURRENT_AUTOREPEAT_SPELL; ++i)
                {
                    curSpell = u->GetCurrentSpell(i);

                    if (curSpell && curSpell->GetSpellInfo()->PreventionType != SPELL_PREVENTION_TYPE_SILENCE)
                        return false;
                }
            }

            return true; //do not check players and non-interrupt non-silence spells
        }

    private:
        Unit* me;
        float min_range, max_range;
        uint32 m_spell;
        uint8 m_minHpPct;
        CastingUnitCheck(CastingUnitCheck const&);
    };

    class StunUnitCheck
    {
    public:
        explicit StunUnitCheck(Unit* unit, float dist = 20) : me(unit), m_range(dist)
        {
        }

        bool operator()(Unit* u) const
        {
            Creature* cre = me->ToCreature();

            if (!(cre && BotMgr::GetBotAI(cre)->IAmFree()) && u->IsControlledByPlayer())
                return false;

            if (!u->IsAlive())
                return false;

            if (!u->IsInCombat())
                return false;

            if (!u->InSamePhase(me))
                return false;

            if (u->HasUnitState(UNIT_STATE_CONFUSED | UNIT_STATE_STUNNED | UNIT_STATE_FLEEING | UNIT_STATE_DISTRACTED | UNIT_STATE_CONFUSED_MOVE | UNIT_STATE_FLEEING_MOVE))
                return false;

            if (!me->IsWithinDistInMap(u, m_range))
                return false;

            if (!u->IsVisible())
                return false;

            if (!u->isTargetableForAttack())
                return false;

            if (!u->getAttackers().empty())
                return false;

            if (u->GetReactionTo(me) > REP_NEUTRAL)
                return false;

            return false;
        }

    private:
        Unit* me;
        float m_range;
        StunUnitCheck(StunUnitCheck const&);
    };

    class NearbyHostileUnitInConeCheck
    {
        public:
            explicit NearbyHostileUnitInConeCheck(Creature* unit, float maxdist, BotAI const* botAI) : me(unit), max_range(maxdist), ai(botAI), cone(float(M_PI) / 2)
            {
                free = ai->IAmFree();
            }

            bool operator()(Unit* u) const
            {
                if (u == me)
                    return false;

                if (!u->IsInCombat())
                    return false;

                if (!u->InSamePhase(me))
                    return false;

                if (u->HasUnitState(UNIT_STATE_CONFUSED | UNIT_STATE_STUNNED | UNIT_STATE_FLEEING | UNIT_STATE_DISTRACTED | UNIT_STATE_CONFUSED_MOVE))
                    return false;

                if (!ai->IAmFree() && u->IsControlledByPlayer())
                    return false;

                if (!me->IsWithinDistInMap(u, max_range))
                    return false;

                if (!me->HasInArc(cone, u))
                    return false;

                if (free)
                {
                    if (u->IsControlledByPlayer())
                        return false;

                    if (!me->IsValidAttackTarget(u) || !u->isTargetableForAttack(false))
                        return false;
                }

                return true;
            }

        private:
            Unit* me;
            float max_range;
            BotAI const* ai;
            float cone;
            bool free;
            NearbyHostileUnitInConeCheck(NearbyHostileUnitInConeCheck const&);
    };
};

#endif  //_BOT_GRIDNOTIFIERS_H
