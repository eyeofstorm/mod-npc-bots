/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _ACORE_HOOK_SCRIPT_H
#define _ACORE_HOOK_SCRIPT_H

#include "BotMgr.h"
#include "Creature.h"
#include "Chat.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Transport.h"

using namespace Acore::ChatCommands;

class UnitHookScript : public UnitScript
{
public:
    UnitHookScript() : UnitScript("npc_bots_unit_hook") { }

public:
    void OnAuraRemove(Unit* /*unit*/, AuraApplication* /*aurApp*/, AuraRemoveMode /*mode*/) override;
};

class PlayerHookScript : public PlayerScript
{
public:
    PlayerHookScript() : PlayerScript("npc_bots_player_hook") { }

    void OnLogin(Player* /*player*/) override;
    void OnLogout(Player* /*player*/) override;

    // Called when a player switches to a new area (more accurate than UpdateZone)
    void OnUpdateArea(Player* /*player*/, uint32 /*oldArea*/, uint32 /*newArea*/) override;

    // Called before a player is being teleported to new coords
    bool OnBeforeTeleport(Player* /*player*/, uint32 /*mapid*/, float /*x*/, float /*y*/, float /*z*/, float /*orientation*/, uint32 /*options*/, Unit* /*target*/) override;

    // Called when a player changes to a new level.
    void OnLevelChanged(Player* /*player*/, uint8 /*oldlevel*/) override;
};

class CreatureHookScript : public AllCreatureScript
{
public:
    CreatureHookScript() : AllCreatureScript("npc_bots_creature_hook") { }

    void OnAllCreatureUpdate(Creature* /*creature*/, uint32 /*diff*/) override;
};

class SpellHookScript : public SpellSC
{
public:
    SpellHookScript() : SpellSC("npc_bots_spell_hook") { }
    
public:
    void OnSpellGo(Unit const* /*caster*/, Spell const* /*spell*/, bool /*ok*/) override;
};

class BotCommandsScript : public CommandScript
{
public:
    BotCommandsScript() : CommandScript("npc_bot_commands_script") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable botCommandTable =
        {
            { "spawn",   HandleBotSpawnCommand,     SEC_MODERATOR, Console::Yes },
            { "hire",    HandleBotHireCommand,      SEC_MODERATOR, Console::Yes },
            { "dismiss", HandleBotDismissCommand,   SEC_MODERATOR, Console::Yes }
        };

        static ChatCommandTable commandTable =
        {
            { "bot", botCommandTable }
        };

        return commandTable;
    }

    // spawn bot
    static bool HandleBotSpawnCommand(ChatHandler* handler, uint32 entry)
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
        if (creatureTemplate->Entry < 9000000)
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
    static bool HandleBotHireCommand(ChatHandler* handler)
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

        if (bot->GetCreatureTemplate()->Entry < 9000000)
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
    static bool HandleBotDismissCommand(ChatHandler* handler)
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

        if (bot->GetCreatureTemplate()->Entry < 9000000)
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
};
#endif
