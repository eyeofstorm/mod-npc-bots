/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BotDreadlord.h"
#include "BotMgr.h"
#include "Creature.h"

BotDreadlordAI::BotDreadlordAI(Creature* creature) : BotAI(creature)
{
    m_checkAuraTimer = 0;

    // dreadlord immunities
    m_bot->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_POSSESS, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_CHARM, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_SHAPESHIFT, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_TRANSFORM, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_DISARM, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_DISARM_OFFHAND, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_FEAR, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DISARM, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_POLYMORPH, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FEAR, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_HORROR, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_TURN, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SLEEP, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SILENCE, true);
    m_bot->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SNARE, true);

    InitCustomeSpells();
}

void BotDreadlordAI::InitCustomeSpells()
{
    InitSpellMap(CARRION_SWARM_1, true, false);
    InitSpellMap(SLEEP_1, true, false);
    InitSpellMap(INFERNO_1, true, false);

    // INFERNO (dummy summon)
    SpellInfo* sinfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(INFERNO_1));

    sinfo->SpellFamilyName = SPELLFAMILY_WARLOCK;
    sinfo->SpellLevel = 60;
    sinfo->BaseLevel = 60;
    sinfo->RangeEntry = sSpellRangeStore.LookupEntry(4); //30 yds
    sinfo->CastTimeEntry = sSpellCastTimesStore.LookupEntry(3); //500ms
    sinfo->RecoveryTime = 90000;
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
        if (DoSummonInfernoIfReady(uiDiff))
        {
            return;
        }
    }

    DoMeleeAttackIfReady();
}

bool BotDreadlordAI::DoSummonInfernoIfReady(uint32 uiDiff)
{
    bool isSpellReady = IsSpellReady(INFERNO_1, uiDiff);
    bool isInCombat = m_bot->IsInCombat();
    bool hasMana = m_bot->GetPower(POWER_MANA) >= INFERNAL_COST;

    if (isSpellReady && !m_pet && isInCombat && hasMana)
    {
        if (m_bot->GetVictim())
        {
            LOG_DEBUG("npcbots", "ready to cast spell Inferno upon [%s]...", m_bot->GetVictim()->GetName().c_str());

            m_infernoSpwanPos = m_bot->GetVictim()->GetPosition();
        }
        else
        {
            LOG_DEBUG("npcbots", "ready to cast spell Inferno near by [%s]...", m_bot->GetName().c_str());

            m_bot->GetNearPoint(
                        m_bot,
                        m_infernoSpwanPos.m_positionX,
                        m_infernoSpwanPos.m_positionY,
                        m_infernoSpwanPos.m_positionZ,
                        m_bot->GetObjectSize(),
                        5.0f,
                        0.0f);
        }

        // dummy summon infernal
        SpellCastResult result = m_bot->CastSpell(
                                            m_infernoSpwanPos.m_positionX,
                                            m_infernoSpwanPos.m_positionY,
                                            m_infernoSpwanPos.m_positionZ,
                                            INFERNO_1,
                                            false);

        if (result == SPELL_FAILED_SUCCESS || result == SPELL_CAST_OK)
        {
            SetSpellCooldown(INFERNO_1, 90000);

            return true;
        }
        else
        {
            LOG_DEBUG("npcbots", "cast spell Inferno failed...");

            SetSpellCooldown(INFERNO_1, 0);

            return false;
        }
    }

    return false;
}

