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

ChatCommandTable BotCommandsScript::GetCommands() const
{
    static ChatCommandTable botCommandTable =
    {
        { "spawn",   HandleBotSpawnCommand,     SEC_MODERATOR, Console::Yes },
        { "hire",    HandleBotHireCommand,      SEC_MODERATOR, Console::Yes },
        { "dismiss", HandleBotDismissCommand,   SEC_MODERATOR, Console::Yes },
        { "move",    HandleBotMoveCommand,      SEC_MODERATOR, Console::Yes }
    };

    static ChatCommandTable commandTable =
    {
        { "bot", botCommandTable }
    };

    return commandTable;
}

// spawn bot
bool BotCommandsScript::HandleBotSpawnCommand(ChatHandler* handler, uint32 entry)
{
    WorldSession* session = handler->GetSession();
    Player* owner = session->GetPlayer();

    const CreatureTemplate* creatureTemplate = sObjectMgr->GetCreatureTemplate(entry);

    if (!creatureTemplate)
    {
        handler->PSendSysMessage("entry %u is not defined in creature_template table!", entry);
        handler->SetSentErrorMessage(true);

        return false;
    }

    // check if this is a hireable bot.
    if (creatureTemplate->Entry <= BOT_ENTRY_BASE)
    {
        handler->PSendSysMessage("creature %u is not a bot!", entry);
        handler->SetSentErrorMessage(true);

        return false;
    }

    //////////////////////////////////
    // spawn bot at player's position.
    //////////////////////////////////

    Map* map = owner->GetMap();

    float x = owner->GetPositionX();
    float y = owner->GetPositionY();
    float z = owner->GetPositionZ();
    float o = owner->GetOrientation();

    if (Transport* tt = owner->GetTransport())
    {
        if (MotionTransport* trans = tt->ToMotionTransport())
        {
            ObjectGuid::LowType guid = sObjectMgr->GenerateCreatureSpawnId();
            CreatureData& data = sObjectMgr->NewOrExistCreatureData(guid);

            data.id = entry;
            data.posX = x;
            data.posY = y;
            data.posZ = z;
            data.orientation = o;

            Creature* creature = trans->CreateNPCPassenger(guid, &data);

            creature->SaveToDB(
                        trans->GetGOInfo()->moTransport.mapID,
                        1 << map->GetSpawnMode(),
                        owner->GetPhaseMaskForSpawn());

            sObjectMgr->AddCreatureToGrid(guid, &data);

            return true;
        }
    }

    Creature* bot = new Creature();

    if (
        !bot->Create(
                map->GenerateLowGuid<HighGuid::Unit>(),
                map,
                owner->GetPhaseMaskForSpawn(),
                entry,
                0,
                x, y, z, o)
        )
    {
        delete bot;
        return false;
    }

    bot->SetDefaultMovementType(RANDOM_MOTION_TYPE);
    bot->SetWanderDistance(4.0f);
    bot->SetRespawnDelay(1);

    bot->SaveToDB(
            map->GetId(),
            (1 << map->GetSpawnMode()),
            owner->GetPhaseMaskForSpawn());

    ObjectGuid::LowType spawnId = bot->GetSpawnId();

    bot->CleanupsBeforeDelete();
    delete bot;

    bot = new Creature();

    if (!bot->LoadCreatureFromDB(spawnId, map, true, false, false))
    {
        delete bot;
        return false;
    }

    sObjectMgr->AddCreatureToGrid(spawnId, sObjectMgr->GetCreatureData(spawnId));

    handler->SendSysMessage("bot successfully spawned.");

    return true;
}

// hire bot
bool BotCommandsScript::HandleBotHireCommand(ChatHandler* handler)
{
    WorldSession* session = handler->GetSession();
    Player* owner = session->GetPlayer();
    Unit* unit = owner->GetSelectedUnit();

    if (!unit)
    {
        handler->SendSysMessage("please selete the bot you want to hire.");
        handler->SetSentErrorMessage(true);

        return false;
    }

    Creature* bot = unit->ToCreature();

    if (!bot)
    {
        handler->SendSysMessage("for some reason, you can not hire this bot.");
        handler->SetSentErrorMessage(true);

        return false;
    }

    if (bot->GetCreatureTemplate()->Entry <= BOT_ENTRY_BASE)
    {
        handler->SendSysMessage("the creature that you seleted is not a hireable bot.");
        handler->SetSentErrorMessage(true);

        return false;
    }

    BotMgr::HireBot(owner, bot);

    handler->SendSysMessage("bot successfully added.");

    return true;
}

