/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BotDreadlord.h"
#include "BotEvents.h"
#include "BotMgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "SpellAuras.h"
#include "Unit.h"

BotDreadlordAI::BotDreadlordAI(Creature* creature) : BotAI(creature)
{
    LOG_INFO("npcbots", "↓↓↓↓↓↓ BotDreadlordAI::BotDreadlordAI (this: 0X%016llX, name: %s)", (unsigned long long)this, creature->GetName().c_str());

    m_checkAuraTimer = 0;
    m_potionTimer = 0;

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

    m_bot->SetReactState(REACT_DEFENSIVE);

    InitCustomeSpells();

    sBotsRegistry->Register(this);

    LOG_INFO("npcbots", "↑↑↑↑↑↑ BotDreadlordAI::BotDreadlordAI (this: 0X%016llX, name: %s)", (unsigned long long)this, creature->GetName().c_str());
}

BotDreadlordAI::~BotDreadlordAI()
{
    LOG_INFO("npcbots", "↓↓↓↓↓↓ BotDreadlordAI::~BotDreadlordAI (this: 0X%016llX, name: %s)", (unsigned long long)this, m_bot->GetName().c_str());

    sBotsRegistry->Unregister(this);

    LOG_INFO("npcbots", "↑↑↑↑↑↑ BotDreadlordAI::~BotDreadlordAI (this: 0X%016llX, name: %s)", (unsigned long long)this, m_bot->GetName().c_str());
}