void BotDreadlordAI::OnClassSpellGo(SpellInfo const* spellInfo)
{
    uint32 baseId = spellInfo->GetFirstRankSpell()->Id;

    if (baseId == INFERNO_1)
    {
        SpellCastResult result = m_bot->CastSpell(
                                            m_infernoSpwanPos.m_positionX,
                                            m_infernoSpwanPos.m_positionY,
                                            m_infernoSpwanPos.m_positionZ,
                                            SPELL_INFERNO_METEOR_VISUAL,
                                            true);

        if (result == SPELL_FAILED_SUCCESS || result == SPELL_CAST_OK)
        {
            DelayedSummonInfernoEvent* summonInfernoEvent = new DelayedSummonInfernoEvent(m_bot, m_infernoSpwanPos);
            m_bot->m_Events.AddEvent(summonInfernoEvent, m_bot->m_Events.CalculateTime(800));
        }
        else
        {
            LOG_DEBUG("npcbots", "cast spell Inferno meteor visual failed...");

            SetSpellCooldown(INFERNO_1, 0);
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

    if (!m_bot->HasAura(VAMPIRIC_AURA, m_bot->GetGUID()))
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
            m_bot->GetName().c_str(),
            m_bot->GetEntry());

        return;
    }

    if (!spellId)
    {
        LOG_ERROR(
            "npcbots", 
            "BotDreadlordAI::RefreshAura(): spellId is 0 for bot %s (entry: %u)",
            m_bot->GetName().c_str(),
            m_bot->GetEntry());

        return;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);

    if (!spellInfo)
    {
        LOG_ERROR(
            "npcbots", 
            "BotDreadlordAI::RefreshAura(): Invalid spellInfo for spell %u! Bot - %s (entry: %u)", 
            spellId, 
            m_bot->GetName().c_str(),
            m_bot->GetEntry());

        return;
    }

    if (!target)
    {
        target = m_bot;
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

    ReduceSpellCooldown(INFERNO_1, uiDiff);
}

// called by DelayedSummonInfernoEvent::Execute(...)
void BotDreadlordAI::SummonBotPet(const Position *pos)
{
    if (m_pet)
    {
        m_pet->ToTempSummon()->UnSummon();
    }

    uint32 entry = BOT_PET_INFERNAL;

    Creature* myPet = m_bot->SummonCreature(
                                entry,
                                *pos,
                                TEMPSUMMON_MANUAL_DESPAWN);

    if (myPet)
    {
        myPet->SetCreatorGUID(m_master->GetGUID());
        myPet->SetOwnerGUID(m_master->GetGUID());
        myPet->SetFaction(m_master->GetFaction());
        myPet->m_ControlledByPlayer = !IAmFree();
        myPet->SetPvP(m_bot->IsPvP());
        myPet->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
        myPet->SetByteValue(UNIT_FIELD_BYTES_2, 1, m_master->GetByteValue(UNIT_FIELD_BYTES_2, 1));
        myPet->SetUInt32Value(UNIT_CREATED_BY_SPELL, INFERNO_1);

        //infernal is immune to magic
        myPet->CastSpell(myPet, SPELL_INFERNO_EFFECT, true); //damage, stun
        myPet->CastSpell(myPet, IMMOLATION, true);

        //dreadlord immunities
        myPet->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_SHAPESHIFT, true);
        myPet->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_TRANSFORM, true);
        myPet->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_FEAR, true);
        myPet->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_POLYMORPH, true);
        myPet->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FEAR, true);
        myPet->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_HORROR, true);
        myPet->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_TURN, true);
        myPet->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SLEEP, true);
        myPet->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SNARE, true);

        BotMgr::SetBotLevel(myPet, m_master->getLevel(), false);

        BotAI* myPetAI = BotMgr::GetBotAI(myPet);

        if (myPetAI)
        {
            myPetAI->SetBotOwner(m_master);
            myPetAI->StartFollow(m_master);
        }

        m_pet = myPet;

        DelayedUnsummonInfernoEvent* unsummonInfernoEvent = new DelayedUnsummonInfernoEvent(m_bot);
        m_bot->m_Events.AddEvent(unsummonInfernoEvent, m_bot->m_Events.CalculateTime(30000));
    }
    else
    {
        LOG_DEBUG("npcbots", "summon infernal servant faild...");

        SetSpellCooldown(INFERNO_1, 0);
    }
}

// called by DelayedUnsummonInfernoEvent::Execute(...)
void BotDreadlordAI::UnSummonBotPet()
{
    if (m_pet)
    {
        LOG_DEBUG("npcbots", "unsummon bot [%s]...", m_pet->GetName().c_str());
        m_pet->ToTempSummon()->UnSummon();
    }
}

void BotDreadlordAI::SummonedCreatureDespawn(Creature* summon)
{
    if (summon == m_pet)
    {
        LOG_DEBUG("npcbots", "sumoned bot [%s] despawned...", summon->GetName().c_str());
        m_pet = nullptr;
    }
}
