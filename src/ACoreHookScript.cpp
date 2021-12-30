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
void UnitHookScript::OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode mode)
{
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
    BotsEntry* entry = sBotsRegistry->GetEntry(player->GetGUID());

    if (entry)
    {
        BotsMap botsMap = entry->GetBots();

        for (BotsMap::iterator itr = botsMap.begin(); itr != botsMap.end(); itr++)
        {
            Creature const* bot = itr->second;

            BotMgr::DismissBot(const_cast<Creature*>(bot));
        }
    }
}

// Called when a player switches to a new area (more accurate than UpdateZone)
void PlayerHookScript::OnUpdateArea(Player* player, uint32 oldAreaId, uint32 newAreaId)
{
    if (player)
    {
        if (strcmp(player->GetName().c_str(), "Felthas") == 0)
        {
            std::string mapName = player->FindMap() ? player->FindMap()->GetMapName() : "unknown map";
            std::string zoneName = "unknown zone";
            std::string oldAreaName = "unknown area";
            std::string newAreaName = "unknown area";
            LocaleConstant locale = sWorld->GetDefaultDbcLocale();

            if (AreaTableEntry const* zone = sAreaTableStore.LookupEntry(player->GetZoneId()))
            {
                zoneName = zone->area_name[locale];
            }

            if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(oldAreaId))
            {
                oldAreaName = area->area_name[locale];
            }

            if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(newAreaId))
            {
                newAreaName = area->area_name[locale];
            }

            LOG_DEBUG(
                    "npcbots",
                    "player [%s] switches from [%s] to [%s, %s, %s].",
                    player->GetName().c_str(),
                    oldAreaName.c_str(),
                    newAreaName.c_str(),
                    zoneName.c_str(),
                    mapName.c_str());

            if (player->GetDisplayId() != 27545)
            {
                player->SetDisplayId(27545);
            }
        }
    }
}

bool PlayerHookScript::OnBeforeTeleport(
                            Player* player,
                            uint32 mapid,
                            float x, float y, float z, float orientation,
                            uint32 options, Unit* target)
{
    if (player)
    {
        BotsEntry* entry = sBotsRegistry->GetEntry(player->GetGUID());

        if (entry)
        {
            BotsMap botsMap = entry->GetBots();

            for (BotsMap::iterator itr = botsMap.begin(); itr != botsMap.end(); itr++)
            {
                Creature const* bot = itr->second;

                BotMgr::DismissBot(const_cast<Creature*>(bot));
            }
        }

        uint32 areaId, zoneId;
        std::string mapName = "unknown map";
        std::string zoneName = "unknown zone";
        std::string areaName = "unknown area";
        LocaleConstant locale = sWorld->GetDefaultDbcLocale();

        Map* map = sMapMgr->FindBaseMap(mapid);

        if (map)
        {
            mapName = map->GetMapName();

            map->GetZoneAndAreaId(player->GetPhaseMask(), zoneId, areaId, x, y, z);

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
                "player [%s] before teleport to [%s, %s, %s].",
                player->GetName().c_str(),
                areaName.c_str(),
                zoneName.c_str(),
                mapName.c_str());

        if (strcmp(player->GetName().c_str(), "Felthas") == 0)
        {
            if (player->GetDisplayId() != 27545)
            {
                player->SetDisplayId(27545);
            }
        }
    }

    return true;
}

void PlayerHookScript::OnLevelChanged(Player* player, uint8 oldlevel)
{
    if (player)
    {
        uint8 newLevel = player->getLevel();

        BotsEntry* entry = sBotsRegistry->GetEntry(player->GetGUID());

        if (entry)
        {
            BotsMap botsMap = entry->GetBots();

            for (BotsMap::iterator itr = botsMap.begin(); itr != botsMap.end(); itr++)
            {
                Creature const* bot = itr->second;

                BotMgr::SetBotLevel(const_cast<Creature*>(bot), newLevel, false);
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
            BotAI* ai = BotMgr::GetBotAI(creature);

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
                    "[%s] cast spell [%u: %s] %s...",
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
                    "[%s] cast spell [id: %u %s] %s...",
                    creature->GetName().c_str(),
                    spellEntry->Id,
                    spellEntry ? spellEntry->SpellName[sWorld->GetDefaultDbcLocale()] : "unkown",
                    ok ? "ok" : "failed");

                BotMgr::OnBotSpellGo(creature, spell, ok);
            }
        }
    }
}
