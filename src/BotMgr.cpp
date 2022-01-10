/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "Creature.h"
#include "BotAI.h"
#include "BotCommon.h"
#include "BotEvents.h"
#include "BotMgr.h"
#include "Group.h"
#include "Item.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "Player.h"
#include "Unit.h"

void BotMgr::HireBot(Player* owner, Creature* bot)
{
    ASSERT(owner != nullptr);
    ASSERT(bot != nullptr);

    LOG_DEBUG("npcbots", "[%s] begin hire the bot [%s].", owner->GetName().c_str(), bot->GetName().c_str());

    bot->SetOwnerGUID(owner->GetGUID());
    bot->SetCreatorGUID(owner->GetGUID());
    bot->SetByteValue(UNIT_FIELD_BYTES_2, 1, owner->GetByteValue(UNIT_FIELD_BYTES_2, 1));
    bot->SetFaction(owner->GetFaction());
    bot->SetPvP(owner->IsPvP());
    bot->SetPhaseMask(owner->GetPhaseMask(), true);
    bot->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
    bot->m_ControlledByPlayer = true;

    BotMgr::SetBotLevel(bot, owner->getLevel(), true);

    if (BotAI* ai = (BotAI*)bot->AI())
    {
        ai->SetBotOwner(owner);
        sBotsRegistry->Update(ai, MODE_UPDATE_HIRE);
        ai->StartFollow(owner);
    }
    else
    {
        LOG_ERROR("npcbots", "AI of bot [%s] not found.", bot->GetName().c_str());
    }

    LOG_DEBUG("npcbots", "[%s] end hire the bot [%s].", owner->GetName().c_str(), bot->GetName().c_str());
}

bool BotMgr::DismissBot(Creature* bot)
{
    ASSERT(bot != nullptr);

    LOG_DEBUG("npcbots", "begin dismiss bot [%s]", bot->GetName().c_str());

    if (bot->IsSummon())
    {
        return false;
    }

    CleanupsBeforeBotDismiss(bot);

    if (bot->IsInWorld())
    {
        Map* map = bot->FindMap();

        if (!map || map->IsDungeon())
        {
            BotAI* botAI = (BotAI *)bot->AI();

            TeleportHomeEvent* homeEvent = new TeleportHomeEvent(botAI);
            botAI->GetEvents()->AddEvent(homeEvent, botAI->GetEvents()->CalculateTime(50));
        }
        else
        {
            LOG_DEBUG("npcbots", "bot [%s] despawned...", bot->GetName().c_str());

            bot->DespawnOrUnsummon();
        }
    }

    LOG_DEBUG("npcbots", "end dismiss bot [%s].", bot->GetName().c_str());

    return true;
}

void BotMgr::CleanupsBeforeBotDismiss(Creature* bot)
{
    ASSERT(bot != nullptr);

    if (BotAI* ai = (BotAI*)bot->AI())
    {
        ai->SetBotOwner(nullptr);
        ai->UnSummonBotPet();
        ai->SetFollowComplete();

        sBotsRegistry->Update(ai, MODE_UPDATE_DISMISS);
    }

    if (bot->GetVehicle())
    {
        bot->ExitVehicle();
    }

    bot->SetOwnerGUID(ObjectGuid::Empty);
    bot->SetCreatorGUID(ObjectGuid::Empty);
    bot->SetByteValue(UNIT_FIELD_BYTES_2, 1, 0);
    bot->SetPhaseMask(0, true);
    bot->SetFaction(bot->GetCreatureTemplate()->faction);
    bot->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
    bot->m_ControlledByPlayer = false;

    SetBotLevel(bot, bot->GetCreatureTemplate()->minlevel, false);
}

BotAI const* BotMgr::GetBotAI(Creature const* bot)
{
    ASSERT(bot != nullptr);

    return (BotAI*)bot->AI();
}

