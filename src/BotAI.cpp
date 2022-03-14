/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BotAI.h"
#include "BotCommon.h"
#include "BotEvents.h"
#include "BotGridNotifiers.h"
#include "BotMgr.h"
#include "CellImpl.h"
#include "Creature.h"
#include "GameEventMgr.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Log.h"
#include "MapMgr.h"
#include "SpellAuraEffects.h"
#include "Vehicle.h"
#include "Unit.h"

const float MAX_PLAYER_DISTANCE = 100.0f;

enum ePoints
{
    POINT_COMBAT_START  = 0xFFFFFF
};

static uint16 __rand; //calculated for each bot separately once every updateAI tick

BotAI::BotAI(Creature* creature) : ScriptedAI(creature)
{
    LOG_INFO("npcbots", "↓↓↓↓↓↓ BotAI::BotAI (this: 0X%016llX, name: %s)", (unsigned long long)this, creature->GetName().c_str());

    m_gcdTimer = 0;
    m_uiFollowerTimer = 2500;
    m_uiWaitTimer = 0;
    m_uiUpdateTimerMedium = 0;
    m_regenTimer = 0;
    m_energyFraction = 0.f;

    m_uiLeaderGUID = ObjectGuid::Empty;
    m_uiBotState = STATE_FOLLOW_NONE;

    m_bot = creature;

    if (creature->GetOwnerGUID() != ObjectGuid::Empty)
    {
        if (Player* player = ObjectAccessor::FindPlayer(m_bot->GetOwnerGUID()))
        {
            m_owner = player;
        }
        else
        {
            m_owner = ObjectAccessor::GetUnit(*creature, creature->GetOwnerGUID());
        }
    }
    else
    {
        m_owner = nullptr;
    }

    m_pet = nullptr;

    m_isFeastMana = false;
    m_isFeastHealth = false;
    m_isDoUpdateMana = false;

    m_botClass = CLASS_NONE;
    m_botSpec = BOT_SPEC_DEFAULT;

    m_haste = 0;
    m_hit = 0.f;
    m_parry = 0.f;
    m_dodge = 0.f;
    m_block = 0.f;
    m_crit = 0.f;
    m_dmgTakenPhy = 1.f;
    m_dmgTakenMag = 1.f;
    m_armorPen = 0.f;
    m_expertise = 0;
    m_spellPower = 0;
    m_spellPen = 0;
    m_defense = 0;
    m_blockValue = 1;

    LOG_INFO("npcbots", "↑↑↑↑↑↑ BotAI::BotAI (this: 0X%016llX, name: %s)", (unsigned long long)this, creature->GetName().c_str());
}

BotAI::~BotAI()
{
    LOG_INFO("npcbots", "↓↓↓↓↓↓ BotAI::~BotAI (this: 0X%016llX, name: %s)", (unsigned long long)this, m_bot->GetName().c_str());

    while (!m_spells.empty())
    {
        BotSpellMap::iterator itr = m_spells.begin();
        delete itr->second;
        m_spells.erase(itr);
    }

    LOG_INFO("npcbots", "↑↑↑↑↑↑ BotAI::~BotAI (this: 0X%016llX, name: %s)", (unsigned long long)this, m_bot->GetName().c_str());
}

uint32 BotAI::GetBotClass() const
{
    switch (m_botClass)
    {
    case BOT_CLASS_DREADLORD:
        return BOT_CLASS_PALADIN;
    default:
        return m_botClass;
    }
}

void BotAI::MovementInform(uint32 motionType, uint32 pointId)
{
    if (motionType != POINT_MOTION_TYPE)
    {
        return;
    }

    if (pointId == POINT_COMBAT_START)
    {
        if (GetLeaderForFollower())
        {
            if (!HasBotState(STATE_FOLLOW_PAUSED))
            {
                AddBotState(STATE_FOLLOW_RETURNING);
            }
        }
        else
        {
            LOG_DEBUG("npcbots", "bot [%s] has no owner. stop follow.", m_bot->GetName().c_str());

            if (HasBotState(STATE_FOLLOW_INPROGRESS))
            {
                RemoveBotState(STATE_FOLLOW_INPROGRESS);
                AddBotState(STATE_FOLLOW_COMPLETE);
            }
        }
    }
}

void BotAI::AttackStart(Unit* who)
{
    if (!who)
    {
        return;
    }

    if (m_bot->Attack(who, true))
    {
        if (m_bot->HasUnitState(UNIT_STATE_FOLLOW))
        {
            m_bot->ClearUnitState(UNIT_STATE_FOLLOW);
        }

        if (IsCombatMovementAllowed())
        {
            m_bot->GetMotionMaster()->MoveChase(who);
        }
    }
}

void BotAI::MoveInLineOfSight(Unit* who)
{
    if (m_bot->GetVictim())
    {
        return;
    }

    if (!IAmFree() && m_bot->HasReactState(REACT_PASSIVE))
    {
        return;
    }

    if (!m_bot->HasUnitState(UNIT_STATE_STUNNED) &&
        who->isTargetableForAttack(true, m_bot) &&
        who->isInAccessiblePlaceFor(m_bot))
    {
        if (AssistPlayerInCombat(who))
        {
            if (Player* player = who->GetVictim()->GetCharmerOrOwnerPlayerOrPlayerItself())
            {
//                LOG_DEBUG(
//                    "npcbots",
//                    "bot [%s] start assist player [%s] attack creature [%s] in LOS.",
//                    m_bot->GetName().c_str(),
//                    player->GetName().c_str(),
//                    who->GetName().c_str());
            }

            return;
        }
    }

    if (m_bot->CanStartAttack(who))
    {
        if (m_bot->HasUnitState(UNIT_STATE_DISTRACTED))
        {
            m_bot->ClearUnitState(UNIT_STATE_DISTRACTED);
            m_bot->GetMotionMaster()->Clear();
        }

        AttackStart(who);

//        LOG_DEBUG(
//              "npcbots",
//              "bot [%s] start attack creature [%s] in LOS.",
//              m_bot->GetName().c_str(),
//              who->GetName().c_str());

        return;
    }
}