// dismiss bot
bool BotCommandsScript::HandleBotDismissCommand(ChatHandler* handler)
{
    WorldSession* session = handler->GetSession();
    Player* owner = session->GetPlayer();
    Unit* selectedUnit = owner->GetSelectedUnit();

    if (!selectedUnit)
    {
        handler->SendSysMessage("please selete the bot you want to dismiss.");
        handler->SetSentErrorMessage(true);

        return false;
    }

    Creature* bot = selectedUnit->ToCreature();

    if (!bot)
    {
        handler->SendSysMessage("for some reason, you can not dismiss this bot.");
        handler->SetSentErrorMessage(true);

        return false;
    }

    if (bot->GetCreatureTemplate()->Entry <= BOT_ENTRY_BASE)
    {
        handler->SendSysMessage("the creature that you seleted is not a bot.");
        handler->SetSentErrorMessage(true);

        return false;
    }

    if (BotMgr::DismissBot(bot))
    {
        handler->SendSysMessage("bot successfully dismissed.");

        return true;
    }
    else
    {
        handler->SendSysMessage("bot dismiss failed.");
        handler->SetSentErrorMessage(true);

        return false;
    }
}

// move bot to player position
bool BotCommandsScript::HandleBotMoveCommand(ChatHandler* handler, uint32 creatureTemplateEntry)
{
    WorldSession* session = handler->GetSession();
    Player* player = session->GetPlayer();

    const CreatureTemplate* creatureTemplate = sObjectMgr->GetCreatureTemplate(creatureTemplateEntry);

    if (!creatureTemplate)
    {
        handler->PSendSysMessage("entry %u is not defined in creature_template table!", creatureTemplateEntry);
        handler->SetSentErrorMessage(true);

        return false;
    }

    // check if this is a hireable bot.
    if (creatureTemplate->Entry <= BOT_ENTRY_BASE)
    {
        handler->PSendSysMessage("creature(entry: %u) is not a bot!", creatureTemplateEntry);
        handler->SetSentErrorMessage(true);

        return false;
    }

    Creature *bot = sBotsRegistry->FindFirstBot(creatureTemplateEntry);

    if (!bot)
    {
        handler->PSendSysMessage("bot(entry: %u) is not spawned!", creatureTemplateEntry);
        handler->SetSentErrorMessage(true);

        return false;
    }

    uint32 lowguid = bot->GetSpawnId();
    CreatureData const* data = sObjectMgr->GetCreatureData(lowguid);

    if (!data)
    {
        handler->PSendSysMessage(LANG_COMMAND_CREATGUIDNOTFOUND, lowguid);
        handler->SetSentErrorMessage(true);

        return false;
    }

    CreatureData* cdata = const_cast<CreatureData*>(data);

    Position spawnPoint = player->GetPosition();
    spawnPoint.GetPosition(cdata->posX, cdata->posY, cdata->posZ, cdata->orientation);
    cdata->mapid = player->GetMapId();

    WorldDatabase.PExecute(
        "UPDATE creature SET position_x = %.3f, position_y = %.3f, position_z = %.3f, orientation = %.3f, map = %u WHERE guid = %u",
        cdata->posX, cdata->posY, cdata->posZ, cdata->orientation, uint32(cdata->mapid), lowguid);

    if (BotMgr::GetBotAI(bot)->IAmFree() && bot->IsInWorld() && !bot->IsInCombat() && bot->IsAlive())
    {
        BotMgr::TeleportBot(
                    bot,
                    player->GetMap(),
                    spawnPoint.m_positionX,
                    spawnPoint.m_positionY,
                    spawnPoint.m_positionZ,
                    spawnPoint.m_orientation);

        LOG_DEBUG(
              "npcbots",
              "bot [%s] was moved to player [%s].",
              bot->GetName().c_str(),
              player->GetName().c_str());

        handler->PSendSysMessage(
                     "bot [%s] was moved to player [%s].",
                     bot->GetName().c_str(),
                     player->GetName().c_str());

        return true;
    }

    LOG_DEBUG(
          "npcbots",
          "bot [%s] cannot moved to player [%s]. IAmFree: %s, IsInWorld: %s, IsInCombat %s, IsAlive: %s",
          bot->GetName().c_str(),
          player->GetName().c_str(),
          BotMgr::GetBotAI(bot)->IAmFree() ? "true" : "false",
          bot->IsInWorld() ? "true" : "false",
          bot->IsInCombat() ? "true" : "false",
          bot->IsAlive() ? "true" : "false");

    handler->PSendSysMessage(
                 "bot [%s] cannot moved to player [%s].",
                 bot->GetName().c_str(),
                 player->GetName().c_str());

    handler->SetSentErrorMessage(true);

    return false;
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
        if (creature->GetEntry() > BOT_ENTRY_BASE)
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
            if (creature->GetEntry() > BOT_ENTRY_BASE)
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

        LOG_INFO(
                "npcbots",
                "☆☆☆ on player [%s] worldported to [%s, %s, %s].",
                player->GetName().c_str(),
                areaName.c_str(),
                zoneName.c_str(),
                mapName.c_str());

        BotMgr::OnPlayerMoveWorldport(player);
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
                "☆☆☆ on player [%s] teleported to [%s, %s, %s].",
                player->GetName().c_str(),
                areaName.c_str(),
                zoneName.c_str(),
                mapName.c_str());

        BotMgr::OnPlayerMoveTeleport(player);
    }
}