void BotMgr::SetBotLevel(Creature* bot, uint8 level, bool showLevelChange)
{
    CreatureTemplate const* cInfo = bot->GetCreatureTemplate();

    bot->SetLevel(level, showLevelChange);

    CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(level, cInfo->unit_class);

    // health
    float healthmod = sWorld->getRate(RATE_CREATURE_ELITE_ELITE_HP);

    uint32 basehp = std::max<uint32>(1, stats->GenerateHealth(cInfo));
    uint32 health = uint32(basehp * healthmod);

    bot->SetCreateHealth(health);
    bot->SetMaxHealth(health);
    bot->SetHealth(health);
    bot->ResetPlayerDamageReq();

    // mana
    uint32 mana = stats->GenerateMana(cInfo);

    bot->SetCreateMana(mana);
    bot->SetMaxPower(POWER_MANA, mana);
    bot->SetPower(POWER_MANA, mana);

    bot->SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, (float)health);
    bot->SetModifierValue(UNIT_MOD_MANA, BASE_VALUE, (float)mana);

    // damage
    float baseDamage = stats->GenerateBaseDamage(cInfo);
    float damageMod = sWorld->getRate(RATE_CREATURE_NORMAL_DAMAGE);

    float weaponBaseMinDamage = baseDamage;
    float weaponBaseMaxDamage = baseDamage * damageMod;

    bot->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    bot->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);

    bot->SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, weaponBaseMinDamage * 0.75);
    bot->SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, weaponBaseMaxDamage * 0.75);

    bot->SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    bot->SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);

    bot->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, stats->AttackPower);
    bot->SetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, stats->RangedAttackPower);
}

int BotMgr::GetBotsCount(Unit* owner)
{
    BotEntryMap botsMap = sBotsRegistry->GetEntryByOwnerGUID(owner->GetGUID());

    return botsMap.size();
}

void BotMgr::OnBotSpellGo(Unit const* caster, Spell const* spell, bool ok)
{
    BotAI* ai = const_cast<BotAI*>(GetBotAI(caster->ToCreature()));

    if (ai)
    {
        ai->OnBotSpellGo(spell, ok);
    }
}

void BotMgr::OnPlayerMoveWorldport(Player* player)
{
    if (!player)
    {
        return;
    }

    BotEntryMap botsMap = sBotsRegistry->GetEntryByOwnerGUID(player->GetGUID());

    if (!botsMap.empty())
    {
        for (BotEntryMap::iterator itr = botsMap.begin(); itr != botsMap.end(); ++itr)
        {
            BotEntry* entry = itr->second;

            if (entry)
            {
                BotAI* ai = entry->GetBotAI();

                if (ai)
                {
                    ai->OnBotOwnerMoveWorldport(player);
                }
            }
        }
    }
}

void BotMgr::OnPlayerMoveTeleport(Player* player)
{
    if (!player)
    {
        return;
    }

    BotEntryMap botsMap = sBotsRegistry->GetEntryByOwnerGUID(player->GetGUID());

    if (!botsMap.empty())
    {
        for (BotEntryMap::iterator itr = botsMap.begin(); itr != botsMap.end(); ++itr)
        {
            BotEntry* entry = itr->second;

            if (entry)
            {
                BotAI* ai = entry->GetBotAI();

                if (ai)
                {
                    ai->OnBotOwnerMoveTeleport(player);
                }
            }
        }
    }
}

bool BotMgr::TeleportBot(Creature* bot, Map* newMap, float x, float y, float z, float ori)
{
    BotAI* oldAI = (BotAI *)bot->AI();

    oldAI->KillEvents(true);

    // remove bot from old map
    if (Map* mymap = bot->FindMap())
    {
        oldAI->BotStopMovement();
        oldAI->UnSummonBotPet();

        bot->InterruptNonMeleeSpells(true);

        if (bot->IsInWorld())
        {
            bot->RemoveFromWorld();
        }

        bot->RemoveAllGameObjects();
        bot->m_Events.KillAllEvents(false); // non-delatable (currently casted spells) will not deleted now but it will deleted at call in Map::RemoveAllObjectsInRemoveList
        bot->CombatStop();
        bot->ClearComboPointHolders();

        mymap->RemoveFromMap(bot, false);

        LOG_INFO(
            "npcbots",
            "remove bot [%s] from map [%s]. old AI(0X%016llX)",
            bot->GetName().c_str(),
            mymap->GetMapName(),
            (uint64)oldAI);
    }

    // update group member online state
    if (Unit const* owner = oldAI->GetBotOwner())
    {
        Player const* player = owner->ToPlayer();

        if (player)
        {
            if (Group* gr = const_cast<Group*>(player->GetGroup()))
            {
                if (gr->IsMember(bot->GetGUID()))
                {
                    gr->SendUpdate();
                }
            }
        }
    }

    bot->Relocate(x, y, z, ori);
    bot->SetMap(newMap);

    // NOTICE!!!
    // call function AddToMap below on bot will lead to create a new BotAI instance for it.
    // so wo need save some old AI's state before call AddToMap function.
    Unit* leader = oldAI->GetLeaderForFollower();
    bool isFreeBot = oldAI->IAmFree();

    // add bot to new map
    newMap->AddToMap(bot);

    if (isFreeBot)
    {
        return true;
    }

    // use the new created AI here.
    BotAI* newAI = (BotAI*)bot->AI();

    LOG_INFO(
         "npcbots",
         "add bot [%s] back to map [%s]. old AI(0X%016llX), new AI(0X%016llX)",
         bot->GetName().c_str(),
         newMap->GetMapName(),
         (uint64)oldAI,
         (uint64)newAI);

    // restore saved old BotAI state.
    if (leader)
    {
        newAI->SetLeaderGUID(leader->GetGUID());
    }

    TeleportFinishEvent* finishEvent = new TeleportFinishEvent(newAI);
    newAI->GetEvents()->AddEvent(finishEvent, newAI->GetEvents()->CalculateTime(urand(500, 800)));
}