void BotAI::EnterEvadeMode()
{
    m_bot->RemoveAllAuras();
    m_bot->DeleteThreatList();
    m_bot->CombatStop(true);
    m_bot->SetLootRecipient(nullptr);

    if (HasBotState(STATE_FOLLOW_INPROGRESS))
    {
        if (m_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
        {
//            LOG_DEBUG("npcbots", "bot [%s] left combat, returning to combat start position.", m_bot->GetName().c_str());

            float fPosX, fPosY, fPosZ;
            m_bot->GetPosition(fPosX, fPosY, fPosZ);

            m_bot->GetMotionMaster()->MovePoint(POINT_COMBAT_START, fPosX, fPosY, fPosZ);
        }
    }
    else
    {
        if (m_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
        {
//            LOG_DEBUG("npcbots", "bot [%s] left combat, returning to home.", m_bot->GetName().c_str());

            m_bot->GetMotionMaster()->MoveTargetedHome();
        }
    }

    Reset();
}

void BotAI::EnterCombat(Unit* /*victim*/)
{
    // clear gossip during combat.
    if (m_bot->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP))
    {
        m_bot->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
    }
}

void BotAI::JustDied(Unit* /*killer*/)
{
    if (!HasBotState(STATE_FOLLOW_INPROGRESS) || !m_uiLeaderGUID)
    {
        return;
    }

    if (!IAmFree())
    {
        if (Unit* owner = GetBotOwner())
        {
            if (Player* player = owner->ToPlayer())
            {
                if (Group* grp = player->GetGroup())
                {
                    if (grp->IsMember(m_bot->GetGUID()))
                    {
                        grp->SendUpdate();
                    }
                }
            }
        }
    }

//    m_bot->DespawnOrUnsummon();
}

void BotAI::JustRespawned()
{
    m_uiBotState = STATE_FOLLOW_NONE;

    if (!IsCombatMovementAllowed())
    {
        SetCombatMovement(true);
    }

    if (m_bot->GetFaction() != m_bot->GetCreatureTemplate()->faction)
    {
        m_bot->SetFaction(m_bot->GetCreatureTemplate()->faction);
    }

    Reset();
}

bool BotAI::OnBeforeCreatureUpdate(uint32 uiDiff)
{
    UpdateCommonTimers(uiDiff);

    return true;
}

bool BotAI::UpdateCommonBotAI(uint32 uiDiff)
{
    m_lastUpdateDiff = uiDiff;

    UpdateFollowerAI(uiDiff);

    if (m_uiUpdateTimerMedium <= uiDiff)
    {
        m_uiUpdateTimerMedium = 500;

        if (m_bot->IsInWorld())
        {
            if (Unit* owner = GetBotOwner())
            {
                if (Player* player = owner->ToPlayer())
                {
                    if (Group const* grp = player->GetGroup())
                    {
                        if (grp->IsMember(m_bot->GetGUID()))
                        {
                            WorldPacket data;
                            BuildGrouUpdatePacket(&data);

                            for (GroupReference const* itr = grp->GetFirstMember();
                                itr != nullptr;
                                itr = itr->next())
                            {
                                if (itr->GetSource())
                                {
                                    itr->GetSource()->GetSession()->SendPacket(&data);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (!m_bot->IsAlive())
    {
        return false;
    }

    if (m_isDoUpdateMana)
    {
        m_isDoUpdateMana = false;
        OnManaUpdate();
    }

    Regenerate();

    // update flags
    if (!m_bot->IsInCombat())
    {
        if (!m_bot->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP))
        {
            m_bot->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        }

        if (m_bot->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PET_IN_COMBAT))
        {
            m_bot->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PET_IN_COMBAT);
        }
    }

    UpdateBotRations();

    if (DelayUpdateIfNeeded())
    {
        return false;
    }

    GenerateRand();

    if (!m_bot->GetVehicle() && CCed(m_bot))
    {
        return false;
    }

    UpdateMountedState();
    UpdateStandState();

    return true;
}

void BotAI::OnManaUpdate() const
{
    if (m_bot->GetMaxPower(POWER_MANA) <= 1)
    {
        return;
    }

    uint8 mylevel = m_bot->getLevel();

    bool fullmana = m_bot->GetPower(POWER_MANA) == m_bot->GetMaxPower(POWER_MANA);
    float pct = fullmana ? 100.f : (float(m_bot->GetPower(POWER_MANA)) * 100.f) / float(m_bot->GetMaxPower(POWER_MANA));
    float baseMana = 0.0f;

    if (m_botClass == BOT_CLASS_BM)
    {
        baseMana = BASE_MANA_1_BM + (BASE_MANA_10_BM - BASE_MANA_1_BM) * (mylevel / 81.f);
    }
    else if (m_botClass == BOT_CLASS_SPHYNX)
    {
        baseMana = BASE_MANA_SPHYNX;
    }
    else if (m_botClass == BOT_CLASS_ARCHMAGE)
    {
        baseMana = BASE_MANA_1_ARCHMAGE + (BASE_MANA_10_ARCHMAGE - BASE_MANA_1_ARCHMAGE) * ((mylevel - 20) / 81.f);
    }
    else if (m_botClass == BOT_CLASS_DREADLORD)
    {
        baseMana = BASE_MANA_1_DREADLORD + (BASE_MANA_10_DREADLORD - BASE_MANA_1_DREADLORD) * ((mylevel - 60) / 83.f);
    }
    else if (m_botClass == BOT_CLASS_SPELLBREAKER)
    {
        baseMana = BASE_MANA_SPELLBREAKER;
    }
    else if (m_botClass == BOT_CLASS_DARK_RANGER)
    {
        baseMana = BASE_MANA_1_DARK_RANGER + (BASE_MANA_10_DARK_RANGER - BASE_MANA_1_DARK_RANGER) * ((mylevel - 40) / 82.f);
    }
    else if (m_botClass == BOT_CLASS_NECROMANCER)
    {
        baseMana = BASE_MANA_NECROMANCER;
    }
    else
    {
        baseMana = m_classLevelInfo->BaseMana;
    }

    LOG_DEBUG(
        "npcbots",
        "BotAI::OnManaUpdate() => bot [%s], m_botClass: %u, Max mana: %u, m_classLevelInfo->BaseMana: %u, pct: %.2f, baseMana: %.2f",
        m_bot->GetName().c_str(),
        m_botClass,
        m_bot->GetMaxPower(POWER_MANA),
        m_classLevelInfo->BaseMana,
        pct,
        baseMana);

    m_bot->SetCreateMana(m_classLevelInfo->BaseMana);

    float intellectValue = GetTotalBotStat(BOT_STAT_MOD_INTELLECT);

    intellectValue -= std::min<float>(m_bot->GetCreateStat(STAT_INTELLECT), 20.f); //not a mistake
    intellectValue = std::max<float>(intellectValue, 0.f);

    float intellectMult = m_botClass < BOT_CLASS_EX_START ? 15.f : IsHeroExClass(m_botClass) ? 5.f : 1.5f;

    baseMana += intellectValue * intellectMult + 20.f;
    baseMana += GetTotalBotStat(BOT_STAT_MOD_MANA);

    //mana bonuses
    uint8 bonuspct = 0;

    //Fel Vitality
    if (m_botClass == BOT_CLASS_WARLOCK && mylevel >= 15)
    {
        bonuspct += 3;
    }

    if (bonuspct)
    {
        baseMana = (baseMana * (100 + bonuspct)) / 100;
    }

    baseMana = baseMana * m_bot->GetCreatureTemplate()->ModMana;

    LOG_DEBUG(
        "npcbots",
        "BotAI::OnManaUpdate() => bot [%s], (1) intellectValue: %.2f, intellectMult: %.2f, baseMana: %.2f",
        m_bot->GetName().c_str(),
        intellectValue,
        intellectMult,
        baseMana);

    if (baseMana < m_bot->GetMaxPower(POWER_MANA))
    {
        baseMana = m_bot->GetMaxPower(POWER_MANA);
    }

    m_bot->SetModifierValue(UNIT_MOD_MANA, BASE_VALUE, baseMana);
    m_bot->UpdateMaxPower(POWER_MANA);

    LOG_DEBUG(
        "npcbots",
        "BotAI::OnManaUpdate() => bot [%s], (2) Max mana: %u, Current Mana: %u",
        m_bot->GetName().c_str(),
        m_bot->GetMaxPower(POWER_MANA),
        m_bot->GetPower(POWER_MANA));

    m_bot->SetPower(POWER_MANA, fullmana ? m_bot->GetMaxPower(POWER_MANA) : uint32(0.5f + float(m_bot->GetMaxPower(POWER_MANA)) * pct / 100.f)); //restore pct

    LOG_DEBUG(
        "npcbots",
        "BotAI::OnManaUpdate() => bot [%s], (3) Max mana: %u, Current Mana: %u",
        m_bot->GetName().c_str(),
        m_bot->GetMaxPower(POWER_MANA),
        m_bot->GetPower(POWER_MANA));

    OnManaRegenUpdate();
}

//Mana regen for minions
void BotAI::OnManaRegenUpdate() const
{
    //regen_normal
    uint8 mylevel = m_bot->getLevel();
    float value = IAmFree() ? mylevel / 2 : 0; //200/0 mp5 at 80

    float power_regen_mp5;
    int32 modManaRegenInterrupt;

    if (m_botClass < BOT_CLASS_EX_START)
    {
        // Mana regen from spirit and intellect
        float spiregen = 0.001f;

        if (GtRegenMPPerSptEntry const* moreRatio = sGtRegenMPPerSptStore.LookupEntry((m_botClass - 1) * GT_MAX_LEVEL + mylevel - 1))
        {
            spiregen = moreRatio->ratio * GetTotalBotStat(BOT_STAT_MOD_SPIRIT);
        }

        // PCT bonus from SPELL_AURA_MOD_POWER_REGEN_PERCENT aura on spirit base regen
        value += sqrt(GetTotalBotStat(BOT_STAT_MOD_INTELLECT)) * spiregen * m_bot->GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);

        // regen from SPELL_AURA_MOD_POWER_REGEN aura (per second)
        power_regen_mp5 = 0.2f * (m_bot->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) + GetTotalBotStat(BOT_STAT_MOD_MANA_REGENERATION));

        if (IAmFree())
        {
            power_regen_mp5 += float(mylevel);
        }

        // bonus from SPELL_AURA_MOD_MANA_REGEN_FROM_STAT aura
        Unit::AuraEffectList const& regenAura = m_bot->GetAuraEffectsByType(SPELL_AURA_MOD_MANA_REGEN_FROM_STAT);

        for (Unit::AuraEffectList::const_iterator i = regenAura.begin(); i != regenAura.end(); ++i)
        {
            power_regen_mp5 += m_bot->GetStat(Stats((*i)->GetMiscValue())) * (*i)->GetAmount() * 0.002f; //per second
        }

        //bot also receive bonus from SPELL_AURA_MOD_POWER_REGEN_PERCENT for mp5 regen
        power_regen_mp5 *= m_bot->GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);

        // Set regen rate in cast state apply only on spirit based regen
        modManaRegenInterrupt = std::min<int32>(100, m_bot->GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT));
    }
    else
    {
        modManaRegenInterrupt = 100;
        power_regen_mp5 = 0.0f;

        if (IsHeroExClass(m_botClass))
        {
            float basemana;

            if (BOT_CLASS_EX_START == BOT_CLASS_BM)
            {
                basemana = BASE_MANA_1_BM;
            }
            else if (BOT_CLASS_EX_START == BOT_CLASS_ARCHMAGE)
            {
                basemana = BASE_MANA_1_ARCHMAGE;
            }
            else if (BOT_CLASS_EX_START == BOT_CLASS_DREADLORD)
            {
                basemana = BASE_MANA_1_DREADLORD;
            }
            else if (BOT_CLASS_EX_START == BOT_CLASS_DARK_RANGER)
            {
                basemana = BASE_MANA_1_DARK_RANGER;
            }
            else
            {
                basemana = 0.f;
            }

            value = basemana * 0.0087f + 0.08f * GetTotalBotStat(BOT_STAT_MOD_INTELLECT);
            value += 0.2f * (m_bot->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) + GetTotalBotStat(BOT_STAT_MOD_MANA_REGENERATION));
            value *= m_bot->GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);
        }
        else if (m_botClass == BOT_CLASS_SPHYNX)
        {
            value = CalculatePct(m_bot->GetCreateMana(), 2); //-2% basemana/sec
        }
        else if (m_botClass == BOT_CLASS_SPELLBREAKER)
        {
            value = 4.f; //base 0.8/sec
            value += 0.2f * (m_bot->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) + GetTotalBotStat(BOT_STAT_MOD_MANA_REGENERATION));
            value *= m_bot->GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);
        }
        else if (m_botClass == BOT_CLASS_NECROMANCER)
        {
            value = 7.5f; //base 1.5/sec
            value += 0.2f * (m_bot->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) + GetTotalBotStat(BOT_STAT_MOD_MANA_REGENERATION));
            value *= m_bot->GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);
        }
        else
        {
            value = 0;
        }

        if (IAmFree())
        {
            value += float(mylevel);
        }
    }

    // Unrelenting Storm, Dreamstate: 12% of intellect as mana regen always (divided by 5)
    if ((m_botClass == BOT_CLASS_SHAMAN && m_botSpec == BOT_SPEC_SHAMAN_ELEMENTAL) ||
        (m_botClass == BOT_CLASS_DRUID && m_botSpec == BOT_SPEC_DRUID_BALANCE))
    {
        power_regen_mp5 += 0.024f * GetTotalBotStat(BOT_STAT_MOD_INTELLECT);
    }

    m_bot->SetStatFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER, power_regen_mp5 + CalculatePct(value, modManaRegenInterrupt));
    m_bot->SetStatFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER, power_regen_mp5 + value);
}

