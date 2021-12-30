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
#include "Unit.h"

enum BotRegisterResult
{
    BOT_REGISTER_SUCCESS        = 0, 
    BOT_ALREADY_REGISTERED      = 1, 
    BOT_HAS_NO_OWNER            = 2, 
    BOT_UNREGISTER_SUCCESS      = 3,
    BOT_NOT_FOUND               = 4
};

typedef std::map<ObjectGuid, Creature const*> BotsMap;
class BotsRegistry;

class BotsEntry
{
    friend class BotsRegistry;

public:
    BotsMap GetBots() const
    {
        return m_bots;
    }

    Unit const* GetBotsOwner()
    {
        return m_owner;
    }

protected:
    explicit BotsEntry(Unit const* owner) : m_owner(owner) { }

protected:
    void AddBot(Creature const* bot) { m_bots[bot->GetGUID()] = bot; }
    void RemoveBot(Creature const* bot) { m_bots.erase(bot->GetGUID()); }

private:
    Unit const*     m_owner;
    BotsMap         m_bots;
};

class BotsRegistry
{
protected:
    explicit BotsRegistry() { }

public:
    static BotsRegistry* instance()
    {
        static BotsRegistry instance;
        return &instance; 
    }

    BotsEntry* GetEntry(ObjectGuid ownerGuid)
    {
        return m_registry[ownerGuid];
    }

    BotsEntry* GetEntry(Creature const* bot)
    {
        if (!bot)
        {
            return nullptr;
        }

        if (bot->GetOwnerGUID() == ObjectGuid::Empty)
        {
            return nullptr;
        }

        BotsEntry* entry = m_registry[bot->GetOwnerGUID()];

        return entry;
    }

    BotRegisterResult RegisterBot(Unit const* owner, Creature const* bot)
    {
        if (bot->GetOwnerGUID() == ObjectGuid::Empty)
        {
            LOG_ERROR("npcbots", "bot [%s] has no owner.", bot->GetName().c_str());
            return BOT_HAS_NO_OWNER;
        }

        BotsEntry* entry = GetEntry(bot);

        if (!entry)
        {
            LOG_DEBUG("npcbots", "owner [%s] not exist in bot registry. add one...", owner->GetName().c_str());

            entry = new BotsEntry(owner);
            m_registry[owner->GetGUID()] = entry;

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
            LOG_DEBUG("npcbots", "start add bot [%s] to owner [%s]...", bot->GetName().c_str(), owner->GetName().c_str());
            entry->AddBot(bot);
            LOG_DEBUG("npcbots", "end add bot [%s] to owner [%s]...", bot->GetName().c_str(), owner->GetName().c_str());

            return BOT_REGISTER_SUCCESS;
        }
    }

    BotRegisterResult UnregisterBot(Unit const* owner, Creature const* bot)
    {
        LOG_DEBUG("npcbots", "begin unregister bot [%s]", bot->GetName().c_str());

        if (!owner)
        {
            LOG_ERROR("npcbots", "end unregister bot [%s]. bot has no owner...", bot->GetName().c_str());

            return BOT_HAS_NO_OWNER;
        }

        BotsEntry* entry = GetEntry(owner->GetGUID());

        LOG_DEBUG("npcbots", "start remove bot from owner [%s]...", owner->GetName().c_str());
        entry->RemoveBot(bot);
        LOG_DEBUG("npcbots", "end remove bot from owner [%s]...", owner->GetName().c_str());

        if (entry->GetBots().empty())
        {
            LOG_DEBUG("npcbots", "owner has no bots. delete entry from bots registry too.");
            m_registry.erase(owner->GetGUID());
            delete entry;
        }

        LOG_DEBUG("npcbots", "end unregister bot [%s]. bot unregister success...", bot->GetName().c_str());

        return BOT_UNREGISTER_SUCCESS;
    }

private:
    std::map<ObjectGuid, BotsEntry*> m_registry;
};

#define sBotsRegistry BotsRegistry::instance()

class BotMgr
{
protected:
    BotMgr() { }

public:
    static void HireBot(Unit* /*owner*/, Creature* /*bot*/);
    static bool DismissBot(Creature* /*bot*/);

    static BotAI* GetBotAI(Creature const* /*bot*/);
    static Unit const* GetBotOwner(Creature* /*bot*/);
    static void SetBotLevel(Creature* /*bot*/, uint8 /*level*/, bool showLevelChange = true);
    static int GetBotsCount(Unit* owner);

    //onEvent hooks
    static void OnBotSpellGo(Unit const* caster, Spell const* spell, bool ok = true);

private:
    static void CleanupsBeforeBotRemove(Unit* /*owner*/, Creature* /*bot*/);
};

#endif //_BOT_MGR_H 
