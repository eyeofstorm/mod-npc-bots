/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_MGR_H
#define _BOT_MGR_H

#include "BotAI.h"
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
            LOG_ERROR(
                "npcbots",
                "invalid parameter [bot] of function PlayerBotsEntry::GetEntry(Player const*)");

            return nullptr;
        }

        return m_registry[player->GetGUID()];
    }

    PlayerBotsEntry* GetEntry(Creature const* bot)
    {
        if (!bot)
        {
            LOG_ERROR(
                "npcbots",
                "invalid parameter [bot] of function PlayerBotsEntry::GetEntry(Creature const*)");

            return nullptr;
        }

        if (bot->GetOwnerGUID() == ObjectGuid::Empty)
        {
            LOG_ERROR("npcbots", "bot [%s] has no owner.", bot->GetName().c_str());
            return nullptr;
        }

        PlayerBotsEntry* entry = m_registry[bot->GetOwnerGUID()];

        return entry;
    }

    BotRegisterResult RegisterBot(Creature const* bot)
    {
        if (bot->GetOwnerGUID() == ObjectGuid::Empty)
        {
            LOG_ERROR("npcbots", "bot [%s] has no owner.", bot->GetName().c_str());
            return BOT_HAS_NO_OWNER;
        }

        PlayerBotsEntry* entry = GetEntry(bot);

        if (!entry)
        {
            Player* player = ObjectAccessor::FindPlayer(bot->GetOwnerGUID());

            LOG_DEBUG("npcbots", "player [%s] not exist in bot registry. add one...", player->GetName().c_str());

            entry = new PlayerBotsEntry(player);
            m_registry[player->GetGUID()] = entry;

            LOG_DEBUG("npcbots", "add entry to bot registry...ok!");
        }

        BotsMap bots = entry->GetBots();

        if (bots[bot->GetGUID()])
        {
            LOG_WARN("npcbots", "bot [%s] already exist in bot registry. skip...", bot->GetName().c_str());
            return BOT_ALREADY_REGISTERED;
        } 
        else 
        {
            LOG_DEBUG("npcbots", "start add bot [%s] to player...", bot->GetName().c_str());
            entry->AddBot(bot);
            LOG_DEBUG("npcbots", "end add bot [%s] to player...", bot->GetName().c_str());

            return BOT_REGISTER_SUCCESS;
        }
    }

    BotRegisterResult UnregisterBot(Player const* owner, Creature const* bot)
    {
        LOG_DEBUG("npcbots", "begin unregister bot [%s]", bot->GetName().c_str());

        if (!owner)
        {
            LOG_ERROR("npcbots", "end unregister bot [%s]. bot has no owner...", bot->GetName().c_str());

            return BOT_HAS_NO_OWNER;
        }

        PlayerBotsEntry* entry = GetEntry(owner);

        LOG_DEBUG("npcbots", "start remove bot from player [%s]...", owner->GetName().c_str());
        entry->RemoveBot(bot);
        LOG_DEBUG("npcbots", "end remove bot from player [%s]...", owner->GetName().c_str());

        if (entry->GetBots().empty())
        {
            LOG_DEBUG("npcbots", "player has no bots. so delete player bots entry from player bots registry too.");
            m_registry.erase(owner->GetGUID());
            delete entry;
        }

        LOG_DEBUG("npcbots", "end unregister bot [%s]. bot unregister success...", bot->GetName().c_str());

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
    static void SetBotLevel(Creature* /*bot*/, uint8 /*level*/, bool showLevelChange = true);

    static int GetPlayerBotsCount(Player* player);

    //onEvent hooks
    static void OnBotSpellGo(Unit const* caster, Spell const* spell, bool ok = true);

private:
    static void CleanupsBeforeBotRemove(Player* /*owner*/, Creature* /*bot*/);
};

#endif //_BOT_MGR_H 