inline float BotAI::GetTotalBotStat(uint8 stat) const
{
    int32 value = 0;

// *********************************************************************
// TODO: implement this => equipment bonus.
//    for (uint8 slot = BOT_SLOT_MAINHAND; slot != BOT_INVENTORY_SIZE; ++slot)
//    {
//        value += _stats[slot][stat];
//    }
// *********************************************************************

    uint8 lvl = m_bot->getLevel();
    float fval = float(value);

    switch (stat)
    {
        case BOT_STAT_MOD_STRENGTH:
        {
            fval += m_bot->GetTotalStatValue(STAT_STRENGTH);

            switch (m_botClass)
            {
                case BOT_CLASS_WARRIOR:
                {
                    //Vitality, Strength of Arms
                    if (lvl >= 45 && m_botSpec == BOT_SPEC_WARRIOR_PROTECTION)
                    {
                        fval *= 1.06f;
                    }

                    if (lvl >= 40 && m_botSpec == BOT_SPEC_WARRIOR_ARMS)
                    {
                        fval *= 1.04f;
                    }

                    //Improved Berserker Stance part 1 (all stances)
                    if (lvl >= 45 && m_botSpec == BOT_SPEC_WARRIOR_FURY)
                    {
                        fval *= 1.2f;
                    }

                    break;
                }
                case BOT_CLASS_PALADIN:
                {
                    //Divine Strength
                    if (lvl >= 10)
                    {
                        fval *= 1.15f;
                    }

                    break;
                }
                case BOT_CLASS_DEATH_KNIGHT:
                {
                    //Ravenous Dead part 1
                    //Endless Winter part 1
                    //Veteran of the Third War part 1
                    //Abomination's might part 2
                    if (lvl >= 56)
                    {
                        fval *= 1.03f;
                    }

                    if (lvl >= 58)
                    {
                        fval *= 1.04f;
                    }

                    if (lvl >= 59 && m_botSpec == BOT_SPEC_DK_BLOOD)
                    {
                        fval *= 1.06f;
                    }

                    if (lvl >= 60 && m_botSpec == BOT_SPEC_DK_BLOOD)
                    {
                        fval *= 1.02f;
                    }

                    //Frost Presence passive / Improved Frost Presence
                    if (lvl >= 61 && GetBotStance() == DEATH_KNIGHT_FROST_PRESENCE && m_botSpec == BOT_SPEC_DK_FROST)
                    {
                        fval *= 1.08f;
                    }

                    break;
                }
                case BOT_CLASS_DRUID:
                {
                    //Survival of the Fittest, Improved Mark of the Wild
                    if (lvl >= 35 && m_botSpec == BOT_SPEC_DRUID_FERAL)
                    {
                        fval *= 1.08f;
                    }
                    else if (lvl >= 10)
                    {
                        fval *= 1.02f;
                    }

                    break;
                }
                default:
                {
                    break;
                }
            }

            break;
        }

        case BOT_STAT_MOD_AGILITY:
        {
            fval += m_bot->GetTotalStatValue(STAT_AGILITY);

            switch (m_botClass)
            {
                case BOT_CLASS_HUNTER:
                {
                    //Combat Experience, Lightning Reflexes
                    if (lvl >= 35 && m_botSpec == BOT_SPEC_HUNTER_MARKSMANSHIP)
                    {
                        fval *= 1.04f;
                    }

                    if (lvl >= 35 && m_botSpec == BOT_SPEC_HUNTER_SURVIVAL)
                    {
                        fval *= 1.15f;
                    }

                    //Hunting Party
                    if (lvl >= 35 && m_botSpec == BOT_SPEC_HUNTER_SURVIVAL)
                    {
                        fval *= 1.03f;
                    }

                    break;
                }

                case BOT_CLASS_ROGUE:
                {
                    //Sinister Calling
                    if (lvl >= 45 && m_botSpec == BOT_SPEC_ROGUE_SUBTLETY)
                    {
                        fval *= 1.15f;
                    }

                    break;
                }

                case BOT_CLASS_DRUID:
                {
                    //Survival of the Fittest, Improved Mark of the Wild
                    if (lvl >= 35 && m_botSpec == BOT_SPEC_DRUID_FERAL)
                    {
                        fval *= 1.08f;
                    }
                    else if (lvl >= 10)
                    {
                        fval *= 1.02f;
                    }

                    break;
                }

                default:
                {
                    break;
                }
            }

            break;
        }

        case BOT_STAT_MOD_STAMINA:
        {
            fval += m_bot->GetTotalStatValue(STAT_STAMINA);

            switch (m_botClass)
            {
                case BOT_CLASS_WARRIOR:
                {
                    //Vitality, Strength of Arms
                    if (lvl >= 45 && m_botSpec == BOT_SPEC_WARRIOR_PROTECTION)
                    {
                        fval *= 1.09f;
                    }

                    if (lvl >= 40 && m_botSpec == BOT_SPEC_WARRIOR_ARMS)
                    {
                        fval *= 1.04f;
                    }

                    break;
                }

                case BOT_CLASS_PALADIN:
                {
                    //Combat Expertise, Sacred Duty
                    if (lvl >= 45 && m_botSpec == BOT_SPEC_PALADIN_PROTECTION)
                    {
                        fval *= 1.06f;
                    }

                    if (lvl >= 35 && m_botSpec == BOT_SPEC_PALADIN_PROTECTION)
                    {
                        fval *= 1.04f;
                    }

                    break;
                }

                case BOT_CLASS_HUNTER:
                {
                    //Survivalist
                    if (lvl >= 20)
                    {
                        fval *= 1.1f;
                    }

                    break;
                }

                case BOT_CLASS_ROGUE:
                {
                    //Lightning Reflexes part 2
                    if (lvl >= 25 && m_botSpec == BOT_SPEC_ROGUE_COMBAT)
                    {
                        fval *= 1.04f;
                    }

                    break;
                }

                case BOT_CLASS_PRIEST:
                {
                    //Improved Power Word: Shield
                    if (lvl >= 15)
                    {
                        fval *= 1.04f;
                    }

                    break;
                }

                case BOT_CLASS_DEATH_KNIGHT:
                {
                    //Veteran of the Third War part 2
                    if (lvl >= 59 && m_botSpec == BOT_SPEC_DK_BLOOD)
                    {
                        fval *= 1.03f;
                    }

                    break;
                }

                case BOT_CLASS_WARLOCK:
                {
                    //Demonic Embrace: 10% stam bonus
                    if (lvl >= 10)
                    {
                        fval *= 1.1f;
                    }

                    break;
                }

                case BOT_CLASS_DRUID:
                {
                    if (GetBotStance() == DRUID_BEAR_FORM)
                    {
                        //Bear form: stamina bonus base 25%
                        //Heart of the Wild: 10% stam bonus for bear
                        fval *= 1.25f;
                        if (lvl >= 35 && m_botSpec == BOT_SPEC_DRUID_FERAL)
                            fval *= 1.1f;
                    }

                    //Survival of the Fittest, Improved Mark of the Wild
                    if (lvl >= 35 && m_botSpec == BOT_SPEC_DRUID_FERAL)
                    {
                        fval *= 1.06f;
                    }

                    if (lvl >= 10)
                    {
                        fval *= 1.02f;
                    }

                    break;
                }

                default:
                {
                    break;
                }
            }

            break;
        }

        case BOT_STAT_MOD_INTELLECT:
        {
            fval += m_bot->GetTotalStatValue(STAT_INTELLECT);

            switch (m_botClass)
            {
                case BOT_CLASS_PALADIN:
                {
                    // Divine Intellect
                    if (lvl >= 15)
                    {
                        fval *= 1.1f;
                    }

                    break;
                }

                case BOT_CLASS_HUNTER:
                {
                    //Combat Experience
                    if (lvl >= 35 && m_botSpec == BOT_SPEC_HUNTER_MARKSMANSHIP)
                    {
                        fval *= 1.04f;
                    }

                    break;
                }

                case BOT_CLASS_MAGE:
                {
                    //Arcane Mind
                    if (lvl >= 30 && m_botSpec == BOT_SPEC_MAGE_ARCANE)
                    {
                        fval *= 1.15f;
                    }

                    break;
                }

                case BOT_CLASS_PRIEST:
                {
                    //Mental Strength
                    if (lvl >= 30 && m_botSpec == BOT_SPEC_PRIEST_DISCIPLINE)
                    {
                        fval *= 1.15f;
                    }

                    break;
                }

                case BOT_CLASS_SHAMAN:
                {
                    //Ancestral Knowledge
                    if (lvl >= 10)
                    {
                        fval *= 1.1f;
                    }

                    break;
                }

                case BOT_CLASS_DRUID:
                {
                    //Survival of the Fittest, Improved Mark of the Wild
                    if (lvl >= 35 && m_botSpec == BOT_SPEC_DRUID_FERAL)
                    {
                        fval *= 1.08f;
                    }
                    else if (lvl >= 10)
                    {
                        fval *= 1.02f;
                    }

                    //Furor (Moonkin Form)
                    if (GetBotStance() == DRUID_MOONKIN_FORM)
                    {
                        fval *= 1.1f;
                    }

                    //Heart of the Wild: ferals only (tanks included)
                    if (lvl >= 35 && m_botSpec == BOT_SPEC_DRUID_FERAL)
                    {
                        fval *= 1.2f;
                    }

                    break;
                }

                default:
                {
                    break;
                }
            }

            break;
        }

        case BOT_STAT_MOD_SPIRIT:
        {
            fval += m_bot->GetTotalStatValue(STAT_SPIRIT);

            switch (m_botClass)
            {
                case BOT_CLASS_PRIEST:
                {
                    //Spirit of Redemption part 1
                    if (lvl >= 30 && m_botSpec == BOT_SPEC_PRIEST_HOLY)
                    {
                        fval *= 1.05f;
                    }

                    //Enlightenment part 1
                    if (lvl >= 35 && m_botSpec == BOT_SPEC_PRIEST_DISCIPLINE)
                    {
                        fval *= 1.06f;
                    }

                    break;
                }

                case BOT_CLASS_MAGE:
                {
                    //Student of the Mind
                    if (lvl >= 20)
                    {
                        fval *= 1.1f;
                    }

                    break;
                }

                case BOT_CLASS_DRUID:
                {
                    //Survival of the Fittest, Improved Mark of the Wild
                    if (lvl >= 35 && m_botSpec == BOT_SPEC_DRUID_FERAL)
                    {
                        fval *= 1.08f;
                    }
                    else if (lvl >= 10)
                    {
                        fval *= 1.02f;
                    }

                    //Living Spirit
                    if (lvl >= 40 && m_botSpec == BOT_SPEC_DRUID_RESTORATION)
                    {
                        fval *= 1.15f;
                    }

                    break;
                }

                default:
                {
                    break;
                }
            }

            break;
        }
    }

    return fval;
}

uint8 BotAI::GetBotStance() const
{
    return BOT_STANCE_NONE;
}

bool BotAI::DelayUpdateIfNeeded()
{
    Unit* owner = GetBotOwner();

    if (m_uiWaitTimer > m_lastUpdateDiff || (owner && !owner->IsInWorld()))
    {
        return true;
    }

    if (IAmFree())
    {
        m_uiWaitTimer = m_bot->IsInCombat() ? 500 : urand(750, 1250);
    }
    else if ((owner && !owner->GetMap()->IsRaid()))
    {
        m_uiWaitTimer = std::min<uint32>(uint32(50 * (BotMgr::GetBotsCount(owner) - 1) + __rand + __rand), 500);
    }
    else
    {
        m_uiWaitTimer = __rand;
    }

    return false;
}

