/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BotAI.h"
#include "BotCommon.h"
#include "BotMgr.h"
#include "CellImpl.h"
#include "Creature.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Log.h"
#include "SpellAuraEffects.h"
#include "Vehicle.h"

const float MAX_PLAYER_DISTANCE = 100.0f;

enum ePoints
{
    POINT_COMBAT_START  = 0xFFFFFF
};

static uint16 __rand; //calculated for each bot separately once every updateAI tick

BotAI::BotAI(Creature* creature) : ScriptedAI(creature)
{
    m_uiFollowerTimer = 2500;
    m_uiWaitTimer = 0;
    m_uiUpdateTimerMedium = 0;
    m_regenTimer = 0;
    m_energyFraction = 0.f;

    m_uiBotState = STATE_FOLLOW_NONE;

    m_master = nullptr;
    m_hasBotPet = false;
}

BotAI::~BotAI()
{
    while (!m_spells.empty())
    {
        BotSpellMap::iterator itr = m_spells.begin();
        delete itr->second;
        m_spells.erase(itr);
    }
}

void BotAI::MovementInform(uint32 motionType, uint32 pointId)
{
    if (motionType != POINT_MOTION_TYPE || !HasBotState(STATE_FOLLOW_INPROGRESS))
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
            me->DespawnOrUnsummon();
        }
    }
}

void BotAI::AttackStart(Unit* who)
{
    if (!who)
    {
        return;
    }

    if (me->Attack(who, true))
    {
        if (me->HasUnitState(UNIT_STATE_FOLLOW))
        {
            me->ClearUnitState(UNIT_STATE_FOLLOW);
        }

        if (IsCombatMovementAllowed())
        {
            me->GetMotionMaster()->MoveChase(who);
        }
    }
}

void BotAI::MoveInLineOfSight(Unit* who)
{
    if (me->GetVictim())
    {
        return;
    }

    if (!me->HasUnitState(UNIT_STATE_STUNNED) && 
        who->isTargetableForAttack(true, me) && 
        who->isInAccessiblePlaceFor(me))
    {
        if (AssistPlayerInCombat(who)) 
        {
            return;
        }
    }

    if (me->CanStartAttack(who))
    {
        if (me->HasUnitState(UNIT_STATE_DISTRACTED))
        {
            me->ClearUnitState(UNIT_STATE_DISTRACTED);
            me->GetMotionMaster()->Clear();
        }

        AttackStart(who);
    }
}

void BotAI::EnterEvadeMode()
{
    me->RemoveAllAuras();
    me->DeleteThreatList();
    me->CombatStop(true);
    me->SetLootRecipient(nullptr);

    if (HasBotState(STATE_FOLLOW_INPROGRESS))
    {
        LOG_DEBUG("npcbots", "BotAI left combat, returning to CombatStartPosition.");

        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
        {
            float fPosX, fPosY, fPosZ;
            me->GetPosition(fPosX, fPosY, fPosZ);
            me->GetMotionMaster()->MovePoint(POINT_COMBAT_START, fPosX, fPosY, fPosZ);
        }
    }
    else
    {
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
        {
            me->GetMotionMaster()->MoveTargetedHome();
        }
    }

    Reset();
}

void BotAI::JustDied(Unit* pKiller)
{
    if (!HasBotState(STATE_FOLLOW_INPROGRESS) || !m_uiLeaderGUID)
    {
        return;
    }

    // TODO: npcbots JustDied
}

void BotAI::JustRespawned()
{
    m_uiBotState = STATE_FOLLOW_NONE;

    if (!IsCombatMovementAllowed())
    {
        SetCombatMovement(true);
    }

    if (me->GetFaction() != me->GetCreatureTemplate()->faction)
    {
        me->SetFaction(me->GetCreatureTemplate()->faction);
    }

    Reset();
}

