/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BotDreadlord.h"
#include "Creature.h"

BotDreadlordAI::BotDreadlordAI(Creature* creature) : BotAI(creature)
{
    m_checkAuraTimer = 0;

    // dreadlord immunities
    me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_POSSESS, true);
    me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_CHARM, true);
    me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_SHAPESHIFT, true);
    me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_TRANSFORM, true);
    me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_DISARM, true);
    me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_DISARM_OFFHAND, true);
    me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_FEAR, true);
    me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DISARM, true);
    me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_POLYMORPH, true);
    me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FEAR, true);
    me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_HORROR, true);
    me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_TURN, true);
    me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SLEEP, true);
    me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SILENCE, true);
    me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SNARE, true);

    InitCustomeSpells();
}

void BotDreadlordAI::InitCustomeSpells()
{
    InitSpellMap(CARRION_SWARM_1, true, false);
    InitSpellMap(SLEEP_1, true, false);
    InitSpellMap(INFERNO_1, true, false);

    uint32 spellId;
    SpellInfo* sinfo;

    // INFERNO VISUAL (dummy summon)
    spellId = SPELL_INFERNO_METEOR_VISUAL; //5739
    sinfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(spellId));

    sinfo->SpellFamilyName = SPELLFAMILY_WARLOCK;
    sinfo->SpellLevel = 60;
    sinfo->BaseLevel = 60;
    sinfo->RangeEntry = sSpellRangeStore.LookupEntry(4); //30 yds
    sinfo->CastTimeEntry = sSpellCastTimesStore.LookupEntry(3); //500ms
    sinfo->RecoveryTime = 180000;
    sinfo->StartRecoveryTime = 1500;
    sinfo->StartRecoveryCategory = 133;
    sinfo->ManaCost = 175 * 5;
    sinfo->ExplicitTargetMask = TARGET_FLAG_DEST_LOCATION;
    sinfo->Attributes &= ~(SPELL_ATTR0_IS_ABILITY);
    sinfo->AttributesEx |= SPELL_ATTR1_NO_THREAT;
    sinfo->Effects[0].Effect = SPELL_EFFECT_DUMMY;
    sinfo->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_DEST_DEST);
    sinfo->Effects[0].BasePoints = 1;
}

void BotDreadlordAI::UpdateBotAI(uint32 uiDiff)
{
    if (!UpdateVictim())
    {
        return;
    }

    CheckAura(uiDiff);

    if (IsCasting())
    {
        return;
    }

    uint16 r = Rand();

    if (r < 60)
    {
        //LOG_DEBUG("npcbots", "Have an chance to cast some spells...");

        if (DoSummonInfernoIfReady(uiDiff))
        {
            return;
        }
    }

    // TODO: npcbots => dreadlord ai.

    DoMeleeAttackIfReady();
}

bool BotDreadlordAI::DoSummonInfernoIfReady(uint32 uiDiff)
{
    bool isSpellReady = IsSpellReady(SPELL_INFERNO_METEOR_VISUAL, uiDiff);
    bool isInCombat = me->IsInCombat();
    bool hasMana = me->GetPower(POWER_MANA) >= INFERNAL_COST;

    if (isSpellReady && !m_hasBotPet && isInCombat && hasMana)
    {
        LOG_DEBUG("npcbots", "Ready to cast spell Inferno...");

        float dist = CalcSpellMaxRange(SPELL_INFERNO);
        //LOG_DEBUG("npcbots", "Max range of Spell inferno is %.2f...", dist);

        Unit* target = FindAOETarget(dist);

        if (target)
        {
            LOG_DEBUG("npcbots", "AOE target found...");

            m_infernoPos = target->GetPosition();
        }
        else if (me->GetVictim())
        {
            LOG_DEBUG("npcbots", "Victim found...");

            m_infernoPos = me->GetVictim()->GetPosition();
        }
        else
        {
            LOG_DEBUG("npcbots", "Near point found...");

            me->GetNearPoint(
                    me,
                    m_infernoPos.m_positionX,
                    m_infernoPos.m_positionY,
                    m_infernoPos.m_positionZ,
                    me->GetObjectSize(),
                    5.0f,
                    0.0f);
        }

        uint32 spellIdInferno = GetSpell(INFERNO_1);

        if (spellIdInferno)
        {
            SpellCastResult result = me->CastSpell(
                                            m_infernoPos.m_positionX,
                                            m_infernoPos.m_positionY,
                                            m_infernoPos.m_positionZ,
                                            spellIdInferno,
                                            false);

//            LOG_DEBUG(
//                "npcbots",
//                "Cast dummy spell(INFERNO_1 id: %u) result=%d",
//                spellIdInferno, result);

            return true;
        }
    }
    else
    {
//        LOG_DEBUG(
//            "npcbots",
//            "Can't cast spell Inferno now. isSpellReady=%s, hasInferno=%s, isInCombat=%s, hasMana=%s",
//            isSpellReady == true ? "true" : "false",
//            m_hasBotPet == true ? "true" : "false",
//            isInCombat == true ? "true" : "false",
//            hasMana == true ? "true" : "false");
    }

    return false;
}

