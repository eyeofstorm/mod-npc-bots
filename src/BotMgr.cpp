/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BotCommon.h"
#include "BotEvents.h"
#include "BotMgr.h"
#include "Group.h"
#include "Item.h"
#include "Log.h"
#include "Map.h"

void BotMgr::HireBot(Unit* owner, Creature* bot)
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

    BotMgr::SetBotLevel(bot, owner->getLevel(), true);

    if (BotAI* ai = (BotAI*)bot->AI())
    {
        ai->SetBotOwner(owner);
        sBotsRegistry->RegisterOrUpdate(ai);
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

    CleanupsBeforeBotRemove(bot);

    if (bot->IsInWorld() && bot->FindMap())
    {
        bot->DespawnOrUnsummon();
    }

    LOG_DEBUG("npcbots", "end dismiss bot [%s]. dismiss bot ok...", bot->GetName().c_str());

    return true;
}

void BotMgr::CleanupsBeforeBotRemove(Creature* bot)
{
    ASSERT(bot != nullptr);

    if (BotAI* ai = (BotAI*)bot->AI())
    {
        ai->SetBotOwner(nullptr);
        ai->UnSummonBotPet();
        sBotsRegistry->Unregister(ai);
        ai->SetFollowComplete();
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

    SetBotLevel(bot, bot->GetCreatureTemplate()->minlevel, false);

//    Map* map = bot->FindMap();
//
//    if (!map || map->IsDungeon())
//    {
//        LOG_DEBUG("npcbots", "CleanupsBeforeBotRemove calls Creature::RemoveFromWorld()");
//        bot->RemoveFromWorld();
//    }
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

void BotMgr::TeleportBot(Creature* bot, Map* newMap, float x, float y, float z, float ori)
{
    LOG_INFO(
              "npcbots",
              "↓↓↓↓↓↓  start BotMgr::TeleportBot([bot: %s], [newMap: %s], [x: %.1f], [y: %.1f], [z: %.1f], [ori: %.1f])",
              bot->GetName().c_str(),
              newMap->GetMapName(),
              x, y, z, ori);

    BotAI* oldAI = const_cast<BotAI *>(GetBotAI(bot));

    oldAI->KillEvents(true);

    if (bot->GetVehicle())
    {
        bot->ExitVehicle();
    }

    if (bot->IsInWorld())
    {
        bot->CastSpell(bot, COSMETIC_TELEPORT_EFFECT, true);
    }

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
    }

    // add bot to new map
    if (oldAI->IAmFree())
    {
        bot->Relocate(x, y, z, ori);
        bot->SetMap(newMap);

        bot->GetMap()->AddToMap(bot);

        LOG_INFO("npcbots", "↑↑↑↑↑↑  end BotMgr::TeleportBot(...)  (1)");

        return;
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

    TeleportFinishEvent* finishEvent = new TeleportFinishEvent(oldAI);
    uint32 delay = urand(5000, 8000);
    LOG_DEBUG("npcbots", "AI(0X%016llX) execute event TeleportFinishEvent. delay %ums", (uint64)oldAI, delay);
    oldAI->GetEvents()->AddEvent(finishEvent, oldAI->GetEvents()->CalculateTime(delay));

    finishEvent->ScheduleAbort(); //make sure event will not be executed twice
    finishEvent->Execute(0, 0);

    LOG_INFO("npcbots", "↑↑↑↑↑↑  end BotMgr::TeleportBot(...)  (2)");
}

bool BotMgr::RestrictBots(Creature const* bot, bool add)
{
    if (Unit const* owner = const_cast<Unit const*>(GetBotAI(bot)->GetBotOwner()))
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

void BotsRegistry::RegisterOrUpdate(BotAI* ai)
{
    ASSERT(ai != nullptr);
    ASSERT(ai->GetBot() != nullptr);

    Creature* bot = ai->GetBot();
    ObjectGuid botGUID = bot->GetGUID();

    if (bot->GetOwnerGUID() == ObjectGuid::Empty)
    {
        LOG_INFO(
            "npcbots",
            "↓↓↓↓↓↓  begin register bot [GUID: %u %s] to free bot registry...",
            bot->GetGUID().GetCounter(),
            bot->GetName().c_str());

        BotEntry* hiredBot = m_hiredBotRegistry[botGUID];

        if (hiredBot)
        {
//            LOG_WARN(
//                 "npcbots", "bot [GUID: %u %s] already registered in hired bot registry. delete...",
//                 bot->GetGUID().GetCounter(),
//                 bot->GetName().c_str());

            m_hiredBotRegistry.erase(botGUID);
            delete hiredBot;
        }

        BotEntry* freeBot = m_freeBotRegistry[botGUID];

        if (freeBot)
        {
//            LOG_WARN(
//                "npcbots", "bot [GUID: %u %s] already registered in free bot registry. delete...",
//                bot->GetGUID().GetCounter(),
//                bot->GetName().c_str());

            m_freeBotRegistry.erase(botGUID);
            delete freeBot;
        }

        m_freeBotRegistry[botGUID] = new BotEntry(ai);

        LOG_INFO(
            "npcbots",
            "↑↑↑↑↑↑  end register bot [GUID: %u AI: 0X%016llX  %s] to free bot registry...",
            m_freeBotRegistry[botGUID]->GetBot()->GetGUID().GetCounter(),
            (uint64)m_freeBotRegistry[botGUID]->GetBotAI(),
            m_freeBotRegistry[botGUID]->GetBot()->GetName().c_str());
    }
    else
    {
        LOG_INFO(
            "npcbots",
            "↓↓↓↓↓↓  begin register bot [GUID: %u %s] to hired bot registry...",
            bot->GetGUID().GetCounter(),
            bot->GetName().c_str());

        BotEntry* freeBot = m_freeBotRegistry[botGUID];

        if (freeBot)
        {
//            LOG_WARN(
//                "npcbots", "bot [GUID: %u %s] already registered in free bot registry. delete...",
//                bot->GetGUID().GetCounter(),
//                bot->GetName().c_str());

            m_freeBotRegistry.erase(botGUID);
            delete freeBot;
        }

        BotEntry* hiredBot = m_hiredBotRegistry[botGUID];

        if (hiredBot)
        {
//            LOG_WARN(
//                "npcbots", "bot [GUID: %u %s] already registered in hired bot registry. delete...",
//                bot->GetGUID().GetCounter(),
//                bot->GetName().c_str());

            m_hiredBotRegistry.erase(botGUID);
            delete hiredBot;
        }

        m_hiredBotRegistry[botGUID] = new BotEntry(ai);

        LOG_INFO(
            "npcbots",
            "↑↑↑↑↑↑  end register bot [GUID: %u AI: 0X%016llX  %s] to hired bot registry...",
            m_hiredBotRegistry[botGUID]->GetBot()->GetGUID().GetCounter(),
            (uint64)m_hiredBotRegistry[botGUID]->GetBotAI(),
            m_hiredBotRegistry[botGUID]->GetBot()->GetName().c_str());
    }
}

void BotsRegistry::Unregister(BotAI* ai)
{
    ASSERT(ai != nullptr);
    ASSERT(ai->GetBot() != nullptr);

    Creature* bot = ai->GetBot();
    ObjectGuid botGUID = bot->GetGUID();

    LOG_INFO(
        "npcbots",
        "↓↓↓↓↓↓  begin unregister bot [GUID: %u %s] from bot registry...",
        bot->GetGUID().GetCounter(),
        bot->GetName().c_str());

    BotEntry* freeBot = m_freeBotRegistry[botGUID];

    if (freeBot)
    {
        if (freeBot->GetBot()->GetGUID() == botGUID)
        {
            LOG_WARN(
                "npcbots", "bot [GUID: %u %s] skip unregister from free bot registry.",
                bot->GetGUID().GetCounter(),
                bot->GetName().c_str());
        }
        else
        {
//            LOG_DEBUG(
//                "npcbots", "bot [GUID: %u %s] unregistered from free bot registry.",
//                bot->GetGUID().GetCounter(),
//                bot->GetName().c_str());

            m_freeBotRegistry.erase(botGUID);
            delete freeBot;
        }
    }

    BotEntry* hiredBot = m_hiredBotRegistry[botGUID];

    if (hiredBot)
    {
        if (hiredBot->GetBot()->GetGUID() == botGUID)
        {
            LOG_WARN(
                "npcbots", "bot [GUID: %u %s] skip unregister from hired bot registry.",
                bot->GetGUID().GetCounter(),
                bot->GetName().c_str());
        }
        else
        {
//            LOG_DEBUG(
//                "npcbots", "bot [GUID: %u %s] unregistered from hired bot registry",
//                bot->GetGUID().GetCounter(),
//                bot->GetName().c_str());

            m_hiredBotRegistry.erase(botGUID);
            delete hiredBot;
        }
    }

    LOG_INFO(
        "npcbots",
        "↑↑↑↑↑↑  end   unregister bot [GUID: %u AI: 0X%016llX  %s] from bot registry...",
        bot->GetGUID().GetCounter(),
        (uint64)ai,
        bot->GetName().c_str());
}

BotEntry* BotsRegistry::GetEntry(Creature const* bot)
{
    ASSERT(bot != nullptr);

    ObjectGuid ownerGUID = bot->GetOwnerGUID();
    ObjectGuid botGUID = bot->GetGUID();

    if (ownerGUID == ObjectGuid::Empty)
    {
        BotEntry* freeBot = m_freeBotRegistry[botGUID];
        return freeBot;
    }
    else
    {
        BotEntry* hiredBot = m_hiredBotRegistry[botGUID];
        return hiredBot;
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

            if (entry)
            {
                if (entry->GetBot()->GetOwnerGUID() == ownerGUID)
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