bool BotMgr::RestrictBots(Creature const* bot, bool add)
{
    if (Unit* owner = GetBotAI(bot)->GetBotOwner())
    {
        if (!owner->FindMap())
        {
            return true;
        }

        if (owner->IsInFlight())
        {
            return true;
        }
    }

    return false;
}

void BotsRegistry::Register(BotAI* ai)
{
    ASSERT(ai != nullptr);
    ASSERT(ai->GetBot() != nullptr);

    Creature* bot = ai->GetBot();
    ObjectGuid botGUID = bot->GetGUID();

    if (!ai->GetBotOwner())
    {
        BotEntryMap::iterator itr = m_hiredBotRegistry.find(botGUID);

        if (itr != m_hiredBotRegistry.end())
        {
            m_hiredBotRegistry.erase(itr);
            delete itr->second;
        }

        itr = m_freeBotRegistry.find(botGUID);

        if (itr != m_freeBotRegistry.end())
        {
            m_freeBotRegistry.erase(itr);
            delete itr->second;

            LOG_WARN(
                "npcbots",
                "bot [GUID: %u AI: 0X%016llX  %s] register to free bot registry...",
                bot->GetGUID().GetCounter(),
                (uint64)ai,
                bot->GetName().c_str());

            m_freeBotRegistry[botGUID] = new BotEntry(ai);
        }
        else
        {
            LOG_WARN(
                "npcbots",
                "bot [GUID: %u AI: 0X%016llX  %s] register to free bot registry...",
                bot->GetGUID().GetCounter(),
                (uint64)ai,
                bot->GetName().c_str());

            m_freeBotRegistry[botGUID] = new BotEntry(ai);
        }
    }
    else
    {
        BotEntryMap::iterator itr = m_freeBotRegistry.find(botGUID);

        if (itr != m_freeBotRegistry.end())
        {
            m_freeBotRegistry.erase(itr);
            delete itr->second;
        }

        itr = m_hiredBotRegistry.find(botGUID);

        if (itr != m_hiredBotRegistry.end())
        {
            m_hiredBotRegistry.erase(itr);
            delete itr->second;

            LOG_WARN(
                "npcbots",
                "bot [GUID: %u AI: 0X%016llX  %s] register to hired bot registry...",
                bot->GetGUID().GetCounter(),
                (uint64)ai,
                bot->GetName().c_str());

            m_hiredBotRegistry[botGUID] = new BotEntry(ai);
        }
        else
        {
            LOG_WARN(
                "npcbots",
                "bot [GUID: %u AI: 0X%016llX  %s] register to hired bot registry...",
                bot->GetGUID().GetCounter(),
                (uint64)ai,
                bot->GetName().c_str());

            m_hiredBotRegistry[botGUID] = new BotEntry(ai);
        }
    }

    sBotsRegistry->LogFreeBotRegistryEntries();
    sBotsRegistry->LogHiredBotRegistryEntries();
}