void BotDreadlordAI::OnClassSpellGo(SpellInfo const* spellInfo)
{
    uint32 baseId = spellInfo->GetFirstRankSpell()->Id;

    if (baseId == INFERNO_1)
    {
        SpellCastResult result = me->CastSpell(
                                        m_infernoPos.m_positionX,
                                        m_infernoPos.m_positionY,
                                        m_infernoPos.m_positionZ,
                                        SPELL_INFERNO,
                                        true);

//        LOG_DEBUG(
//            "npcbots",
//              "Cast spell(SPELL_INFERNO id: %u) result=%d",
//              SPELL_INFERNO,
//              result);
        
        if (result == SPELL_FAILED_SUCCESS || result == SPELL_CAST_OK)
        {
            m_hasBotPet = true;

            DelayedPetUnSpawnEvent* spawnEvent = new DelayedPetUnSpawnEvent(me, &m_infernoPos);
            me->m_Events.AddEvent(spawnEvent, me->m_Events.CalculateTime(180000));
        }
    }
}

void BotDreadlordAI::CheckAura(uint32 uiDiff)
{
    if (m_checkAuraTimer > uiDiff || IsCasting())
    {
        return;
    }

    m_checkAuraTimer = 10000;

    if (!me->HasAura(VAMPIRIC_AURA, me->GetGUID()))
    {
        //LOG_DEBUG("npcbots", "Refresh vampiric aura.");
        RefreshAura(VAMPIRIC_AURA);
    }
}

void BotDreadlordAI::RefreshAura(uint32 spellId, int8 count, Unit* target) const
{
    if (count < 0 || count > 1)
    {
        LOG_ERROR(
            "npcbots", 
            "BotDreadlordAI::RefreshAura(): count is out of bounds (%i) for bot %s (entry: %u)",
            int32(count), 
            me->GetName().c_str(), 
            me->GetEntry());

        return;
    }

    if (!spellId)
    {
        LOG_ERROR(
            "npcbots", 
            "BotDreadlordAI::RefreshAura(): spellId is 0 for bot %s (entry: %u)",
            me->GetName().c_str(), 
            me->GetEntry());

        return;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);

    if (!spellInfo)
    {
        LOG_ERROR(
            "npcbots", 
            "BotDreadlordAI::RefreshAura(): Invalid spellInfo for spell %u! Bot - %s (entry: %u)", 
            spellId, 
            me->GetName().c_str(), 
            me->GetEntry());

        return;
    }

    if (!target)
    {
        target = me;
    }

    target->RemoveAurasDueToSpell(spellId);

    if (count)
    {
        target->AddAura(spellInfo, MAX_EFFECT_MASK, target);
    }
}

void BotDreadlordAI::ReduceCooldown(uint32 uiDiff)
{
    if (m_checkAuraTimer > uiDiff)
    {
        m_checkAuraTimer -= uiDiff;
    }
}

// called by BotDreadlordAI::DelayedPetSpawnEvent::Execute(...)
void BotDreadlordAI::UnSummonBotPet()
{
    m_hasBotPet = false;
    LOG_DEBUG("npcbots", "can cast summon inferno again...");
}
