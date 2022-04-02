/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ACoreHookScript.h"
#include "BotAI.h"
#include "BotMgr.h"
#include "Creature.h"
#include "MapMgr.h"
#include "Player.h"
#include "Spell.h"
#include "Transport.h"

/////////////////////////////////
// Azeroth core hook scripts here
/////////////////////////////////

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
void PlayerHookScript::OnUpdateArea(Player* player, uint32 /*oldAreaId*/, uint32 /*newAreaId*/)
{
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

void PlayerHookScript::OnLevelChanged(Player* player, uint8 /*oldlevel*/)
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
                    if (BotAI* ai = entry->GetBotAI())
                    {
                        ai->OnBotOwnerLevelChanged(newLevel, true);
                    }
                }
            }
        }
    }
}

bool CreatureHookScript::OnBeforeCreatureUpdate(Creature* creature, uint32 diff)
{
    if (creature)
    {
        if (creature->GetEntry() > BOT_ENTRY_BASE)
        {
            BotAI* ai = BotMgr::GetBotAI(creature);

            if (ai)
            {
                return ai->OnBeforeCreatureUpdate(diff);
            }
        }
    }

    return true;
}

void SpellHookScript::OnSpellGo(Spell const* spell, bool ok)
{
    if (!spell)
    {
        return;
    }

    Unit const* caster = spell->GetCaster();

    if (!caster)
    {
        return;
    }

    if (caster->GetTypeId() == TYPEID_PLAYER)
    {
        Player const* player = caster->ToPlayer();

        if (player)
        {
            SpellEntry const* spellEntry = sSpellStore.LookupEntry(spell->GetSpellInfo()->Id);

            if (spellEntry)
            {
                LOG_DEBUG(
                    "npcbots",
                    "player [{}] cast spell [{}: {}] {}...",
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
            if (creature->GetEntry() > BOT_ENTRY_BASE)
            {
                SpellEntry const* spellEntry = sSpellStore.LookupEntry(spell->GetSpellInfo()->Id);

                if (spellEntry)
                {
                    LOG_DEBUG(
                        "npcbots",
                        "bot [{}] cast spell [id: {} {}] {}...",
                        creature->GetName().c_str(),
                        spellEntry->Id,
                        spellEntry ? spellEntry->SpellName[sWorld->GetDefaultDbcLocale()] : "unkown",
                        ok ? "ok" : "failed");
                }

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

        LOG_INFO(
                "npcbots",
                "on player [{}] worldported to [{}, {}, {}].",
                player->GetName().c_str(),
                areaName.c_str(),
                zoneName.c_str(),
                mapName.c_str());

        BotMgr::OnBotOwnerMoveWorldport(player);
    }
}

void MovementHandlerHookScript::OnPlayerMoveTeleport(Player* player)
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

        LOG_INFO(
                "npcbots",
                "on player [{}] teleported to [{}, {}, {}].",
                player->GetName().c_str(),
                areaName.c_str(),
                zoneName.c_str(),
                mapName.c_str());

        BotMgr::OnBotOwnerMoveTeleport(player);
    }
}
