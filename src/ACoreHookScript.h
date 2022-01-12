/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _ACORE_HOOK_SCRIPT_H
#define _ACORE_HOOK_SCRIPT_H

#include "Chat.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

class UnitHookScript : public UnitScript
{
public:
    UnitHookScript() : UnitScript("npc_bots_unit_hook") { }
};

class PlayerHookScript : public PlayerScript
{
public:
    PlayerHookScript() : PlayerScript("npc_bots_player_hook") { }

    void OnLogin(Player* /*player*/) override;
    void OnLogout(Player* /*player*/) override;

    // Called when a player switches to a new area (more accurate than UpdateZone)
    void OnUpdateArea(Player* /*player*/, uint32 /*oldArea*/, uint32 /*newArea*/) override;

    // Called when a player changes to a new level.
    void OnLevelChanged(Player* /*player*/, uint8 /*oldlevel*/) override;
};

class CreatureHookScript : public AllCreatureScript
{
public:
    CreatureHookScript() : AllCreatureScript("npc_bots_creature_hook") { }

public:
    void OnAllCreatureUpdate(Creature* /*creature*/, uint32 /*diff*/) override;
};

class SpellHookScript : public SpellSC
{
public:
    SpellHookScript() : SpellSC("npc_bots_spell_hook") { }
    
public:
    void OnSpellGo(Unit const* /*caster*/, Spell const* /*spell*/, bool /*ok*/) override;
};

class MovementHandlerHookScript : public MovementHandlerScript
{
public:
    MovementHandlerHookScript() : MovementHandlerScript("npc_bots_movement_handler_hook") { }

public:
    void OnPlayerMoveWorldport(Player* /*player*/) override;
    void OnPlayerMoveTeleport(Player* /*player*/) override;
};

class BotCommandsScript : public CommandScript
{
public:
    BotCommandsScript() : CommandScript("npc_bot_commands_script") { }

    ChatCommandTable GetCommands() const override;

    static bool HandleBotSpawnCommand(ChatHandler* /*handler*/, uint32 /*entry*/);
    static bool HandleBotHireCommand(ChatHandler* /*handler*/);
    static bool HandleBotDismissCommand(ChatHandler* /*handler*/);
    static bool HandleBotMoveCommand(ChatHandler* /*handler*/, uint32 /*creatureTemplateEntry*/);
};

#endif  // _ACORE_HOOK_SCRIPT_H