void BotAI::BuildGrouUpdatePacket(WorldPacket* data)
{
    uint32 mask = GROUP_UPDATE_FULL;

    if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)
    {
        mask |= (GROUP_UPDATE_FLAG_CUR_POWER | GROUP_UPDATE_FLAG_MAX_POWER);
    }

    if (mask & GROUP_UPDATE_FLAG_PET_POWER_TYPE)
    {
        mask |= (GROUP_UPDATE_FLAG_PET_CUR_POWER | GROUP_UPDATE_FLAG_PET_MAX_POWER);
    }

    uint32 byteCount = 0;

    for (uint8 i = 1; i < GROUP_UPDATE_FLAGS_COUNT; ++i)
    {
        if (mask & (1 << i))
        {
            byteCount += GroupUpdateLength[i];
        }
    }

    data->Initialize(SMSG_PARTY_MEMBER_STATS, 8 + 4 + byteCount);
    *data << m_bot->GetGUID().WriteAsPacked();
    *data << uint32(mask);

    if (mask & GROUP_UPDATE_FLAG_STATUS)
    {
        uint16 playerStatus = MEMBER_STATUS_ONLINE;

        if (m_bot->IsPvP())
        {
            playerStatus |= MEMBER_STATUS_PVP;
        }

        if (!m_bot->IsAlive())
        {
            playerStatus |= MEMBER_STATUS_DEAD;
        }

        if (m_bot->HasByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP))
        {
            playerStatus |= MEMBER_STATUS_PVP_FFA;
        }

        *data << uint16(playerStatus);
    }

    if (mask & GROUP_UPDATE_FLAG_CUR_HP)
    {
        *data << uint32(m_bot->GetHealth());
    }

    if (mask & GROUP_UPDATE_FLAG_MAX_HP)
    {
        *data << uint32(m_bot->GetMaxHealth());
    }

    Powers powerType = m_bot->getPowerType();

    if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)
    {
        *data << uint8(powerType);
    }

    if (mask & GROUP_UPDATE_FLAG_CUR_POWER)
    {
        *data << uint16(m_bot->GetPower(powerType));
    }

    if (mask & GROUP_UPDATE_FLAG_MAX_POWER)
    {
        *data << uint16(m_bot->GetMaxPower(powerType));
    }

    if (mask & GROUP_UPDATE_FLAG_LEVEL)
    {
        *data << uint16(m_bot->getLevel());
    }

    if (mask & GROUP_UPDATE_FLAG_ZONE)
    {
        *data << uint16(m_bot->GetZoneId());
    }

    if (mask & GROUP_UPDATE_FLAG_POSITION)
    {
        *data << uint16(m_bot->GetPositionX());
        *data << uint16(m_bot->GetPositionY());
    }

    if (mask & GROUP_UPDATE_FLAG_VEHICLE_SEAT)
    {
        if (Vehicle* veh = m_bot->GetVehicle())
        {
            *data << uint32(veh->GetVehicleInfo()->m_seatID[m_bot->m_movementInfo.transport.seat]);
        }
        else
        {
            *data << uint32(0);
        }
    }
}

void BotAI::UpdateCommonTimers(uint32 uiDiff)
{
    Events.Update(uiDiff);

    UpdateSpellCD(uiDiff);

    if (m_gcdTimer > uiDiff)
    {
        m_gcdTimer -= uiDiff;
    }

    if (m_uiWaitTimer > uiDiff)
    {
        m_uiWaitTimer -= uiDiff;
    }

    if (m_potionTimer > uiDiff && (m_potionTimer < POTION_CD || !m_bot->IsInCombat()))
    {
        m_potionTimer -= uiDiff;
    }

    if (m_uiUpdateTimerMedium > uiDiff)
    {
        m_uiUpdateTimerMedium -= uiDiff;
    }
}

void BotAI::UpdateFollowerAI(uint32 uiDiff)
{
    Unit* victim = m_bot->GetVictim();

    if (HasBotState(STATE_FOLLOW_INPROGRESS) && !victim)
    {
        if (m_uiFollowerTimer <= uiDiff)
        {
            m_uiFollowerTimer = 1000;

            if (Unit* leader = GetLeaderForFollower())
            {
                bool bIsMaxRangeExceeded = true;

                if (HasBotState(STATE_FOLLOW_RETURNING))
                {
//                    LOG_DEBUG("npcbots", "bot [%s] is returning to leader.", m_bot->GetName().c_str());

                    RemoveBotState(STATE_FOLLOW_RETURNING);
                    m_bot->GetMotionMaster()->MoveFollow(leader, BOT_FOLLOW_DIST, BOT_FOLLOW_ANGLE);

                    return;
                }

                Player *player = leader->ToPlayer();

                if (player)
                {
                    if (Group* group = player->GetGroup())
                    {
                        for (GroupReference* groupRef = group->GetFirstMember();
                             groupRef != nullptr;
                             groupRef = groupRef->next())
                        {
                            Player* member = groupRef->GetSource();

                            if (member && m_bot->IsWithinDistInMap(member, MAX_PLAYER_DISTANCE))
                            {
                                bIsMaxRangeExceeded = false;
                                break;
                            }
                        }
                    }
                    else
                    {
                        if (m_bot->IsWithinDistInMap(player, MAX_PLAYER_DISTANCE))
                        {
                            bIsMaxRangeExceeded = false;
                        }
                    }
                }
                else
                {
                    Creature* creature = leader->ToCreature();

                    if (creature)
                    {
                        if (m_bot->IsWithinDistInMap(creature, MAX_PLAYER_DISTANCE))
                        {
                            bIsMaxRangeExceeded = false;
                        }
                    }
                }

                if (bIsMaxRangeExceeded)
                {
                    LOG_DEBUG(
                        "npcbots",
                        "bot [%s] was too far away from leader [%s].",
                        m_bot->GetName().c_str(),
                        leader->GetName().c_str());

                    // teleport bot to player
                    Player* player = leader->ToPlayer();

                    if (player)
                    {
                        float x, y, z, o;
                        player->GetPosition(x, y, z, o);

                        LOG_DEBUG("npcbots", "bot [%s] is teleporting to leader.", m_bot->GetName().c_str());
                        BotMgr::TeleportBot(m_bot, player->GetMap(), x, y, z, o);
                    }
                    else
                    {
                        m_bot->DespawnOrUnsummon();
                    }

                    return;
                }
            }
            else
            {
                m_bot->DespawnOrUnsummon();
            }
        }
        else
        {
            m_uiFollowerTimer -= uiDiff;
        }
    }
    else if (HasBotState(STATE_FOLLOW_COMPLETE))
    {
        LOG_DEBUG("npcbots", "bot [%s] is set completed, despawns.", m_bot->GetName().c_str());

        m_bot->DespawnOrUnsummon();

        return;
    }
}

void BotAI::UpdateBotCombatAI(uint32 /*uiDiff*/)
{
    if (!UpdateVictim())
    {
        return;
    }

    if (!IAmFree() && m_bot->HasReactState(REACT_PASSIVE))
    {
        return;
    }

    DoMeleeAttackIfReady();
}

void BotAI::UpdateAI(uint32 uiDiff)
{
    if (!UpdateCommonBotAI(uiDiff))
    {
        return;
    }

    UpdateBotCombatAI(uiDiff);
}

void BotAI::UpdateBotRations()
{
    bool noFeast = m_bot->IsInCombat() || (m_bot->isMoving()) || m_bot->GetVictim() || CCed(m_bot);

    // check
    if (IAmFree() || (m_owner && !m_owner->IsSitState()))
    {
        if (m_isFeastMana)
        {
            if (noFeast || m_bot->IsStandState() || m_bot->GetMaxPower(POWER_MANA) <= 1 || m_bot->GetPower(POWER_MANA) >= m_bot->GetMaxPower(POWER_MANA))
            {
                std::list<uint32> spellIds;
                Unit::AuraApplicationMap const& aurApps = m_bot->GetAppliedAuras();

                for (Unit::AuraApplicationMap::const_iterator ci = aurApps.begin(); ci != aurApps.end(); ++ci)
                {
                    if (ci->second->GetBase()->GetSpellInfo()->GetSpellSpecific() == SPELL_SPECIFIC_DRINK &&
                        !ci->second->GetBase()->GetSpellInfo()->HasAura(SPELL_AURA_PERIODIC_TRIGGER_SPELL)) //skip buffing food
                    {
                        spellIds.push_back(ci->first);
                    }
                }

                for (std::list<uint32>::const_iterator cit = spellIds.begin(); cit != spellIds.end(); ++cit)
                {
                    m_bot->RemoveAurasDueToSpell(*cit);
                }

                m_isFeastMana = false;
            }
        }

        if (m_isFeastHealth)
        {
            if (noFeast || m_bot->IsStandState() || m_bot->GetHealth() >= m_bot->GetMaxHealth())
            {
                std::list<uint32> spellIds;
                Unit::AuraApplicationMap const& aurApps = m_bot->GetAppliedAuras();

                for (Unit::AuraApplicationMap::const_iterator ci = aurApps.begin(); ci != aurApps.end(); ++ci)
                {
                    if (ci->second->GetBase()->GetSpellInfo()->GetSpellSpecific() == SPELL_SPECIFIC_FOOD &&
                        !ci->second->GetBase()->GetSpellInfo()->HasAura(SPELL_AURA_PERIODIC_TRIGGER_SPELL)) //skip buffing food
                    {
                        spellIds.push_back(ci->first);
                    }
                }

                for (std::list<uint32>::const_iterator cit = spellIds.begin(); cit != spellIds.end(); ++cit)
                {
                    m_bot->RemoveAurasDueToSpell(*cit);
                }

                m_isFeastHealth = false;
            }
        }
    }

    if (noFeast)
    {
        return;
    }

    // drink
    if (!m_isFeastMana &&
        m_bot->GetMaxPower(POWER_MANA) > 1 &&
        !m_bot->HasAuraType(SPELL_AURA_MOUNTED) &&
        !m_bot->isMoving() &&
        CanDrink() &&
        !m_bot->IsInCombat() &&
        !m_bot->GetVehicle() &&
        !IsCasting() &&
        GetManaPCT(m_bot) < 50 &&
        urand(0, 100) < 20)
    {
        m_bot->CastSpell(m_bot, GetRation(true), true);
    }

    // eat
    if (!m_isFeastHealth &&
        !m_bot->HasAuraType(SPELL_AURA_MOUNTED) &&
        !m_bot->isMoving() &&
        CanEat() &&
        !m_bot->IsInCombat() &&
        !m_bot->GetVehicle() &&
        !IsCasting() &&
        GetHealthPCT(m_bot) < 80 &&
        urand(0, 100) < 20)
    {
        m_bot->CastSpell(m_bot, GetRation(false), true);
    }
}

