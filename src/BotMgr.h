/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_MGR_H
#define _BOT_MGR_H

#include "BotCommon.h"

#include <mutex>

class BotEntry;
class BotMgr;
class BotsRegistry;
class BotAI;
class Creature;
class Map;
class Player;
class Unit;

typedef std::map<ObjectGuid, BotEntry*> BotEntryMap;

class BotEntry
{
    friend class BotsRegistry;

private:
    explicit BotEntry(BotAI* ai)
    {
        m_botAI = ai;
    }

public:
    virtual ~BotEntry() {  }

    bool operator < (const BotEntry& other) const
    {
        return m_botAI->GetBot()->GetGUID() < other.m_botAI->GetBot()->GetGUID();
    }

public:
    BotAI* GetBotAI() const { return m_botAI; }
    Unit* GetBotOwner() const {return m_botAI->GetBotOwner(); }
    Creature* GetBot() const { return m_botAI->GetBot(); }
    Creature* GetPet() const { return m_botAI->GetPet(); }

    bool IsFreeBot() const { return m_botAI->IAmFree(); }

private:
    BotAI* m_botAI;
};

class BotsRegistry
{
    friend class BotMgr;

protected:
    explicit BotsRegistry()
    {
        m_botRegistry.clear();
    }

public:
    static BotsRegistry* instance()
    {
        static BotsRegistry instance;
        return &instance;
    }

public:
    void Register(BotAI* ai);
    void Unregister(BotAI* ai);
    BotEntry* GetEntry(Creature const* bot);
    BotEntryMap GetEntryByOwnerGUID(ObjectGuid ownerGUID);
    Creature* FindFirstBot(uint32 creatureTemplateEntry);

public:
    void LogBotRegistryEntries();

private:
    void lock()
    {
        m_lock.lock();
    }

    void unlock()
    {
        m_lock.unlock();
    }

private:
    std::mutex m_lock;

    BotEntryMap m_botRegistry;
};

#define sBotsRegistry BotsRegistry::instance()

class BotMgr
{
protected:
    BotMgr() { }

public:
    static void HireBot(Player* /*owner*/, Creature* /*bot*/);
    static bool DismissBot(Creature* /*bot*/);

    static BotAI const* GetBotAI(Creature const* /*bot*/);
    static int GetBotsCount(Unit* owner);
    static void SetBotLevel(Creature* /*bot*/, uint8 /*level*/, bool showLevelChange = true);

    //onEvent hooks
    static void OnBotSpellGo(Unit const* caster, Spell const* spell, bool ok = true);
    static void OnPlayerMoveWorldport(Player* player);
    static void OnPlayerMoveTeleport(Player* player);
    static bool RestrictBots(Creature const* bot, bool add);
    static bool TeleportBot(Creature* bot, Map* newMap, float x, float y, float z, float ori);

private:
    static void CleanupsBeforeBotDismiss(Creature* /*bot*/);
};

#endif //_BOT_MGR_H 