void BotDreadlordAI::InitCustomeSpells()
{
    InitSpellMap(CARRION_SWARM_1, true, false);
    InitSpellMap(SLEEP_1, true, false);
    InitSpellMap(INFERNO_1, true, false);

    SpellInfo* sinfo = nullptr;

    // VAMPIRIC AURA
    sinfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_VAMPIRIC_AURA));

    sinfo->ProcFlags = PROC_FLAG_DONE_MELEE_AUTO_ATTACK | PROC_FLAG_DONE_SPELL_MELEE_DMG_CLASS;
    sinfo->SchoolMask = SPELL_SCHOOL_MASK_SHADOW | SPELL_SCHOOL_MASK_ARCANE;
    sinfo->PreventionType = SPELL_PREVENTION_TYPE_NONE;
    sinfo->DmgClass = SPELL_DAMAGE_CLASS_NONE;
    sinfo->SpellLevel = 0;
    sinfo->BaseLevel = 0;
    sinfo->MaxTargetLevel = 0;
    sinfo->RangeEntry = sSpellRangeStore.LookupEntry(1); //0 yds
    sinfo->ExplicitTargetMask = TARGET_FLAG_UNIT;
    sinfo->Attributes |= SPELL_ATTR0_IS_ABILITY | SPELL_ATTR0_PASSIVE;
    sinfo->AttributesEx3 |= SPELL_ATTR3_CAN_PROC_FROM_PROCS;
    sinfo->AttributesEx4 |= SPELL_ATTR4_IGNORE_DEFAULT_ARENA_RESTRICTIONS;
    sinfo->AttributesEx7 |= SPELL_ATTR7_DO_NOT_COUNT_FOR_PVP_SCOREBOARD;
    sinfo->Effects[0].Effect = SPELL_EFFECT_APPLY_AREA_AURA_RAID;
    sinfo->Effects[0].ApplyAuraName = SPELL_AURA_MOD_CRIT_DAMAGE_BONUS;
    sinfo->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
    sinfo->Effects[0].BasePoints = 5;
    sinfo->Effects[0].MiscValue = SPELL_SCHOOL_MASK_NORMAL;
    sinfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_40_YARDS);
    sinfo->Effects[1].Effect = SPELL_EFFECT_APPLY_AREA_AURA_RAID;
    sinfo->Effects[1].ApplyAuraName = SPELL_AURA_PROC_TRIGGER_SPELL;
    sinfo->Effects[1].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
    sinfo->Effects[1].BasePoints = 1;
    sinfo->Effects[1].TriggerSpell = SPELL_TRIGGERED_HEAL;
    sinfo->Effects[1].RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_40_YARDS);
    // END VAMPIRIC AURA

    // VAMPIRIC HEAL
    sinfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_TRIGGERED_HEAL));

    sinfo->PreventionType = SPELL_PREVENTION_TYPE_NONE;
    sinfo->DmgClass = SPELL_DAMAGE_CLASS_NONE;
    sinfo->SchoolMask = SPELL_SCHOOL_MASK_SHADOW | SPELL_SCHOOL_MASK_ARCANE;
    sinfo->Attributes &= ~(SPELL_ATTR0_NOT_SHAPESHIFTED);
    sinfo->AttributesEx |= SPELL_ATTR1_NO_REDIRECTION | SPELL_ATTR1_NO_REFLECTION | SPELL_ATTR1_NO_THREAT;
    sinfo->AttributesEx2 |= SPELL_ATTR2_CANT_CRIT;
    sinfo->AttributesEx3 |= SPELL_ATTR3_ALWAYS_HIT | SPELL_ATTR3_INSTANT_TARGET_PROCS | SPELL_ATTR3_CAN_PROC_FROM_PROCS | SPELL_ATTR3_IGNORE_CASTER_MODIFIERS;
    sinfo->Effects[0].BasePoints = 1;
    sinfo->Effects[1].Effect = 0;
    // END VAMPIRIC HEAL

    // SLEEP
    sinfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_SLEEP));
    sinfo->SpellFamilyName = SPELLFAMILY_WARLOCK;
    sinfo->SchoolMask = SPELL_SCHOOL_MASK_SHADOW | SPELL_SCHOOL_MASK_ARCANE;
    sinfo->InterruptFlags = 0xF;
    sinfo->SpellLevel = 0;
    sinfo->BaseLevel = 0;
    sinfo->MaxTargetLevel = 83;
    sinfo->Dispel = DISPEL_MAGIC;
    sinfo->Mechanic = MECHANIC_SLEEP;
    sinfo->RangeEntry = sSpellRangeStore.LookupEntry(4); //30 yds
    sinfo->CastTimeEntry = sSpellCastTimesStore.LookupEntry(3); //500ms
    sinfo->RecoveryTime = SLEEP_CD;
    sinfo->DurationEntry = sSpellDurationStore.LookupEntry(3); //60000ms
    sinfo->ManaCost = SLEEP_COST;
    sinfo->ManaCostPercentage = 0;
    sinfo->ManaCostPerlevel = 0;
    sinfo->AuraInterruptFlags = AURA_INTERRUPT_FLAG_DIRECT_DAMAGE;
    sinfo->ExplicitTargetMask = TARGET_FLAG_UNIT;
    sinfo->Attributes &= ~(SPELL_ATTR0_NOT_SHAPESHIFTED | SPELL_ATTR0_HEARTBEAT_RESIST);
    sinfo->AttributesEx |= SPELL_ATTR1_NO_REDIRECTION | SPELL_ATTR1_NO_REFLECTION;
    sinfo->AttributesEx3 |= SPELL_ATTR3_ALWAYS_HIT;
    sinfo->Effects[1].Effect = SPELL_EFFECT_APPLY_AURA;
    sinfo->Effects[1].ApplyAuraName = SPELL_AURA_MOD_RESISTANCE_PCT;
    sinfo->Effects[1].MiscValue = SPELL_SCHOOL_MASK_NORMAL;
    sinfo->Effects[1].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ENEMY);
    sinfo->Effects[1].BasePoints = -100;
    // END SLEEP

    // CARRION SWARM
    sinfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_CARRION_SWARM));

    sinfo->SpellFamilyName = SPELLFAMILY_WARLOCK;
    sinfo->DmgClass = SPELL_DAMAGE_CLASS_MAGIC;
    sinfo->SchoolMask = SPELL_SCHOOL_MASK_SHADOW | SPELL_SCHOOL_MASK_ARCANE;
    sinfo->PreventionType = SPELL_PREVENTION_TYPE_NONE;
    sinfo->SpellLevel = 40;
    sinfo->BaseLevel = 40;
    sinfo->MaxTargetLevel = 83;
    sinfo->RangeEntry = sSpellRangeStore.LookupEntry(4); //30 yds
    sinfo->RecoveryTime = CARRION_CD;
    sinfo->StartRecoveryCategory = 133;
    sinfo->StartRecoveryTime = 1500;
    sinfo->ManaCost = CARRION_COST;
    sinfo->Attributes &= ~(SPELL_ATTR0_WEARER_CASTS_PROC_TRIGGER);
    sinfo->AttributesEx |= SPELL_ATTR1_NO_REDIRECTION | SPELL_ATTR1_NO_REFLECTION;
    sinfo->AttributesEx2 |= SPELL_ATTR2_CANT_CRIT;
    sinfo->AttributesEx3 |= SPELL_ATTR3_ALWAYS_HIT;
    sinfo->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CONE_ENEMY_104);
    sinfo->Effects[0].BasePoints = 425;
    sinfo->Effects[0].DieSides = 150;
    sinfo->Effects[0].BonusMultiplier = 2.f;
    sinfo->Effects[0].DamageMultiplier = 1.f;
    sinfo->Effects[0].RealPointsPerLevel = 37.5f; //2000 avg at 80
    sinfo->Effects[0].ValueMultiplier = 1.f;
    sinfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(EFFECT_RADIUS_40_YARDS);
    // END CARRION SWARM

    // INFERNO (dummy summon)
    sinfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(INFERNO_1));

    sinfo->SpellFamilyName = SPELLFAMILY_WARLOCK;
    sinfo->SpellLevel = 60;
    sinfo->BaseLevel = 60;
    sinfo->RangeEntry = sSpellRangeStore.LookupEntry(4); //30 yds
    sinfo->CastTimeEntry = sSpellCastTimesStore.LookupEntry(3); //500ms
    sinfo->RecoveryTime = INFERNAL_CD;
    sinfo->StartRecoveryTime = 1500;
    sinfo->StartRecoveryCategory = 133;
    sinfo->ManaCost = INFERNAL_COST;
    sinfo->ExplicitTargetMask = TARGET_FLAG_DEST_LOCATION;
    sinfo->Attributes &= ~(SPELL_ATTR0_IS_ABILITY);
    sinfo->AttributesEx |= SPELL_ATTR1_NO_THREAT;
    sinfo->Effects[0].Effect = SPELL_EFFECT_DUMMY;
    sinfo->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_DEST_DEST);
    sinfo->Effects[0].BasePoints = 1;
    // END INFERNO

    // INFERNO VISUAL (dummy summon)
    sinfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_INFERNO_METEOR_VISUAL));
    sinfo->ExplicitTargetMask = TARGET_FLAG_DEST_LOCATION;
    sinfo->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_DEST_DEST);
    // END INFERNO VISUAL
}