// MOUNT SUPPORT
void BotAI::UpdateMountedState()
{
    if (IAmFree())
    {
        return;
    }

    bool aura = m_bot->HasAuraType(SPELL_AURA_MOUNTED);
    bool mounted = m_bot->IsMounted() && (m_botClass != BOT_CLASS_ARCHMAGE || aura);

    if (!CanMount() && !aura && !mounted)
    {
        return;
    }

    Unit* master = GetBotOwner();

    if (((master && !master->IsMounted()) || aura != mounted) && (aura || mounted))
    {
        LOG_DEBUG(
            "npcbots",
            "bot [%s] dismount.",
            m_bot->GetName().c_str());

        const_cast<CreatureTemplate*>(m_bot->GetCreatureTemplate())->Movement.Flight = CreatureFlightMovementType::None;

        m_bot->SetCanFly(false);
        m_bot->SetDisableGravity(false);
        m_bot->m_movementInfo.RemoveMovementFlag(MOVEMENTFLAG_FLYING);
        m_bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
        m_bot->Dismount();
        m_bot->RemoveUnitMovementFlag(
                    MOVEMENTFLAG_FALLING |
                    MOVEMENTFLAG_FALLING_FAR |
                    MOVEMENTFLAG_PITCH_UP |
                    MOVEMENTFLAG_PITCH_DOWN |
                    MOVEMENTFLAG_SPLINE_ELEVATION |
                    MOVEMENTFLAG_FALLING_SLOW);

        return;
    }

    if (m_bot->IsInCombat() || m_bot->GetVehicle() || m_bot->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING) || IsCasting())
    {
        return;
    }

    if (master &&
        master->IsMounted() &&
        !m_bot->IsMounted() &&
        !master->IsInCombat() &&
        !m_bot->IsInCombat() &&
        !m_bot->GetVictim())
    {
        uint32 mount = 0;
        Unit::AuraEffectList const &mounts = master->GetAuraEffectsByType(SPELL_AURA_MOUNTED);

        if (!mounts.empty())
        {
            // Winter Veil addition
            if (sGameEventMgr->IsActiveEvent(GAME_EVENT_WINTER_VEIL))
            {
                mount = master->CanFly() ? REINDEER_FLY : REINDEER;
            }
            else
            {
                mount = mounts.front()->GetId();
            }
        }

        if (mount)
        {
            if (m_bot->HasAuraType(SPELL_AURA_MOUNTED))
            {
                m_bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
            }

            if (m_botClass == BOT_CLASS_DRUID && m_bot->GetShapeshiftForm() != FORM_NONE)
            {
                RemoveShapeshiftForm();
            }

            LOG_DEBUG(
                "npcbots",
                "bot [%s] before cast mount spell(id: %u)",
                m_bot->GetName().c_str(),
                mount);

            if (DoCastSpell(m_bot, mount)) {

                LOG_DEBUG(
                    "npcbots",
                    "bot [%s] after cast mount spell[id: %u]...OK",
                    m_bot->GetName().c_str(),
                    mount);
            }
            else
            {
                LOG_ERROR(
                    "npcbots",
                    "bot [%s] after cast mount spell[id: %u]...NG",
                    m_bot->GetName().c_str(),
                    mount);
            }
        }
    }
}

void BotAI::UpdateStandState() const
{
    if (IAmFree())
    {
        if (CanSit())
        {
            if (!m_bot->IsInCombat() && !m_bot->isMoving() && m_bot->IsStandState() && Rand() < 15)
            {
                CreatureData const* data = m_bot->GetCreatureData();
                Position pos;
                pos.Relocate(data->posX, data->posY, data->posZ, data->orientation);

                if (m_bot->GetExactDist(&pos) < 5 && m_bot->GetOrientation() == pos.GetOrientation())
                {
                    m_bot->SetStandState(UNIT_STAND_STATE_SIT);
                }
            }
        }
        else if (m_bot->IsSitState() && !(m_bot->GetInterruptMask() & AURA_INTERRUPT_FLAG_NOT_SEATED))
        {
            m_bot->SetStandState(UNIT_STAND_STATE_STAND);
        }

        return;
    }

    if (m_bot->GetVehicle())
    {
        return;
    }

    if ((m_owner->getStandState() == UNIT_STAND_STATE_STAND || !CanSit()) &&
         m_bot->getStandState() == UNIT_STAND_STATE_SIT &&
        !(m_bot->GetInterruptMask() & AURA_INTERRUPT_FLAG_NOT_SEATED))
    {
        m_bot->SetStandState(UNIT_STAND_STATE_STAND);
    }

    if (CanSit() && !m_bot->IsInCombat() && !m_bot->isMoving() &&
        (m_owner->getStandState() == UNIT_STAND_STATE_SIT || (m_bot->GetInterruptMask() & AURA_INTERRUPT_FLAG_NOT_SEATED)) &&
        m_bot->getStandState() == UNIT_STAND_STATE_STAND)
    {
        m_bot->SetStandState(UNIT_STAND_STATE_SIT);
    }
}

bool BotAI::CanEat() const
{
    return m_botClass != BOT_CLASS_SPHYNX;
}

bool BotAI::CanDrink() const
{
    return m_botClass < BOT_CLASS_EX_START;
}

bool BotAI::CanSit() const
{
    return m_botClass < BOT_CLASS_EX_START || m_botClass == BOT_CLASS_DARK_RANGER;
}

bool BotAI::CanMount() const
{
    switch (m_botClass)
    {
        case BOT_CLASS_BM:
        case BOT_CLASS_SPELLBREAKER:
        case BOT_CLASS_DARK_RANGER:
        case BOT_CLASS_NECROMANCER:
        case BOT_CLASS_DREADLORD:           // TODO: [debug only] dreadlord should not mount. delete this!!!
            return true;
        default:
            return m_botClass < BOT_CLASS_EX_START;
    }
}

void BotAI::SpellHit(Unit* caster, SpellInfo const* spell)
{
    if (!spell->IsPositive() &&
        spell->GetMaxDuration() > 1000 &&
        caster->IsControlledByPlayer() &&
        (m_bot->GetCreatureTemplate()->Entry == BOT_DREADLORD || m_bot->GetCreatureTemplate()->Entry == BOT_INFERNAL))
    {
        // bots of W3 classes will not be easily CCed
        if (spell->HasAura(SPELL_AURA_MOD_STUN) || spell->HasAura(SPELL_AURA_MOD_CONFUSE) ||
            spell->HasAura(SPELL_AURA_MOD_PACIFY) || spell->HasAura(SPELL_AURA_MOD_ROOT))
        {
            if (Aura* cont = m_bot->GetAura(spell->Id, caster->GetGUID()))
            {
                if (AuraApplication const* aurApp = cont->GetApplicationOfTarget(m_bot->GetGUID()))
                {
                    if (!aurApp->IsPositive())
                    {
                        int32 dur = std::max<int32>(cont->GetMaxDuration() / 3, 1000);
                        cont->SetDuration(dur);
                        cont->SetMaxDuration(dur);
                    }
                }
            }
        }
    }

    if (spell->GetSpellSpecific() == SPELL_SPECIFIC_DRINK)
    {
        m_isFeastMana = true;
        UpdateMana();
        m_regenTimer = 0;
    }
    else if (spell->GetSpellSpecific() == SPELL_SPECIFIC_FOOD)
    {
        m_isFeastHealth = true;
        m_regenTimer = 0;
    }

    Unit* master = GetBotOwner();

    for (uint8 i = 0; i != MAX_SPELL_EFFECTS; ++i)
    {
        uint32 const auraname = spell->Effects[i].ApplyAuraName;

        if (auraname == SPELL_AURA_MOUNTED)
        {
            UnSummonBotPet();

            if (master->HasAuraType(SPELL_AURA_MOD_INCREASE_VEHICLE_FLIGHT_SPEED) ||
                master->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED))
            {
                CreatureTemplate* creTemplate = const_cast<CreatureTemplate*>(m_bot->GetCreatureTemplate());
                creTemplate->Movement.Flight = CreatureFlightMovementType::CanFly;

                m_bot->SetCanFly(true);
                m_bot->SetDisableGravity(true);

                if (Aura* mount = m_bot->GetAura(spell->Id))
                {
                    for (uint8 j = 0; j != MAX_SPELL_EFFECTS; ++j)
                    {
                        if (spell->Effects[j].ApplyAuraName != SPELL_AURA_MOD_INCREASE_VEHICLE_FLIGHT_SPEED &&
                            spell->Effects[j].ApplyAuraName != SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED &&
                            spell->Effects[j].ApplyAuraName != SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED)
                        {
                            continue;
                        }

                        if (AuraEffect* meff = mount->GetEffect(j))
                        {
                            meff->ChangeAmount(meff->GetAmount() * 3);
                        }
                    }
                }

                m_bot->m_movementInfo.SetMovementFlags(MOVEMENTFLAG_FLYING);
            }
            else
            {
                m_bot->SetSpeedRate(MOVE_RUN, master->GetSpeedRate(MOVE_RUN) * 1.1f);
            }
        }

        // TODO: implement this => SPELL_EFFECTS
    }
}

