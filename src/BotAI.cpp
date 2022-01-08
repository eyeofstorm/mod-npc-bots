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
    LOG_INFO("npcbots", "↓↓↓↓↓↓ BotAI::BotAI (this: 0X%016llX, name: %s)", (uint64)this, creature->GetName().c_str());

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

    sBotsRegistry->Register(this);

    LOG_INFO("npcbots", "↑↑↑↑↑↑ BotAI::BotAI (this: 0X%016llX, name: %s)", (uint64)this, creature->GetName().c_str());
}

BotAI::~BotAI()
{
    LOG_INFO("npcbots", "↓↓↓↓↓↓ BotAI::~BotAI (this: 0X%016llX, name: %s)", (uint64)this, m_bot->GetName().c_str());

    while (!m_spells.empty())
    {
        BotSpellMap::iterator itr = m_spells.begin();
        delete itr->second;
        m_spells.erase(itr);
    }

    sBotsRegistry->Unregister(this);

    LOG_INFO("npcbots", "↑↑↑↑↑↑ BotAI::~BotAI (this: 0X%016llX, name: %s)", (uint64)this, m_bot->GetName().c_str());
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

    if (!m_bot->HasUnitState(UNIT_STATE_STUNNED) &&
        who->isTargetableForAttack(true, m_bot) &&
        who->isInAccessiblePlaceFor(m_bot))
    {
        if (AssistPlayerInCombat(who))
        {
            Player* player = who->GetVictim()->GetCharmerOrOwnerPlayerOrPlayerItself();

            LOG_DEBUG(
                "npcbots",
                "bot [%s] start assist player [%s] attack creature [%s] in LOS.",
                m_bot->GetName().c_str(),
                player ? player->GetName().c_str() : "unknown",
                who->GetName().c_str());

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

        LOG_DEBUG(
              "npcbots",
              "bot [%s] start attack creature [%s] in LOS.",
              m_bot->GetName().c_str(),
              who->GetName().c_str());

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
            LOG_DEBUG("npcbots", "bot [%s] left combat, returning to combat start position.", m_bot->GetName().c_str());

            float fPosX, fPosY, fPosZ;
            m_bot->GetPosition(fPosX, fPosY, fPosZ);

            m_bot->GetMotionMaster()->MovePoint(POINT_COMBAT_START, fPosX, fPosY, fPosZ);
        }
    }
    else
    {
        if (m_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
        {
            LOG_DEBUG("npcbots", "bot [%s] left combat, returning to home.", m_bot->GetName().c_str());

            m_bot->GetMotionMaster()->MoveTargetedHome();
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

    m_bot->DespawnOrUnsummon();
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
    m_lastUpdateDiff = uiDiff;

    UpdateCommonTimers(uiDiff);

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

    if (m_potionTimer > uiDiff && (m_potionTimer < POTION_CD || !m_bot->IsInCombat()))
    {
        m_potionTimer -= uiDiff;
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
                    LOG_DEBUG("npcbots", "bot [%s] is returning to leader.", m_bot->GetName().c_str());

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
//It will cause me to attack who that are attacking _any_ player (which has been confirmed may happen also on offi)
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

bool BotAI::IsSpellReady(uint32 basespell, uint32 diff) const
{
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
        int32 baseRegen = 0;

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

    m_bot->CastSpell(me, GetPotion(mana));
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

bool BotAI::OnBeforeOwnerTeleport(
                    uint32 mapid, float x, float y, float z, float orientation,
                    uint32 options,
                    Unit* target)
{
    LOG_DEBUG(
          "npcbots",
          "☆☆☆ bot [%s] despawn due to owner move worldport/teleport...",
          m_bot->GetName().c_str());

    m_bot->DespawnOrUnsummon();

    return true;
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

void BotAI::OnBotOwnerMoveTeleport(Player* owner)
{
    Map* botCurMap = m_bot->FindMap();
    Map* ownerCurMap = owner->FindMap();
    
    if ((m_bot->IsInWorld() || (botCurMap && botCurMap->IsDungeon())) &&
        m_bot->IsAlive() &&
        HasBotState(STATE_FOLLOW_INPROGRESS))
    {
        if (IsTeleportNear(owner))
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

    if (m_bot->IsAlive())
    {
        m_bot->CastSpell(m_bot, COSMETIC_TELEPORT_EFFECT, true);
    }

    Unit* owner = GetBotOwner();

    if (owner)
    {
        Map* map = owner->FindMap();

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

        StartFollow(leader);
    }

    return true;
}

void BotAI::BotTeleportHome()
{
    ASSERT(m_bot != nullptr);

    uint16 mapid;
    Position pos;

    GetBotHomePosition(mapid, &pos);
    Map* map = sMapMgr->CreateBaseMap(mapid);

    LOG_DEBUG(
        "npcbots",
        "bot [%s] teleport to home [%s].",
        m_bot->GetName().c_str(),
        map ? map->GetMapName() : "known");

    BotMgr::TeleportBot(
                m_bot,
                map,
                pos.m_positionX,
                pos.m_positionY,
                pos.m_positionZ,
                pos.m_orientation);
}

void BotAI::GetBotHomePosition(uint16& mapid, Position* pos)
{
    CreatureData const* data = m_bot->GetCreatureData();

    mapid = data->mapid;
    pos->Relocate(data->posX, data->posY, data->posZ, data->orientation);
}

bool BotAI::IsTeleportNear(WorldObject* landPos)
{
     Map* landMap = landPos->GetMap();
       Map* botMap = m_bot->GetMap();

       if (landMap && botMap)
       {
           uint32 landMapId = landPos->GetMap()->GetId();
           uint32 botMapId = m_bot->GetMap()->GetId();

           return landMapId == botMapId;
       }

       return false;
}

bool BotAI::IsTeleportFar(WorldObject* landPos)
{
    Map* landMap = landPos->GetMap();
    Map* botMap = m_bot->GetMap();

    if (landMap && botMap)
    {
        uint32 landMapId = landPos->GetMap()->GetId();
        uint32 botMapId = m_bot->GetMap()->GetId();

        return landMapId != botMapId;
    }

    return false;
}