void BotDreadlordAI::UpdateBotCombatAI(uint32 uiDiff)
{
    if (!UpdateVictim())
    {
        return;
    }

    CheckAura(uiDiff);

    if (IsPotionReady())
    {
        if (m_bot->GetPower(POWER_MANA) < CARRION_COST)
        {
            LOG_INFO("npcbots", "bot [%s] drink mana potion.", m_bot->GetName().c_str());

            DrinkPotion(true);
        }
        else if (GetHealthPCT(m_bot) < 50)
        {
            LOG_INFO("npcbots", "bot [%s] drink health potion.", m_bot->GetName().c_str());

            DrinkPotion(false);
        }
    }

    if (IsCasting())
    {
        return;
    }

    if (!IAmFree() && m_bot->HasReactState(REACT_PASSIVE))
    {
        return;
    }
    
    if (DoSummonInfernoIfReady(uiDiff))
    {
        return;
    }

    if (DoCastDreadlordSpellSleep(uiDiff))
    {
        return;
    }

    DoDreadlordAttackIfReady(uiDiff);
}

bool BotDreadlordAI::DoSummonInfernoIfReady(uint32 uiDiff)
{
    bool isSpellReady = IsSpellReady(INFERNO_1, uiDiff);
    bool isInCombat = m_bot->IsInCombat();
    bool hasMana = m_bot->GetPower(POWER_MANA) >= INFERNAL_COST;

    if (isSpellReady && !m_pet && isInCombat && hasMana && Rand() < 60)
    {
        float maxRange = DoGetSpellMaxRange(INFERNO_1);

        Unit* target = FindAOETarget(maxRange, 3);

        if (target)
        {
            m_infernoSpwanPos = target->GetPosition();
        }
        else if (Unit* victim = m_bot->GetVictim())
        {
            m_infernoSpwanPos = victim->GetPosition();
        }
        else
        {
            m_bot->GetNearPoint(
                        m_bot,
                        m_infernoSpwanPos.m_positionX,
                        m_infernoSpwanPos.m_positionY,
                        m_infernoSpwanPos.m_positionZ,
                        m_infernoSpwanPos.m_orientation,
                        5.f,
                        0.f);
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
            SetGlobalCooldown(sSpellMgr->GetSpellInfo(INFERNO_1)->StartRecoveryTime);
            SetSpellCooldown(INFERNO_1, INFERNAL_CD);

            return true;
        }
    }

    return false;
}

