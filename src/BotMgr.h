/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_MGR_H
#define _BOT_MGR_H

#include "BotAI.h"
#include "BotPetAI.h"
#include "Creature.h"
#include "Map.h"
#include "Player.h"

enum BotRegisterResult
{
    BOT_REGISTER_SUCCESS        = 0, 
    BOT_ALREADY_REGISTERED      = 1, 
    BOT_HAS_NO_OWNER            = 2, 
    BOT_UNREGISTER_SUCCESS      = 3,
    BOT_NOT_FOUND               = 4
};

typedef std::map<ObjectGuid, Creature const*> BotsMap;
class PlayerBotsRegistry;

class PlayerBotsEntry
{
    friend class PlayerBotsRegistry;

public:
    BotsMap GetBots() const
    {
        return m_bots;
    }

    Player const* GetBotsOwner()
    {
        return m_owner;
    }

protected:
    explicit PlayerBotsEntry(Player const* player) : m_owner(player) { }

protected:
    void AddBot(Creature const* bot) { m_bots[bot->GetGUID()] = bot; }
    void RemoveBot(Creature const* bot) { m_bots.erase(bot->GetGUID()); }

private:
    Player const*   m_owner;
    BotsMap         m_bots;
};

class PlayerBotsRegistry
{
protected:
    explicit PlayerBotsRegistry() { }

public:
    static PlayerBotsRegistry* instance()
    {
        static PlayerBotsRegistry instance;
        return &instance; 
    }

    PlayerBotsEntry* GetEntry(Player const* player)
    {
        if (!player)
        {
            return nullptr;
        }

        return m_registry[player->GetGUID()];
    }

    PlayerBotsEntry* GetEntry(Creature const* bot)
    {
        if (!bot)
        {
            return nullptr;
        }

        if (!bot->GetOwner())
        {
            return nullptr;
        }

        PlayerBotsEntry* entry = m_registry[bot->GetOwner()->GetGUID()];

        return entry;
    }

    BotRegisterResult RegisterBot(Creature const* bot)
    {
        if (!bot->GetOwner())
        {
            LOG_ERROR("npcbots", "bot has no owner.");
            return BOT_HAS_NO_OWNER;
        }

        PlayerBotsEntry* entry = GetEntry(bot);

        if (!entry)
        {
            LOG_DEBUG("npcbots", "bot not exist in bot registry. create one...");
            Player* player = ObjectAccessor::FindPlayer(bot->GetOwner()->GetGUID());

            LOG_DEBUG("npcbots", "create player bots entry for [%s]", player->GetGUID().ToString().c_str());
            entry = new PlayerBotsEntry(player);

            LOG_DEBUG("npcbots", "create OK!!");
            m_registry[player->GetGUID()] = entry;
            LOG_DEBUG("npcbots", "add entry to bot registry......OK!");
        }
    
        BotsMap bots = entry->GetBots();

        if (bots[bot->GetGUID()])
        {
            LOG_DEBUG("npcbots", "bot already exist in bot registry. skip...");
            return BOT_ALREADY_REGISTERED;
        } 
        else 
        {
            LOG_DEBUG("npcbots", "start add bot to player...");
            entry->AddBot(bot);
            LOG_DEBUG("npcbots", "end add bot to player...");

            return BOT_REGISTER_SUCCESS;
        }
    }

    BotRegisterResult UnregisterBot(Creature const* bot)
    {
        PlayerBotsEntry* entry = GetEntry(bot);

        if (!entry)
        {
            return BOT_NOT_FOUND;
        }

        LOG_DEBUG("npcbots", "start remove bot from player...");
        entry->RemoveBot(bot);
        LOG_DEBUG("npcbots", "end remove bot from player...");

        if (entry->GetBots().empty())
        {
            LOG_DEBUG("npcbots", "player has no bot. so delete player bots entry from bots registry too.");
            m_registry.erase(bot->GetOwner()->GetGUID());
            delete entry;
        }

        return BOT_UNREGISTER_SUCCESS;
    }

private:
    std::map<ObjectGuid, PlayerBotsEntry*> m_registry;
};

#define sBotsRegistry PlayerBotsRegistry::instance()

class BotMgr
{
protected:
    BotMgr() { }

public:
    static void PlayerHireBot(Player* /*owner*/, Creature* /*bot*/);
    static bool DismissBot(Creature* /*bot*/);

    static BotAI* GetBotAI(Creature* /*bot*/);
    static void SetBotLevel(Creature* /*bot*/, uint8 /*level*/);

    static int GetPlayerBotsCount(Player* player);

    //onEvent hooks
    static void OnBotSpellGo(Unit const* caster, Spell const* spell, bool ok = true);

private:
    static void CleanupsBeforeBotRemove(Player* /*owner*/, Creature* /*bot*/);
};

#endif //_BOT_MGR_H 