void BotAI::OnCreatureFinishedUpdate(uint32 uiDiff)
{
    if (m_uiWaitTimer > uiDiff)
    {
        m_uiWaitTimer -= uiDiff;
    }

    if (m_uiUpdateTimerMedium > uiDiff)
    {
        m_uiUpdateTimerMedium -= uiDiff;
    }
}

bool BotAI::UpdateCommonBotAI(uint32 uiDiff)
{
    ReduceCooldown(uiDiff);

    m_lastUpdateDiff = uiDiff;

    UpdateFollowerAI(uiDiff);

    if (m_uiUpdateTimerMedium <= uiDiff)
    {
        m_uiUpdateTimerMedium = 500;

        if (me->IsInWorld())
        {
            if (m_master)
            {
                if (Group const* gr = m_master->GetGroup())
                {
                    if (gr->IsMember(me->GetGUID()))
                    {
                        WorldPacket data;
                        BuildGrouUpdatePacket(&data);

                        for (GroupReference const* itr = gr->GetFirstMember(); 
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

    if (!me->IsAlive())
    {
        return false;
    }

    Regenerate();

    if (DelayUpdateIfNeeded())
    {
        return false;
    }

    GenerateRand();

    return true;
}

bool BotAI::DelayUpdateIfNeeded()
{
    if (m_uiWaitTimer > m_lastUpdateDiff || (m_master && !m_master->IsInWorld()))
    {
        return true;
    }

    if (IAmFree())
    {
        m_uiWaitTimer = me->IsInCombat() ? 500 : urand(750, 1250);
    }
    else if ((m_master && !m_master->GetMap()->IsRaid()))
    {
        m_uiWaitTimer = std::min<uint32>(uint32(50 * (BotMgr::GetPlayerBotsCount(m_master) - 1) + __rand + __rand), 500);
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
    *data << me->GetGUID().WriteAsPacked();
    *data << uint32(mask);

    if (mask & GROUP_UPDATE_FLAG_STATUS)
    {
        uint16 playerStatus = MEMBER_STATUS_ONLINE;

        if (me->IsPvP())
        {
            playerStatus |= MEMBER_STATUS_PVP;
        }

        if (!me->IsAlive())
        {
            playerStatus |= MEMBER_STATUS_DEAD;
        }

        if (me->HasByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP))
        {
            playerStatus |= MEMBER_STATUS_PVP_FFA;
        }

        *data << uint16(playerStatus);
    }

    if (mask & GROUP_UPDATE_FLAG_CUR_HP)
    {
        *data << uint32(me->GetHealth());
    }

    if (mask & GROUP_UPDATE_FLAG_MAX_HP)
    {
        *data << uint32(me->GetMaxHealth());
    }

    Powers powerType = me->getPowerType();

    if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)
    {
        *data << uint8(powerType);
    }

    if (mask & GROUP_UPDATE_FLAG_CUR_POWER)
    {
        *data << uint16(me->GetPower(powerType));
    }

    if (mask & GROUP_UPDATE_FLAG_MAX_POWER)
    {
        *data << uint16(me->GetMaxPower(powerType));
    }

    if (mask & GROUP_UPDATE_FLAG_LEVEL)
    {
        *data << uint16(me->getLevel());
    }

    if (mask & GROUP_UPDATE_FLAG_ZONE)
    {
        *data << uint16(me->GetZoneId());
    }

    if (mask & GROUP_UPDATE_FLAG_POSITION)
    {
        *data << uint16(me->GetPositionX());
        *data << uint16(me->GetPositionY());
    }

    if (mask & GROUP_UPDATE_FLAG_VEHICLE_SEAT)
    {
        if (Vehicle* veh = me->GetVehicle())
        {
            *data << uint32(veh->GetVehicleInfo()->m_seatID[me->m_movementInfo.transport.seat]);
        }
        else
        {
            *data << uint32(0);
        }
    }
}

void BotAI::UpdateFollowerAI(uint32 uiDiff)
{
    Unit* victim = me->GetVictim();

    if (HasBotState(STATE_FOLLOW_INPROGRESS) && !victim)
    {
        if (m_uiFollowerTimer <= uiDiff)
        {
            m_uiFollowerTimer = 1000;

            if (HasBotState(STATE_FOLLOW_COMPLETE) && !HasBotState(STATE_FOLLOW_POSTEVENT))
            {
                LOG_DEBUG("npcbots", "BotAI is set completed, despawns.");
                me->DespawnOrUnsummon();

                return;
            }

            bool bIsMaxRangeExceeded = true;

            if (Player* player = GetLeaderForFollower())
            {
                if (HasBotState(STATE_FOLLOW_RETURNING))
                {
                    LOG_DEBUG("npcbots", "BotAI is returning to leader.");

                    RemoveBotState(STATE_FOLLOW_RETURNING);
                    me->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);

                    return;
                }

                if (Group* group = player->GetGroup())
                {
                    for (GroupReference* groupRef = group->GetFirstMember();
                            groupRef != nullptr;
                            groupRef = groupRef->next())
                    {
                        Player* member = groupRef->GetSource();

                        if (member && me->IsWithinDistInMap(member, MAX_PLAYER_DISTANCE))
                        {
                            bIsMaxRangeExceeded = false;
                            break;
                        }
                    }
                }
                else
                {
                    if (me->IsWithinDistInMap(player, MAX_PLAYER_DISTANCE))
                    {
                        bIsMaxRangeExceeded = false;
                    }
                }
            }

            if (bIsMaxRangeExceeded)
            {
                LOG_DEBUG("npcbots", "BotAI failed because player/group was too far away or not found.");
                me->DespawnOrUnsummon();

                return;
            }
        }
        else
        {
            m_uiFollowerTimer -= uiDiff;
        }
    }
}

void BotAI::UpdateBotAI(uint32 uiDiff)
{
    if (!UpdateVictim())
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

    UpdateBotAI(uiDiff);
}

void BotAI::StartFollow(Player* player, uint32 factionForFollower)
{
    if (me->GetVictim())
    {
        LOG_DEBUG("npcbots", "BotAI attempt to StartFollow while in combat.");
        return;
    }

    if (HasBotState(STATE_FOLLOW_INPROGRESS))
    {
        LOG_ERROR("npcbots", "BotAI attempt to StartFollow while already following.");
        return;
    }

    //set variables
    m_uiLeaderGUID = player->GetGUID();

    if (factionForFollower)
    {
        me->SetFaction(factionForFollower);
    }

    if (!me->GetMotionMaster())
    {
        LOG_ERROR("npcbots", "can not follow player, because MotionMaster is not exist!!!");
        return;
    }

    if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
    {
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveIdle();

        LOG_DEBUG("npcbots", "BotAI start with WAYPOINT_MOTION_TYPE, set to MoveIdle.");
    }

    me->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

    AddBotState(STATE_FOLLOW_INPROGRESS);

    me->GetMotionMaster()->MoveFollow(player, BOT_FOLLOW_DIST, BOT_FOLLOW_ANGLE);

    LOG_DEBUG(
        "npcbots", "BotAI start follow %s (%s)", 
        player->GetName().c_str(), 
        m_uiLeaderGUID.ToString().c_str());
}

void BotAI::SetFollowPaused(bool paused)
{
    if (!HasBotState(STATE_FOLLOW_INPROGRESS) || HasBotState(STATE_FOLLOW_COMPLETE))
    {
        return;
    }

    if (paused)
    {
        AddBotState(STATE_FOLLOW_PAUSED);

        if (me->HasUnitState(UNIT_STATE_FOLLOW))
        {
            me->ClearUnitState(UNIT_STATE_FOLLOW);

            me->StopMoving();
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveIdle();
        }
    }
    else
    {
        RemoveBotState(STATE_FOLLOW_PAUSED);

        if (Player* leader = GetLeaderForFollower())
        {
            me->GetMotionMaster()->MoveFollow(leader, BOT_FOLLOW_DIST, BOT_FOLLOW_ANGLE);
        }
    }
}

void BotAI::SetFollowComplete()
{
    if (me->HasUnitState(UNIT_STATE_FOLLOW))
    {
        me->ClearUnitState(UNIT_STATE_FOLLOW);

        me->StopMoving();
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveIdle();
    }

    if (HasBotState(STATE_FOLLOW_POSTEVENT))
    {
        RemoveBotState(STATE_FOLLOW_POSTEVENT);
    }

    AddBotState(STATE_FOLLOW_COMPLETE);
}

Player* BotAI::GetLeaderForFollower()
{
    if (Player* player = ObjectAccessor::GetPlayer(*me, m_uiLeaderGUID))
    {
        if (player->IsAlive())
        {
            return player;
        }
        else
        {
            if (Group* group = player->GetGroup())
            {
                for (GroupReference* groupRef = group->GetFirstMember(); groupRef != nullptr; groupRef = groupRef->next())
                {
                    Player* member = groupRef->GetSource();

                    if (member && me->IsWithinDistInMap(member, MAX_PLAYER_DISTANCE) && member->IsAlive())
                    {
                        m_uiLeaderGUID = member->GetGUID();
                        return member;
                    }
                }
            }
        }
    }

    return nullptr;
}

//This part provides assistance to a player that are attacked by who, even if out of normal aggro range
//It will cause me to attack who that are attacking _any_ player (which has been confirmed may happen also on offi)
//The flag (type_flag) is unconfirmed, but used here for further research and is a good candidate.
bool BotAI::AssistPlayerInCombat(Unit* who)
{
    if (!who || !who->GetVictim())
    {
        return false;
    }

    //experimental (unknown) flag not present
    if (!(me->GetCreatureTemplate()->type_flags & CREATURE_TYPE_FLAG_CAN_ASSIST))
    {
        return false;
    }

    //not a player
    if (!who->GetVictim()->GetCharmerOrOwnerPlayerOrPlayerItself())
    {
        return false;
    }

    //never attack friendly
    if (me->IsFriendlyTo(who))
    {
        return false;
    }

    //too far away and no free sight?
    if (me->IsWithinDistInMap(who, MAX_PLAYER_DISTANCE) && me->IsWithinLOSInMap(who))
    {
        AttackStart(who);

        return true;
    }

    return false;
}

bool BotAI::IAmFree() const
{
    if (m_master)
    {
        return true;
    }

    return false;
}

uint16 BotAI::Rand() const
{
    return __rand;
}

void BotAI::GenerateRand() const
{
    int botCount = 0;

    if (m_master) 
    {
        botCount = BotMgr::GetPlayerBotsCount(m_master);
    }

    __rand = urand(0, IAmFree() ? 100 : 100 + (botCount - 1) * 2);
}

bool BotAI::IsSpellReady(uint32 basespell, uint32 diff) const
{
    BotSpellMap::const_iterator itr = m_spells.find(basespell);

    if (itr == m_spells.end())
    {
        return false;
    }

    if (!itr->second->enabled && !IAmFree())
    {
        return false;
    }

    if (itr->second->spellId == 0)
    {
        return false;
    }
    
    if (itr->second->cooldown > diff)
    {
        return false;
    }

    return true;
}

float BotAI::CalcSpellMaxRange(uint32 spellId, bool enemy) const
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);

    float maxRange = spellInfo->GetMaxRange(!enemy);

    return maxRange;
}

uint32 BotAI::GetSpell(uint32 basespell) const
{
    BotSpellMap::const_iterator itr = m_spells.find(basespell);

    return itr != m_spells.end() && 
                    (itr->second->enabled == true || IAmFree()) ? itr->second->spellId : 0;
}

Unit* BotAI::FindAOETarget(float dist) const
{
    Unit* unit = me->SelectNearbyTarget(nullptr, dist);

    return unit;
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

    uint8 lvl = me->getLevel();
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

void BotAI::OnBotSpellGo(Spell const* spell, bool ok)
{
    SpellInfo const* curInfo = spell->GetSpellInfo();

    if (ok)
    {
        if (CanBotAttackOnVehicle())
        {
            OnClassSpellGo(curInfo);
        }
    }
}

bool BotAI::CanBotAttackOnVehicle() const
{
    if (VehicleSeatEntry const* seat = me->GetVehicle() ? me->GetVehicle()->GetSeatForPassenger(me) : nullptr)
    {
        return seat->m_flags & VEHICLE_SEAT_FLAG_CAN_ATTACK;
    }

    return true;
}

void BotAI::Regenerate()
{
    m_regenTimer += m_lastUpdateDiff;

    // every tick
    if (me->getPowerType() == POWER_ENERGY)
    {
        RegenerateEnergy();
    }

    if (m_regenTimer >= REGEN_CD)
    {
        m_regenTimer -= REGEN_CD;

        // Regen Health
        int32 baseRegen = 0;    // TODO: calc the item bonus as base regen.

        if (!me->IsInCombat() ||
            me->IsPolymorphed() ||
            baseRegen > 0 ||
            me->HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT) ||
            me->HasAuraType(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT))
        {
            if (me->GetHealth() < me->GetMaxHealth())
            {
                int32 add = me->IsInCombat() ? 0 : IAmFree() && !me->GetVictim() ? me->GetMaxHealth() / 32 : 5 + me->GetCreateHealth() / 256;

                if (baseRegen > 0)
                {
                    add += std::max<int32>(baseRegen / 5, 1);
                }

                if (me->IsPolymorphed())
                {
                    add += me->GetMaxHealth() / 6;
                }
                else if (!me->IsInCombat() || me->HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
                {
                    if (!me->IsInCombat())
                    {
                        Unit::AuraEffectList const& mModHealthRegenPct = me->GetAuraEffectsByType(SPELL_AURA_MOD_HEALTH_REGEN_PERCENT);

                        for (Unit::AuraEffectList::const_iterator i = mModHealthRegenPct.begin();
                             i != mModHealthRegenPct.end();
                             ++i)
                        {
                            AddPct(add, (*i)->GetAmount());
                        }

                        add += me->GetTotalAuraModifier(SPELL_AURA_MOD_REGEN) * REGEN_CD / 5000;
                    }
                    else if (me->HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
                    {
                        ApplyPct(add, me->GetTotalAuraModifier(SPELL_AURA_MOD_REGEN_DURING_COMBAT));
                    }
                }

                add += me->GetTotalAuraModifier(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT);

                if (add < 0)
                {
                    add = 0;
                }

                me->ModifyHealth(add);
            }
        }

        // Regen Mana
        if (me->GetMaxPower(POWER_MANA) > 1 &&
            me->GetPower(POWER_MANA) < me->GetMaxPower(POWER_MANA))
        {
            float addvalue;

            if (me->IsUnderLastManaUseEffect())
            {
                addvalue = me->GetFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER);
            }
            else
            {
                addvalue = me->GetFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER);
            }

            addvalue *= sWorld->getRate(RATE_POWER_MANA) * REGEN_CD * 0.001f; //regenTimer threshold / 1000

            if (addvalue < 0.0f)
            {
                addvalue = 0.0f;
            }

            me->ModifyPower(POWER_MANA, int32(addvalue));
        }
    }
}

void BotAI::RegenerateEnergy()
{
    uint32 curValue = me->GetPower(POWER_ENERGY);
    uint32 maxValue = me->GetMaxPower(POWER_ENERGY);

    if (curValue < maxValue)
    {
        float addvalue = 0.01f * m_lastUpdateDiff * sWorld->getRate(RATE_POWER_ENERGY); //10 per sec
        Unit::AuraEffectList const& ModPowerRegenPCTAuras = me->GetAuraEffectsByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);

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
            me->SetPower(POWER_ENERGY, curValue);
        }
        else
        {
            me->UpdateUInt32Value(UNIT_FIELD_POWER1 + POWER_ENERGY, curValue);
        }
    }
}