bool BotDreadlordAI::DoCastDreadlordSpellSleep(uint32 diff)
{
    bool isSpellReady = IsSpellReady(SLEEP_1, diff);
    bool isInCombat = m_bot->IsInCombat();
    bool isCasting = IsCasting();
    bool hasMana = m_bot->GetPower(POWER_MANA) >= SLEEP_COST;
    uint16 rand = Rand();

    if (!isSpellReady || isCasting || !isInCombat || !hasMana || rand > 50)
    {
        return false;
    }

    // fleeing/casting/solo enemy
    Unit* victim = me->GetVictim();
    float dist = DoGetSpellMaxRange(SLEEP_1);

    if (victim &&
        IsSpellReady(CARRION_SWARM_1, diff) &&
        !CCed(victim) &&
        m_bot->GetDistance(victim) < dist &&
        (victim->IsNonMeleeSpellCast(false, false, true) ||
         (victim->IsInCombat() && victim->getAttackers().size() == 1)))
    {
        SpellCastResult result = m_bot->CastSpell(
                                            victim,
                                            SLEEP_1,
                                            false);

        if (result == SPELL_FAILED_SUCCESS || result == SPELL_CAST_OK)
        {
            SetGlobalCooldown(sSpellMgr->GetSpellInfo(SLEEP_1)->StartRecoveryTime);
            SetSpellCooldown(SLEEP_1, SLEEP_CD);

            return true;
        }
    }

    if (Unit* target = FindCastingTarget(dist, 0, SLEEP_1))
    {
        SpellCastResult result = m_bot->CastSpell(
                                            target,
                                            SLEEP_1,
                                            false);

        if (result == SPELL_FAILED_SUCCESS || result == SPELL_CAST_OK)
        {
            SetGlobalCooldown(sSpellMgr->GetSpellInfo(SLEEP_1)->StartRecoveryTime);
            SetSpellCooldown(SLEEP_1, SLEEP_CD);

            return true;
        }
    }

    if (Unit* target = FindStunTarget(dist))
    {
        SpellCastResult result = m_bot->CastSpell(
                                            target,
                                            SLEEP_1,
                                            false);

        if (result == SPELL_FAILED_SUCCESS || result == SPELL_CAST_OK)
        {
            SetGlobalCooldown(sSpellMgr->GetSpellInfo(SLEEP_1)->StartRecoveryTime);
            SetSpellCooldown(SLEEP_1, SLEEP_CD);

            return true;
        }
    }

    return false;
}

void BotDreadlordAI::DoDreadlordAttackIfReady(uint32 uiDiff)
{
    bool isSpellReady = IsSpellReady(CARRION_SWARM_1, uiDiff);
    bool isInCombat = m_bot->IsInCombat();
    bool hasMana = m_bot->GetPower(POWER_MANA) >= CARRION_COST;
    uint16 rand = Rand();

    if (isSpellReady && isInCombat && hasMana && rand < 80)
    {
        Unit* target = m_bot->GetVictim();

        bool cast = false;

        std::list<Unit*> targets;
        GetNearbyTargetsInConeList(targets, 5);

        if (targets.size() >= 3)
        {
            if (target && m_bot->HasInArc(float(M_PI) / 2, target) && m_bot->GetDistance(target) <= 10 &&
                (GetManaPCT(me) > 60 || m_bot->getAttackers().empty() || GetHealthPCT(me) < 50 || target->HasAura(SLEEP_1)))
            {
                cast = true;
            }
        }

        if (cast)
        {
            // carrion swarm
            SpellCastResult result = m_bot->CastSpell(
                                                m_bot,
                                                GetBotSpellId(CARRION_SWARM_1),
                                                false);

            if (result == SPELL_FAILED_SUCCESS || result == SPELL_CAST_OK)
            {
                SetGlobalCooldown(sSpellMgr->GetSpellInfo(CARRION_SWARM_1)->StartRecoveryTime);
                SetSpellCooldown(CARRION_SWARM_1, CARRION_CD);

                return;
            }
        }
    }

    DoMeleeAttackIfReady();
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
            Events.AddEvent(summonInfernoEvent, Events.CalculateTime(500));
        }
    }
}