void BotAI::StartFollow(Unit* leader, uint32 factionForFollower)
{
    if (HasBotState(STATE_FOLLOW_INPROGRESS))
    {
        LOG_WARN("npcbots", "bot [%s] attempt to StartFollow while already following.", m_bot->GetName().c_str());
        return;
    }

    m_uiLeaderGUID = leader->GetGUID();

    if (factionForFollower)
    {
        m_bot->SetFaction(factionForFollower);
    }

    if (!m_bot->GetMotionMaster())
    {
        LOG_ERROR("npcbots", "can not follow player/creature, because MotionMaster is not exist!!!");
        return;
    }

    if (m_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
    {
        m_bot->GetMotionMaster()->Clear();
        m_bot->GetMotionMaster()->MoveIdle();

        LOG_DEBUG("npcbots", "bot [%s] start with WAYPOINT_MOTION_TYPE, set to MoveIdle.", m_bot->GetName().c_str());
    }

    AddBotState(STATE_FOLLOW_INPROGRESS);

    Unit* victim = m_bot->GetVictim();

    if (victim)
    {
        m_bot->GetMotionMaster()->MoveChase(victim);

        LOG_DEBUG(
            "npcbots", "bot [%s] will following player [%s] when enter evade mode.",
            m_bot->GetName().c_str(),
            leader->GetName().c_str());
    }
    else
    {
        m_bot->GetMotionMaster()->MoveFollow(leader, BOT_FOLLOW_DIST, BOT_FOLLOW_ANGLE);

        LOG_DEBUG(
            "npcbots", "bot [%s] start follow [%s].",
            m_bot->GetName().c_str(),
            leader->GetName().c_str());
    }
}

void BotAI::SetFollowComplete()
{
    if (m_bot->HasUnitState(UNIT_STATE_FOLLOW))
    {
        m_bot->ClearUnitState(UNIT_STATE_FOLLOW);

        m_bot->StopMoving();
        m_bot->GetMotionMaster()->Clear();
        m_bot->GetMotionMaster()->MoveIdle();

        if (HasBotState(STATE_FOLLOW_INPROGRESS))
        {
            RemoveBotState(STATE_FOLLOW_INPROGRESS);
            AddBotState(STATE_FOLLOW_COMPLETE);
        }
    }
}

Unit* BotAI::GetLeaderForFollower()
{
    if (Unit* leader = ObjectAccessor::GetUnit(*m_bot, m_uiLeaderGUID))
    {
        if (leader->IsAlive())
        {
            return leader;
        }
        else
        {
            Player *player = leader->ToPlayer();

            if (player)
            {
                if (Group* group = player->GetGroup())
                {
                    for (GroupReference* groupRef = group->GetFirstMember();
                         groupRef != nullptr;
                         groupRef = groupRef->next())
                    {
                        Player* member = groupRef->GetSource();

                        if (member && m_bot->IsWithinDistInMap(member, MAX_PLAYER_DISTANCE) && member->IsAlive())
                        {
                            m_uiLeaderGUID = member->GetGUID();
                            return member;
                        }
                    }
                }
            }
        }
    }

    return nullptr;
}

//This part provides assistance to a player that are attacked by who, even if out of normal aggro range
//It will cause m_bot to attack who that are attacking _any_ player (which has been confirmed may happen also on offi)
//The flag (type_flag) is unconfirmed, but used here for further research and is a good candidate.
bool BotAI::AssistPlayerInCombat(Unit* who)
{
    if (!who || !who->GetVictim())
    {
        return false;
    }

    if (!(m_bot->GetCreatureTemplate()->type_flags & CREATURE_TYPE_FLAG_CAN_ASSIST))
    {
        return false;
    }

    // not a player
    if (!who->GetVictim()->GetCharmerOrOwnerPlayerOrPlayerItself())
    {
        return false;
    }

    // never attack friendly
    if (m_bot->IsFriendlyTo(who))
    {
        return false;
    }

    //too far away and no free sight?
    if (m_bot->IsWithinDistInMap(who, 20.0f) && m_bot->IsWithinLOSInMap(who))
    {
        AttackStart(who);

        return true;
    }

    return false;
}

bool BotAI::IAmFree() const
{
    if (m_owner)
    {
        return false;
    }

    return true;
}

uint16 BotAI::Rand() const
{
    return __rand;
}

void BotAI::GenerateRand() const
{
    int botCount = 0;
    Unit* owner = GetBotOwner();

    if (owner)
    {
        botCount = BotMgr::GetBotsCount(owner);
    }

    __rand = urand(0, IAmFree() ? 100 : 100 + (botCount - 1) * 2);
}

bool BotAI::IsSpellReady(uint32 basespell, uint32 diff, bool checkGCD) const
{
    if (checkGCD && m_gcdTimer > diff)
    {
        return false;
    }

    BotSpellMap::const_iterator itr = m_spells.find(basespell);

    if (itr == m_spells.end())
    {
        return false;
    }

    BotSpell* spell = itr->second;

    return (spell->enabled == true || IAmFree()) && spell->spellId != 0 && spell->cooldown <= diff;
}

Unit* BotAI::FindAOETarget(float dist, uint32 minTargetNum) const
{
    std::list<Unit*> unitList;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck u_check(m_bot, m_bot, dist);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(m_bot, unitList, u_check);
    Cell::VisitAllObjects(m_bot, searcher, dist);

    if (unitList.size() < minTargetNum)
    {
        return nullptr;
    }

    Unit* unit = nullptr;
    float mydist = dist;

    for (std::list<Unit*>::const_iterator itr = unitList.begin(); itr != unitList.end(); ++itr)
    {
        if ((*itr)->isMoving() && (*itr)->GetVictim() &&
            ((*itr)->GetDistance2d((*itr)->GetVictim()->GetPositionX(), (*itr)->GetVictim()->GetPositionY()) > 7.5f ||
            !(*itr)->HasInArc(float(M_PI) * 0.75f, (*itr)->GetVictim())))
        {
            continue;
        }

        if (!unit && (*itr)->GetVictim() && (*itr)->GetDistance((*itr)->GetVictim()) < dist * 0.334f)
        {
            unit = *itr;
            continue;
        }

        if (!unit)
        {
            float destDist = m_bot->GetDistance((*itr)->GetPositionX(), (*itr)->GetPositionY(), (*itr)->GetPositionZ());

            if (destDist < mydist)
            {
                mydist = destDist;
                unit = *itr;
            }
        }

        if (unit)
        {
            uint8 count = 0;

            for (std::list<Unit*>::const_iterator it = unitList.begin(); it != unitList.end(); ++it)
            {
                if (*it != unit && (*it)->GetDistance2d(unit->GetPositionX(), unit->GetPositionY()) < 5.f)
                {
                    if (++count > 2)
                    {
                        if (m_bot->GetDistance(*it) < m_bot->GetDistance(unit) && unit->HasInArc(float(M_PI) / 2, m_bot))
                        {
                            unit = *it;
                        }

                        break;
                    }
                }
            }

            if (count > 2)
            {
                break;
            }

            unit = nullptr;
        }
    }

    return unit;
}

//Finds target for CC spells with MECHANIC_STUN
Unit* BotAI::FindStunTarget(float dist) const
{
    std::list<Unit*> unitList;

    Acore::StunUnitCheck check(m_bot, dist);
    Acore::UnitListSearcher<Acore::StunUnitCheck> searcher(m_bot, unitList, check);
    Cell::VisitAllObjects(m_bot, searcher, dist);

    if (unitList.empty())
    {
        return nullptr;
    }

    if (unitList.size() == 1)
    {
        return *unitList.begin();
    }

    return Acore::Containers::SelectRandomContainerElement(unitList);
}

//Finds casting target (neutral or enemy)
//Can be used to get silence/interruption/reflect/grounding check
Unit* BotAI::FindCastingTarget(float maxdist, float mindist, uint32 spellId, uint8 minHpPct) const
{
    std::list<Unit*> unitList;

    Acore::CastingUnitCheck check(m_bot, mindist, maxdist, spellId, minHpPct);
    Acore::UnitListSearcher<Acore::CastingUnitCheck> searcher(m_bot, unitList, check);
    Cell::VisitAllObjects(m_bot, searcher, maxdist);

    if (unitList.empty())
    {
        return nullptr;
    }

    if (unitList.size() == 1)
    {
        return *unitList.begin();
    }

    return Acore::Containers::SelectRandomContainerElement(unitList);
}

void BotAI::GetNearbyTargetsInConeList(std::list<Unit*> &targets, float maxdist) const
{
    Acore::NearbyHostileUnitInConeCheck check(m_bot, maxdist, this);
    Acore::UnitListSearcher<Acore::NearbyHostileUnitInConeCheck> searcher(m_bot, targets, check);
    Cell::VisitAllObjects(m_bot, searcher, maxdist);
}

uint32 BotAI::GetBotSpellId(uint32 basespell) const
{
    BotSpellMap::const_iterator itr = m_spells.find(basespell);

    return itr != m_spells.end() && 
                    (itr->second->enabled == true || IAmFree()) ? itr->second->spellId : 0;
}

// Using first-rank spell as source, puts spell of max rank allowed for given caster in spellmap
void BotAI::InitSpellMap(uint32 basespell, bool forceadd, bool forwardRank)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(basespell);

    if (!info)
    {
        LOG_ERROR("npcbots", "BotAI::InitSpellMap(): No SpellInfo found for base spell %u", basespell);
        return; //invalid spell id
    }

    uint8 lvl = m_bot->getLevel();
    uint32 spellId = forceadd ? basespell : 0;

    while (info != nullptr && forwardRank && (forceadd || lvl >= info->BaseLevel))
    {
        spellId = info->Id;                 // can use this spell
        info = info->GetNextRankSpell();    // check next rank
    }

    BotSpell* newSpell = m_spells[basespell];

    if (!newSpell)
    {
        newSpell = new BotSpell();
        m_spells[basespell] = newSpell;
    }

    newSpell->spellId = spellId;
}

void BotAI::SetGlobalCooldown(uint32 gcd)
{
    m_gcdTimer = gcd;

    //global cd cannot be less than 1000 ms
    m_gcdTimer = std::max<uint32>(m_gcdTimer, 1000);

    //global cd cannot be greater than 1500 ms
    m_gcdTimer = std::min<uint32>(m_gcdTimer, 1500);
}

void BotAI::SetSpellCooldown(uint32 basespell, uint32 msCooldown)
{
    BotSpellMap::const_iterator itr = m_spells.find(basespell);

    if (itr != m_spells.end())
    {
        itr->second->cooldown = msCooldown;
        return;
    }
    else if (!msCooldown)
    {
        return;
    }

    InitSpellMap(basespell, true, false);
    SetSpellCooldown(basespell, msCooldown);
}

void BotAI::ReduceSpellCooldown(uint32 basespell, uint32 uiDiff)
{
    BotSpellMap::const_iterator itr = m_spells.find(basespell);

    if (itr != m_spells.end())
    {
        if (itr->second->cooldown > uiDiff)
        {
            itr->second->cooldown -= uiDiff;
        }
        else
        {
            itr->second->cooldown = 0;
        }
    }
}