void BotsRegistry::Update(BotAI* ai, uint16 mode)
{
    ASSERT(ai != nullptr);
    ASSERT(ai->GetBot() != nullptr);

    Creature* bot = ai->GetBot();
    ObjectGuid botGUID = bot->GetGUID();

    if (MODE_UPDATE_HIRE == mode)
    {
        BotEntryMap::iterator itr = m_freeBotRegistry.find(botGUID);

        if (itr != m_freeBotRegistry.end())
        {
            BotEntry* freeBot = itr->second;

            if (freeBot->GetBotAI() == ai)
            {
                m_freeBotRegistry.erase(botGUID);
                delete freeBot;
            }
        }

        itr = m_hiredBotRegistry.find(botGUID);

        if (itr != m_hiredBotRegistry.end())
        {
            BotEntry* hiredBot = itr->second;

            if (hiredBot->GetBotAI() == ai)
            {
                m_hiredBotRegistry.erase(botGUID);
                delete hiredBot;

                m_hiredBotRegistry[botGUID] = new BotEntry(ai);
            }
        }
        else
        {
            m_hiredBotRegistry[botGUID] = new BotEntry(ai);
        }
    }
    else if (MODE_UPDATE_DISMISS == mode)
    {
        BotEntryMap::iterator itr = m_hiredBotRegistry.find(botGUID);

        if (itr != m_hiredBotRegistry.end())
        {
            BotEntry* hiredBot = itr->second;

            if (hiredBot->GetBotAI() == ai)
            {
                m_hiredBotRegistry.erase(botGUID);
                delete hiredBot;
            }
        }

        itr = m_freeBotRegistry.find(botGUID);

        if (itr != m_freeBotRegistry.end())
        {
            BotEntry* freeBot = itr->second;

            if (freeBot->GetBotAI() == ai)
            {
                m_freeBotRegistry.erase(botGUID);
                delete freeBot;

                m_freeBotRegistry[botGUID] = new BotEntry(ai);
            }
        }
        else
        {
            m_freeBotRegistry[botGUID] = new BotEntry(ai);
        }
    }

    sBotsRegistry->LogFreeBotRegistryEntries();
    sBotsRegistry->LogHiredBotRegistryEntries();
}

void BotsRegistry::Unregister(BotAI* ai)
{
    ASSERT(ai != nullptr);
    ASSERT(ai->GetBot() != nullptr);

    Creature* bot = ai->GetBot();
    ObjectGuid botGUID = bot->GetGUID();

    BotEntryMap::iterator itr = m_freeBotRegistry.find(botGUID);

    if (itr != m_freeBotRegistry.end())
    {
        BotEntry* freeBot = itr->second;

        if (freeBot->GetBot()->GetGUID() == botGUID && freeBot->GetBotAI() != ai)
        {
            LOG_WARN(
                "npcbots",
                "bot [GUID: %u AI: 0X%016llX  %s] skip unregister from free bot registry.",
                bot->GetGUID().GetCounter(),
                (uint64)ai,
                bot->GetName().c_str());
        }
        else
        {
            LOG_WARN(
                "npcbots",
                "bot [GUID: %u AI: 0X%016llX  %s] unregister from free bot registry.",
                bot->GetGUID().GetCounter(),
                (uint64)ai,
                bot->GetName().c_str());

            m_freeBotRegistry.erase(botGUID);
            delete freeBot;
        }
    }

    itr = m_hiredBotRegistry.find(botGUID);

    if (itr != m_hiredBotRegistry.end())
    {
        BotEntry* hiredBot = itr->second;

        if (hiredBot->GetBot()->GetGUID() == botGUID && hiredBot->GetBotAI() != ai)
        {
            LOG_WARN(
                "npcbots",
                "bot [GUID: %u AI: 0X%016llX  %s] skip unregister from hired bot registry.",
                bot->GetGUID().GetCounter(),
                (uint64)ai,
                bot->GetName().c_str());
        }
        else
        {
            LOG_WARN(
                "npcbots",
                "bot [GUID: %u AI: 0X%016llX  %s] unregister from hired bot registry.",
                bot->GetGUID().GetCounter(),
                (uint64)ai,
                bot->GetName().c_str());

            m_hiredBotRegistry.erase(botGUID);
            delete hiredBot;
        }
    }

    sBotsRegistry->LogFreeBotRegistryEntries();
    sBotsRegistry->LogHiredBotRegistryEntries();
}

BotEntry* BotsRegistry::GetEntry(Creature const* bot)
{
    ASSERT(bot != nullptr);

    BotAI* botAI = (BotAI *)bot->AI();

    ObjectGuid botGUID = bot->GetGUID();

    if (!botAI)
    {
        BotEntryMap::iterator itr = m_freeBotRegistry.find(botGUID);

        if (itr != m_freeBotRegistry.end())
        {
            return itr->second;
        }
        else
        {
            return nullptr;
        }
    }
    else
    {
        BotEntryMap::iterator itr = m_hiredBotRegistry.find(botGUID);

        if (itr != m_hiredBotRegistry.end())
        {
            return itr->second;
        }
        else
        {
            return nullptr;
        }
    }
}