void BotDreadlordAI::CheckAura(uint32 uiDiff)
{
    if (m_checkAuraTimer > uiDiff || m_gcdTimer > uiDiff || IsCasting())
    {
        return;
    }

    m_checkAuraTimer = 10000;

    if (!m_bot->HasAura(VAMPIRIC_AURA, m_bot->GetGUID()))
    {
        LOG_DEBUG("npcbots", "bot [%s] refresh vampiric aura.", m_bot->GetName().c_str());

        RefreshAura(VAMPIRIC_AURA, 1, m_bot);
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

    if (target)
    {
        target->RemoveAurasDueToSpell(spellId);

        if (count)
        {
            target->AddAura(spellInfo, MAX_EFFECT_MASK, target);
        }
    }
}

void BotDreadlordAI::UpdateSpellCD(uint32 uiDiff)
{
    if (m_checkAuraTimer > uiDiff)
    {
        m_checkAuraTimer -= uiDiff;
    }

    ReduceSpellCooldown(INFERNO_1, uiDiff);
    ReduceSpellCooldown(SLEEP_1, uiDiff);
    ReduceSpellCooldown(CARRION_SWARM_1, uiDiff);
}

// called by DelayedSummonInfernoEvent::Execute(...)
void BotDreadlordAI::SummonBotPet(const Position *pos)
{
    if (m_pet)
    {
        m_pet->ToTempSummon()->UnSummon();
    }

    Creature* dreadlord = m_bot;

    Creature* infernal = dreadlord->SummonCreature(
                                        BOT_PET_INFERNAL,
                                        *pos,
                                        TEMPSUMMON_MANUAL_DESPAWN);

    if (infernal)
    {
        Unit* master = m_owner ? m_owner : dreadlord;
        BotAI* infernalAI = (BotAI *)infernal->AI();

        infernal->SetOwnerGUID(master->GetGUID());
        infernal->SetCreatorGUID(master->GetGUID());
        infernal->SetByteValue(UNIT_FIELD_BYTES_2, 1, master->GetByteValue(UNIT_FIELD_BYTES_2, 1));
        infernal->SetFaction(master->GetFaction());
        infernal->SetPvP(master->IsPvP());
        infernal->SetPhaseMask(master->GetPhaseMask(), true);
        infernal->SetUInt32Value(UNIT_CREATED_BY_SPELL, INFERNO_1);
        infernal->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
        infernal->m_ControlledByPlayer = true;

        if (m_owner)
        {
            infernal->SetReactState(REACT_DEFENSIVE);
        }

        BotMgr::SetBotLevel(infernal, master->getLevel(), false);

        infernalAI->SetBotOwner(dreadlord);
        infernalAI->StartFollow(dreadlord);

        // damage, stun
        infernal->CastSpell(infernal, SPELL_INFERNO_EFFECT, true);

        // immolation aura
        infernal->CastSpell(infernal, IMMOLATION, true);

        // infernal is immune to magic
        infernal->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_SHAPESHIFT, true);
        infernal->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_TRANSFORM, true);
        infernal->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_FEAR, true);
        infernal->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_POLYMORPH, true);
        infernal->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FEAR, true);
        infernal->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_HORROR, true);
        infernal->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_TURN, true);
        infernal->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SLEEP, true);
        infernal->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SNARE, true);

        m_pet = infernal;

        DelayedUnsummonInfernoEvent* unsummonInfernoEvent = new DelayedUnsummonInfernoEvent(m_bot);
        Events.AddEvent(unsummonInfernoEvent, Events.CalculateTime(INFERNAL_DURATION));
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

bool BotDreadlord::OnGossipHello(Player* player, Creature* bot)
{
    if (bot)
    {
        player->PlayerTalkClass->ClearMenus();

        BotAI* botAI = (BotAI *)bot->AI();

        if (botAI->GetBotOwner())
        {
            Unit const* owner = bot->GetOwner();

            if (owner && owner->GetGUID() == player->GetGUID())
            {
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Dismiss.", GOSSIP_DREADLORD_SENDER_DISMISS, GOSSIP_ACTION_INFO_DEF + 1);
            }

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind", GOSSIP_DREADLORD_SENDER_EXIT_MENU, GOSSIP_ACTION_INFO_DEF + 2);
        }

        player->PlayerTalkClass->SendGossipMenu(GOSSIP_DREADLORD_GREET, bot->GetGUID());
    }

    return true;
}

bool BotDreadlord::OnGossipSelect(Player* player, Creature* bot, uint32 sender, uint32 /*action*/)
{
    player->PlayerTalkClass->ClearMenus();

    switch (sender)
    {
        case GOSSIP_DREADLORD_SENDER_EXIT_MENU: // exit
        {
            break;
        }

        case GOSSIP_DREADLORD_SENDER_MAIN_MENU: // return to main menu
        {
            return OnGossipHello(player, bot);
        }

        case GOSSIP_DREADLORD_SENDER_DISMISS:
        {
            Unit const* owner = bot->GetOwner();

            if (owner && owner->GetGUID() == player->GetGUID())
            {
                BotMgr::DismissBot(bot);
            }

            break;
        }
    }

    player->PlayerTalkClass->SendCloseGossip();

    return true;
}