void BotAI::OnBotSpellGo(Spell const* spell, bool ok)
{
    SpellInfo const* curInfo = spell->GetSpellInfo();

    if (ok)
    {
        if (CanBotAttackOnVehicle())
        {
            //Set cooldown
            if (!curInfo->IsCooldownStartedOnEvent() && !curInfo->IsPassive())
            {
                uint32 rec = curInfo->RecoveryTime;
                uint32 catrec = curInfo->CategoryRecoveryTime;

                if (rec > 0)
                {
// TODO: implement this => ApplyBotSpellCooldownMods
//                    ApplyBotSpellCooldownMods(curInfo, rec);
                }

                if (catrec > 0 && !(curInfo->AttributesEx6 & SPELL_ATTR6_NO_CATEGORY_COOLDOWN_MODS))
                {
// TODO: implement this => ApplyBotSpellCategoryCooldownMods
//                    ApplyBotSpellCategoryCooldownMods(curInfo, catrec);
                }

                SetSpellCooldown(curInfo->GetFirstRankSpell()->Id, rec);
//                SetSpellCategoryCooldown(curInfo->GetFirstRankSpell(), catrec);
            }

            if (IsPotionSpell(curInfo->Id))
            {
                StartPotionTimer();
            }

            OnClassSpellGo(curInfo);
        }
    }
}

void BotAI::OnBotOwnerLevelChanged(uint8 newLevel, bool showLevelChange)
{
    CreatureTemplate const* cInfo = m_bot->GetCreatureTemplate();

    m_bot->SetLevel(newLevel, showLevelChange);

    m_classLevelInfo = sObjectMgr->GetCreatureBaseStats(newLevel, cInfo->unit_class);

    // health
    float healthmod = sWorld->getRate(RATE_CREATURE_ELITE_ELITE_HP);

    uint32 basehp = std::max<uint32>(1, m_classLevelInfo->GenerateHealth(cInfo));
    uint32 health = uint32(basehp * healthmod);

    m_bot->SetCreateHealth(health);
    m_bot->SetMaxHealth(health);
    m_bot->SetHealth(health);
    m_bot->ResetPlayerDamageReq();

    // mana
    uint32 mana = m_classLevelInfo->GenerateMana(cInfo);

    m_bot->SetCreateMana(mana);
    m_bot->SetMaxPower(POWER_MANA, mana);
    m_bot->SetPower(POWER_MANA, mana);

    m_bot->SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, (float)health);
    m_bot->SetModifierValue(UNIT_MOD_MANA, BASE_VALUE, (float)mana);

    // damage
    float baseDamage = m_classLevelInfo->GenerateBaseDamage(cInfo);

    float weaponBaseMinDamage = baseDamage;
    float weaponBaseMaxDamage = baseDamage * 1.5;

    m_bot->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    m_bot->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);

    m_bot->SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    m_bot->SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);

    m_bot->SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    m_bot->SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);

    m_bot->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, m_classLevelInfo->AttackPower);
    m_bot->SetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, m_classLevelInfo->RangedAttackPower);
}

bool BotAI::CanBotAttackOnVehicle() const
{
    if (VehicleSeatEntry const* seat = m_bot->GetVehicle() ? m_bot->GetVehicle()->GetSeatForPassenger(m_bot) : nullptr)
    {
        return seat->m_flags & VEHICLE_SEAT_FLAG_CAN_ATTACK;
    }

    return true;
}

void BotAI::Regenerate()
{
    m_regenTimer += m_lastUpdateDiff;

    // every tick
    if (m_bot->getPowerType() == POWER_ENERGY)
    {
        RegenerateEnergy();
    }

    if (m_regenTimer >= REGEN_CD)
    {
        m_regenTimer -= REGEN_CD;

        // Regen Health
        int32 baseRegen = int32(GetTotalBotStat(BOT_STAT_MOD_HEALTH_REGEN));;

        if (!m_bot->IsInCombat() ||
            m_bot->IsPolymorphed() ||
            baseRegen > 0 ||
            m_bot->HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT) ||
            m_bot->HasAuraType(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT))
        {
            if (m_bot->GetHealth() < m_bot->GetMaxHealth())
            {
                int32 add = m_bot->IsInCombat() ? 0 : IAmFree() && !m_bot->GetVictim() ? m_bot->GetMaxHealth() / 32 : 5 + m_bot->GetCreateHealth() / 256;

                if (baseRegen > 0)
                {
                    add += std::max<int32>(baseRegen / 5, 1);
                }

                if (m_bot->IsPolymorphed())
                {
                    add += m_bot->GetMaxHealth() / 6;
                }
                else if (!m_bot->IsInCombat() || m_bot->HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
                {
                    if (!m_bot->IsInCombat())
                    {
                        Unit::AuraEffectList const& mModHealthRegenPct = m_bot->GetAuraEffectsByType(SPELL_AURA_MOD_HEALTH_REGEN_PERCENT);

                        for (Unit::AuraEffectList::const_iterator i = mModHealthRegenPct.begin();
                             i != mModHealthRegenPct.end();
                             ++i)
                        {
                            AddPct(add, (*i)->GetAmount());
                        }

                        add += m_bot->GetTotalAuraModifier(SPELL_AURA_MOD_REGEN) * REGEN_CD / 5000;
                    }
                    else if (m_bot->HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
                    {
                        ApplyPct(add, m_bot->GetTotalAuraModifier(SPELL_AURA_MOD_REGEN_DURING_COMBAT));
                    }
                }

                add += m_bot->GetTotalAuraModifier(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT);

                if (add < 0)
                {
                    add = 0;
                }

                m_bot->ModifyHealth(add);
            }
        }

        // Regen Mana
        if (m_bot->GetMaxPower(POWER_MANA) > 1 &&
            m_bot->GetPower(POWER_MANA) < m_bot->GetMaxPower(POWER_MANA))
        {
            float addvalue;

            if (m_bot->IsUnderLastManaUseEffect())
            {
                addvalue = m_bot->GetFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER);
            }
            else
            {
                addvalue = m_bot->GetFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER);
            }

            addvalue *= sWorld->getRate(RATE_POWER_MANA) * REGEN_CD * 0.001f; //regenTimer threshold / 1000

            if (addvalue < 0.0f)
            {
                addvalue = 0.0f;
            }

            m_bot->ModifyPower(POWER_MANA, int32(addvalue));
        }
    }
}

void BotAI::RegenerateEnergy()
{
    uint32 curValue = m_bot->GetPower(POWER_ENERGY);
    uint32 maxValue = m_bot->GetMaxPower(POWER_ENERGY);

    if (curValue < maxValue)
    {
        float addvalue = 0.01f * m_lastUpdateDiff * sWorld->getRate(RATE_POWER_ENERGY); //10 per sec
        Unit::AuraEffectList const& ModPowerRegenPCTAuras = m_bot->GetAuraEffectsByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);

        for (Unit::AuraEffectList::const_iterator i = ModPowerRegenPCTAuras.begin();
             i != ModPowerRegenPCTAuras.end();
             ++i)
        {
            if (Powers((*i)->GetMiscValue()) == POWER_ENERGY)
            {
                AddPct(addvalue, (*i)->GetAmount());
            }
        }

        addvalue += m_energyFraction;

        if (addvalue == 0x0) //only if world rate for enegy is 0
        {
            return;
        }

        uint32 integerValue = uint32(fabs(addvalue));

        curValue += integerValue;

        if (curValue > maxValue)
        {
            curValue = maxValue;
            m_energyFraction = 0.f;
        }
        else
        {
            m_energyFraction = addvalue - float(integerValue);
        }

        if (curValue == maxValue || m_regenTimer >= REGEN_CD)
        {
            m_bot->SetPower(POWER_ENERGY, curValue);
        }
        else
        {
            m_bot->UpdateUInt32Value(UNIT_FIELD_POWER1 + POWER_ENERGY, curValue);
        }
    }
}

bool BotAI::CCed(Unit const* target, bool root)
{
    return target ? target->HasUnitState(UNIT_STATE_CONFUSED | UNIT_STATE_STUNNED | UNIT_STATE_FLEEING | UNIT_STATE_DISTRACTED | UNIT_STATE_CONFUSED_MOVE | UNIT_STATE_FLEEING_MOVE) || (root && (target->HasUnitState(UNIT_STATE_ROOT) || target->isFrozen() || target->isInRoots())) : true;
}

void BotAI::DrinkPotion(bool mana)
{
    if (IsCasting())
    {
        return;
    }

    m_bot->CastSpell(m_bot, GetPotion(mana));
}

bool BotAI::IsPotionReady() const
{
    return m_potionTimer <= m_lastUpdateDiff;
}

void BotAI::StartPotionTimer()
{
    m_potionTimer = POTION_CD;
}

uint32 BotAI::GetPotion(bool mana) const
{
    for (int8 i = MAX_POTION_SPELLS - 1; i >= 0; --i)
    {
        if (m_bot->getLevel() >= (mana ? ManaPotionSpells[i][0] : HealingPotionSpells[i][0]))
        {
            return (mana ? ManaPotionSpells[i][1] : HealingPotionSpells[i][1]);
        }
    }

    return (mana ? ManaPotionSpells[0][1] : HealingPotionSpells[0][1]);
}

bool BotAI::IsPotionSpell(uint32 spellId) const
{
    return spellId == GetPotion(true) || spellId == GetPotion(false);
}

uint32 BotAI::GetRation(bool drink) const
{
    for (int8 i = MAX_FEAST_SPELLS - 1; i >= 0; --i)
    {
        if (m_bot->getLevel() >= (drink ? DrinkSpells[i][0] : EatSpells[i][0]))
        {
            return (drink ? DrinkSpells[i][1] : EatSpells[i][1]);
        }
    }

    return (drink ? DrinkSpells[0][1] : EatSpells[0][1]);
}

void BotAI::KillEvents(bool force)
{
    Events.KillAllEvents(force);
}

void BotAI::BotStopMovement()
{
    if (m_bot->IsInWorld())
    {
        m_bot->GetMotionMaster()->Clear();
        m_bot->GetMotionMaster()->MoveIdle();
    }

    m_bot->StopMoving();
    m_bot->DisableSpline();
}

// Movement set
// Uses MovePoint() for following instead of MoveFollow()
// This helps bots overcome a bug with fanthom walls on grid borders blocking pathing
void BotAI::BotMovement(BotMovementType type, Position const* pos, Unit* target, bool generatePath) const
{
    Vehicle* veh = m_bot->GetVehicle();
    VehicleSeatEntry const* seat = veh ? veh->GetSeatForPassenger(m_bot) : nullptr;
    bool canControl = seat ? (seat->m_flags & VEHICLE_SEAT_FLAG_CAN_CONTROL) : false;
    Unit* mover = canControl ? veh->GetBase() : !veh ? m_bot : nullptr;

    if (!mover)
    {
        return;
    }

    switch (type)
    {
        case BOT_MOVE_CHASE:
            mover->GetMotionMaster()->MoveChase(target);
            break;
        case BOT_MOVE_POINT:
            mover->GetMotionMaster()->MovePoint(mover->GetMapId(), *pos, generatePath);
            break;
        default:
            return;
    }
}

