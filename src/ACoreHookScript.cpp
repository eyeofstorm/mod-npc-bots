/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ACoreHookScript.h"
#include "BotMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "MapMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "Transport.h"

/////////////////////////////////
// Azeroth core hook scripts here
/////////////////////////////////

bool UnitHookScript::OnBeforePlayerTeleport(
                            Player* player,
                            uint32 mapid, float x, float y, float z, float orientation,
                            uint32 options,
                            Unit* target)
{
    if (BotMgr::GetBotsCount(player) > 0)
    {
        BotEntryMap botsMap = sBotsRegistry->GetEntryByOwnerGUID(player->GetGUID());

        if (!botsMap.empty())
        {
            for (BotEntryMap::iterator itr = botsMap.begin(); itr != botsMap.end(); ++itr)
            {
                BotEntry* entry = itr->second;

                if (entry)
                {
                    if (BotAI* ai = entry->GetBotAI())
                    {
                        ai->OnBeforeOwnerTeleport(mapid, x, y, z, orientation, options, target);
                    }
                }
            }
        }
    }

    return true;
}

void PlayerHookScript::OnLogin(Player* player)
{
    ChatHandler(player->GetSession()).SendSysMessage("This server is running npcbots module...");

    if (strcmp(player->GetName().c_str(), "Felthas") == 0)
    {
        player->SetDisplayId(27545);
    }
}

void PlayerHookScript::OnLogout(Player* player)
{
    BotEntryMap botsMap = sBotsRegistry->GetEntryByOwnerGUID(player->GetGUID());

    if (!botsMap.empty())
    {
        for (BotEntryMap::iterator itr = botsMap.begin(); itr != botsMap.end(); ++itr)
        {
            BotEntry* entry = itr->second;

            if (entry)
            {
                BotMgr::DismissBot(entry->GetBot());
            }
        }
    }
}

// Called when a player switches to a new area (more accurate than UpdateZone)
void PlayerHookScript::OnUpdateArea(Player* player, uint32 oldAreaId, uint32 newAreaId)
{
//    std::string mapName = player->FindMap() ? player->FindMap()->GetMapName() : "unknown map";
//    std::string zoneName = "unknown";
//    std::string oldAreaName = "unknown";
//    std::string newAreaName = "unknown";
//    LocaleConstant locale = sWorld->GetDefaultDbcLocale();
//
//    if (AreaTableEntry const* zone = sAreaTableStore.LookupEntry(player->GetZoneId()))
//    {
//        zoneName = zone->area_name[locale];
//    }
//
//    if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(oldAreaId))
//    {
//        oldAreaName = area->area_name[locale];
//    }
//
//    if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(newAreaId))
//    {
//        newAreaName = area->area_name[locale];
//    }
//
//    LOG_DEBUG(
//            "npcbots",
//            "player [%s] switches from [%s] to [%s, %s, %s].",
//            player->GetName().c_str(),
//            oldAreaName.c_str(),
//            newAreaName.c_str(),
//            zoneName.c_str(),
//            mapName.c_str());

    if (player)
    {
        if (strcmp(player->GetName().c_str(), "Felthas") == 0)
        {
            if (player->GetDisplayId() != 27545)
            {
                player->SetDisplayId(27545);
            }
        }
    }
}

void PlayerHookScript::OnLevelChanged(Player* player, uint8 oldlevel)
{
    if (player)
    {
        uint8 newLevel = player->getLevel();

        BotEntryMap botsMap = sBotsRegistry->GetEntryByOwnerGUID(player->GetGUID());

        if (!botsMap.empty())
        {
            for (BotEntryMap::iterator itr = botsMap.begin(); itr != botsMap.end(); ++itr)
            {
                BotEntry* entry = itr->second;

                if (entry)
                {
                    if (Creature* bot = entry->GetBot())
                    {
                        BotMgr::SetBotLevel(bot, newLevel, false);
                    }
                }
            }
        }
    }
}

void CreatureHookScript::OnAllCreatureUpdate(Creature* creature, uint32 diff)
{
    if (creature)
    {
        if (creature->GetEntry() >= 9000000) 
        {
            BotAI* ai = const_cast<BotAI*>(BotMgr::GetBotAI(creature));

            if (ai)
            {
                ai->OnCreatureFinishedUpdate(diff);
            }
        }
    }
}

void SpellHookScript::OnSpellGo(Unit const* caster, Spell const* spell, bool ok)
{
    if (!caster)
    {
        return;
    }

    if (caster->GetTypeId() == TYPEID_PLAYER)
    {
        Player const* player = caster->ToPlayer();

        if (player)
        {
            if (strcmp(player->GetName().c_str(), "Felthas") == 0)
            {
                SpellEntry const* spellEntry = sSpellStore.LookupEntry(spell->GetSpellInfo()->Id);

                LOG_DEBUG(
                    "npcbots",
                    "bot [%s] cast spell [%u: %s] %s...",
                    player->GetName().c_str(),
                    spellEntry->Id,
                    spellEntry ? spellEntry->SpellName[sWorld->GetDefaultDbcLocale()] : "unkown",
                    ok ? "ok" : "failed");
            }
        }
    }
    else if (caster->GetTypeId() == TYPEID_UNIT)
    {
        Creature const* creature = caster->ToCreature();

        if (creature)
        {
            if (creature->GetEntry() >= 9000000)
            {
                SpellEntry const* spellEntry = sSpellStore.LookupEntry(spell->GetSpellInfo()->Id);

                LOG_DEBUG(
                    "npcbots",
                    "bot [%s] cast spell [id: %u %s] %s...",
                    creature->GetName().c_str(),
                    spellEntry->Id,
                    spellEntry ? spellEntry->SpellName[sWorld->GetDefaultDbcLocale()] : "unkown",
                    ok ? "ok" : "failed");

                BotMgr::OnBotSpellGo(creature, spell, ok);
            }
        }
    }
}

void MovementHandlerHookScript::OnPlayerMoveWorldport(Player* player)
{
    if (player)
    {
        std::string mapName = "unknown";
        std::string zoneName = "unknown";
        std::string areaName = "unknown";
        LocaleConstant locale = sWorld->GetDefaultDbcLocale();

        Map* map = player->GetMap();

        if (map)
        {
            mapName = map->GetMapName();

            uint32 areaId, zoneId;
            map->GetZoneAndAreaId(
                      player->GetPhaseMask(),
                      zoneId,
                      areaId,
                      player->GetPositionX(),
                      player->GetPositionY(),
                      player->GetPositionZ());

            if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId))
            {
                areaName = area->area_name[locale];
            }

            if (AreaTableEntry const* zone = sAreaTableStore.LookupEntry(zoneId))
            {
                zoneName = zone->area_name[locale];
            }
        }

        LOG_DEBUG(
                "npcbots",
                "☆☆☆ on player [%s] worldported to [%s, %s, %s].",
                player->GetName().c_str(),
                areaName.c_str(),
                zoneName.c_str(),
                mapName.c_str());

        BotMgr::OnPlayerMoveWorldport(player);
    }
}