BotEntryMap BotsRegistry::GetEntryByOwnerGUID(ObjectGuid ownerGUID)
{
    BotEntryMap botsMap;

    if (!m_hiredBotRegistry.empty())
    {
        for (BotEntryMap::iterator itr = m_hiredBotRegistry.begin(); itr != m_hiredBotRegistry.end(); ++itr)
        {
            BotEntry* entry = itr->second;

            if (entry && !entry->GetBotAI()->IsPetAI())
            {
                Unit* owner = entry->GetBotOwner();

                if (owner && owner->GetGUID() == ownerGUID)
                {
                    botsMap[entry->GetBot()->GetGUID()] = entry;
                }
            }
        }
    }

    return botsMap;
}

Creature* BotsRegistry::FindFirstBot(uint32 creatureTemplateEntry)
{
    if (!m_freeBotRegistry.empty())
    {
        for (BotEntryMap::iterator itr = m_freeBotRegistry.begin(); itr != m_freeBotRegistry.end(); ++itr)
        {
            BotEntry* entry = itr->second;

            if (entry)
            {
                if (entry->GetBot()->GetCreatureTemplate()->Entry == creatureTemplateEntry)
                {
                    return entry->GetBot();
                }
            }
        }
    }

    if (!m_hiredBotRegistry.empty())
    {
        for (BotEntryMap::iterator itr = m_hiredBotRegistry.begin(); itr != m_hiredBotRegistry.end(); ++itr)
        {
            BotEntry* entry = itr->second;

            if (entry)
            {
                if (entry->GetBot()->GetCreatureTemplate()->Entry == creatureTemplateEntry)
                {
                    return entry->GetBot();
                }
            }
        }
    }

    return nullptr;
}

void BotsRegistry::LogHiredBotRegistryEntries()
{
    if (!m_hiredBotRegistry.empty())
    {
        LOG_WARN("npcbots", "hired bot registry entries: %lu entries", m_hiredBotRegistry.size());

        for (BotEntryMap::iterator itr = m_hiredBotRegistry.begin(); itr != m_hiredBotRegistry.end(); ++itr)
        {
            BotEntry* entry = itr->second;

            if (entry)
            {
                std::string botName = entry->GetBot()? entry->GetBot()->GetName() : "null";
                std::string botOwnerName = entry->GetBotOwner() ? entry->GetBotOwner()->GetName() : "null";

                LOG_WARN(
                    "npcbots",
                    "    +-- GUID Low: %u, Entry: { name: \"%s\", owner: \"%s\", ai: 0X%016llX }",
                    itr->first.GetCounter(),
                    botName.c_str(),
                    botOwnerName.c_str(),
                    entry->GetBotAI() ? (uint64)entry->GetBotAI() : 0);
            }
            else
            {
                LOG_ERROR("npcbots","    +-- GUID Low: %u, Entry: null", itr->first.GetCounter());
            }
        }
    }
    else
    {
        LOG_WARN("npcbots", "hired bot registry entries:");
        LOG_WARN("npcbots", "    +-- (empty)");
    }
}

void BotsRegistry::LogFreeBotRegistryEntries()
{
    if (!m_freeBotRegistry.empty())
    {
        LOG_WARN("npcbots", "free  bot registry entries: %lu entries", m_freeBotRegistry.size());

        for (BotEntryMap::iterator itr = m_freeBotRegistry.begin(); itr != m_freeBotRegistry.end(); ++itr)
        {
            BotEntry* entry = itr->second;

            if (entry)
            {
                std::string botName = entry->GetBot()? entry->GetBot()->GetName() : "null";
                std::string botOwnerName = entry->GetBotOwner() ? entry->GetBotOwner()->GetName() : "null";

                LOG_WARN(
                    "npcbots",
                    "    +-- GUID Low: %u, Entry: { name: \"%s\", owner: \"%s\", ai: 0X%016llX }",
                    itr->first.GetCounter(),
                    botName.c_str(),
                    botOwnerName.c_str(),
                    entry->GetBotAI() ? (uint64)entry->GetBotAI() : 0);
            }
            else
            {
                LOG_ERROR("npcbots","    +-- GUID Low: %u, Entry: null", itr->first.GetCounter());
            }
        }
    }
    else
    {
        LOG_WARN("npcbots", "free  bot registry entries:");
        LOG_WARN("npcbots", "    +-- (empty)");
    }
}