void BotAI::OnBotOwnerMoveWorldport(Player* owner)
{
    Map* botCurMap = m_bot->FindMap();

    if ((m_bot->IsInWorld() || (botCurMap && botCurMap->IsDungeon())) &&
        m_bot->IsAlive() &&
        HasBotState(STATE_FOLLOW_INPROGRESS))
    {
        // teleport bot to player
        BotMgr::TeleportBot(
                    m_bot,
                    owner->GetMap(),
                    owner->m_positionX,
                    owner->m_positionY,
                    owner->m_positionZ,
                    owner->m_orientation);
    }
    else
    {
        uint32 areaId, zoneId;
        std::string zoneName = "unknown", areaName = "unknown";
        LocaleConstant locale = sWorld->GetDefaultDbcLocale();

        owner->GetMap()->GetZoneAndAreaId(
                              owner->GetPhaseMask(),
                              zoneId,
                              areaId,
                              owner->GetPositionX(),
                              owner->GetPositionY(),
                              owner->GetPositionZ());

        if (AreaTableEntry const* zone = sAreaTableStore.LookupEntry(zoneId))
        {
            zoneName = zone->area_name[locale];
        }

        if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId))
        {
            areaName = area->area_name[locale];
        }

        LOG_DEBUG(
            "npcbots",
            "bot [Name: %s, IsInWorld: %s, Map: %s, IsDungeon: %s, IsAlive: %s, IsFollowed: %s] cannot worldport to player [%s] in [%s, %s, %s].",
            m_bot->GetName().c_str(),
            m_bot->IsInWorld() ? "true" : "false",
            botCurMap ? botCurMap->GetMapName() : "unknown",
            botCurMap && botCurMap->IsDungeon() ? "true" : "false",
            m_bot->IsAlive() ? "true" : "false",
            HasBotState(STATE_FOLLOW_INPROGRESS) ? "true" : "false",
            owner->GetName().c_str(),
            areaName.c_str(),
            zoneName.c_str(),
            owner->GetMap()->GetMapName());
    }
}

bool BotAI::BotFinishTeleport()
{
    LOG_DEBUG("npcbots", "bot [%s] finishing teleport...", m_bot->GetName().c_str());

//    if (m_bot->IsAlive())
//    {
//        m_bot->CastSpell(m_bot, COSMETIC_TELEPORT_EFFECT, true);
//    }

    Unit* owner = GetBotOwner();

    if (owner)
    {
        // update group member online state
        if (Player const* player = owner->ToPlayer())
        {
            if (Group* gr = const_cast<Group*>(player->GetGroup()))
            {
                if (gr->IsMember(m_bot->GetGUID()))
                {
                    gr->SendUpdate();
                }
            }
        }
    }

    Unit* leader = GetLeaderForFollower();

    if (leader && leader->IsAlive())
    {
        if (HasBotState(STATE_FOLLOW_INPROGRESS))
        {
            RemoveBotState(STATE_FOLLOW_INPROGRESS);
        }

        StartFollow(leader, owner ? owner->GetFaction() : 0);
    }

    return true;
}

bool BotAI::IsHeroExClass(uint8 botClass)
{
    return botClass == BOT_CLASS_BM ||
            botClass == BOT_CLASS_ARCHMAGE ||
            botClass == BOT_CLASS_DREADLORD ||
            botClass == BOT_CLASS_DARK_RANGER;
}

bool BotAI::JumpingOrFalling() const
{
    return Jumping() ||
            m_bot->IsFalling() ||
            m_bot->HasUnitMovementFlag(MOVEMENTFLAG_PITCH_UP | MOVEMENTFLAG_PITCH_DOWN | MOVEMENTFLAG_FALLING_SLOW);
}

bool BotAI::Jumping() const
{
    return m_bot->HasUnitState(UNIT_STATE_JUMPING);
}

bool BotAI::DoCastSpell(Unit* victim, uint32 spellId, bool triggered)
{
    return DoCastSpell(victim, spellId, triggered ? TRIGGERED_FULL_MASK : TRIGGERED_NONE);
}

bool BotAI::DoCastSpell(Unit* victim, uint32 spellId, TriggerCastFlags flags)
{
    if (spellId == 0)
    {
        return false;
    }

    if (!victim || !victim->IsInWorld() || m_bot->GetMap() != victim->FindMap())
    {
        return false;
    }

    if (IsCasting())
    {
        return false;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);

    if (!spellInfo)
    {
        return false;
    }

    // select aura level
    if (victim->isType(TYPEMASK_UNIT))
    {
        if (SpellInfo const* actualSpellInfo = spellInfo->GetAuraRankForLevel(victim->getLevel()))
        {
            spellInfo = actualSpellInfo;
        }

        if (!spellInfo->IsTargetingArea())
        {
            // check if target already has the same type, but more powerful aura
            if (spellInfo->IsStrongerAuraActive(m_bot, victim))
            {
                return false;
            }
        }

        if ((flags & TRIGGERED_FULL_MASK) != TRIGGERED_FULL_MASK &&
            !(spellInfo->AttributesEx2 & SPELL_ATTR2_IGNORE_LINE_OF_SIGHT) &&
            !m_bot->IsWithinLOSInMap(victim))
        {
            return false;
        }
    }

    // spells with cast time
    if (m_bot->isMoving() &&
        ((spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT) || spellId == SHOOT_WAND || spellId == 48155) &&
        !(spellInfo->Attributes & SPELL_ATTR0_ON_NEXT_SWING) &&
        !spellInfo->IsAutoRepeatRangedSpell() &&
        !(flags & TRIGGERED_CAST_DIRECTLY) &&
        (spellInfo->IsChanneled() || spellInfo->CalcCastTime()))
    {
        if (JumpingOrFalling())
        {
            return false;
        }

        if (!m_bot->GetVictim() && m_bot->IsInWorld() && (m_bot->GetMap()->IsRaid() || m_bot->GetMap()->IsHeroic()))
        {
            return false;
        }

        if (!spellInfo->HasEffect(SPELL_EFFECT_HEAL) && Rand() > (IAmFree() ? 80 : 50))
        {
            return false;
        }

        LOG_WARN("npcbots", "bot [%s] stop movement. (1)", m_bot->GetName().c_str());
        BotStopMovement();
    }

    if ((!victim->isType(TYPEMASK_UNIT)) && !victim->IsWithinLOSInMap(m_bot))
    {
        if (!IAmFree())
        {
            if (m_bot->GetDistance(victim) > 10.f)
            {
                Position pos = victim->GetPosition();
                BotMovement(BOT_MOVE_POINT, &pos);
            }
            else
            {
                m_bot->Relocate(victim);
            }
        }
        else
        {
            return false;
        }
    }

    // remove shapeshifts manually to restore powers/stats
    if (m_bot->GetShapeshiftForm() != FORM_NONE)
    {
        if (spellInfo->CheckShapeshift(m_bot->GetShapeshiftForm()) != SPELL_CAST_OK)
        {
            if (!RemoveShapeshiftForm())
            {
                return false;
            }
        }
    }

    // CHECKS PASSED, NOW DO IT

    if (m_bot->getStandState() == UNIT_STAND_STATE_SIT &&
        !(spellInfo->Attributes & SPELL_ATTR0_ALLOW_WHILE_SITTING))
    {
        if (!m_isDoUpdateMana && (m_bot->GetInterruptMask() & AURA_INTERRUPT_FLAG_NOT_SEATED))
        {
            UpdateMana();
        }

        m_isFeastHealth = false;
        m_isFeastMana = false;
        m_bot->SetStandState(UNIT_STAND_STATE_STAND);
    }

    bool triggered = (flags & TRIGGERED_CAST_DIRECTLY);
    SpellCastTargets targets;
    targets.SetUnitTarget(victim);
    Spell* spell = new Spell(m_bot, spellInfo, flags);
    spell->prepare(&targets); // sets current spell if succeed
    bool casted = triggered; // triggered casts are casted immediately

    for (uint8 i = 0; i != CURRENT_MAX_SPELL; ++i)
    {
        if (m_bot->GetCurrentSpell(i) == spell)
        {
            casted = true;
            break;
        }
    }

    if (triggered)
    {
        return true;
    }

    if (spellInfo->IsPassive() || spellInfo->IsCooldownStartedOnEvent())
    {
        return true;
    }

    if (!spellInfo->StartRecoveryCategory || !spellInfo->StartRecoveryTime)
    {
        return true;
    }

    float gcd = float(spellInfo->StartRecoveryTime);

// TODO: implement this => ApplyBotSpellGlobalCooldownMods
//    ApplyBotSpellGlobalCooldownMods(spellInfo, gcd);

    // Apply haste to cooldown
    if (m_haste &&
        spellInfo->StartRecoveryCategory == 133 &&
        spellInfo->StartRecoveryTime == 1500 &&
        spellInfo->DmgClass != SPELL_DAMAGE_CLASS_MELEE &&
        spellInfo->DmgClass != SPELL_DAMAGE_CLASS_RANGED &&
        !(spellInfo->Attributes & (SPELL_ATTR0_USES_RANGED_SLOT | SPELL_ATTR0_IS_ABILITY)))
    {
// TODO: implement this => ApplyBotPercentModFloatVar
//        ApplyBotPercentModFloatVar(gcd, float(m_hastehaste), false);
    }

    // if cast time is lower than 1.5 sec it also reduces gcd but only if not instant
    if (spellInfo->CastTimeEntry)
    {
        if (int32 castTime = spellInfo->CastTimeEntry->CastTime)
        {
            if (castTime > 0)
            {
// TODO: implement this => ApplyClassSpellCastTimeMods
//                ApplyClassSpellCastTimeMods(spellInfo, castTime);

                if (castTime < gcd)
                {
                    gcd = float(castTime);
                }
            }
        }
    }

    m_gcdTimer = uint32(gcd);

    //global cd cannot be less than 1000 ms
    m_gcdTimer = std::max<uint32>(m_gcdTimer, 1000);

    //global cd cannot be greater than 1500 ms
    m_gcdTimer = std::min<uint32>(m_gcdTimer, 1500);

    return true;
}
