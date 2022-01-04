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

class BotEntry;
class BotsRegistry;
class BotMgr;

typedef std::map<ObjectGuid, BotEntry*> BotEntryMap;

class BotEntry
{
    friend class BotsRegistry;

private:
    explicit BotEntry(BotAI* ai)
    {
        ASSERT(ai != nullptr);

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

    bool IsFreeBot() const { return m_botAI->GetBot()->GetOwnerGUID() == ObjectGuid::Empty; }

private:
    BotAI* m_botAI;
};

class BotsRegistry
{
    friend class BotMgr;

protected:
    explicit BotsRegistry()
    {
        m_freeBotRegistry.clear();
        m_hiredBotRegistry.clear();
    }

public:
    static BotsRegistry* instance()
    {
        static BotsRegistry instance;
        return &instance;
    }

public:
    void RegisterOrUpdate(BotAI* ai);
    void Unregister(BotAI* ai);
    BotEntry* GetEntry(Creature const* bot);
    BotEntryMap GetEntryByOwnerGUID(ObjectGuid ownerGUID);
    Creature* FindFirstBot(uint32 creatureTemplateEntry);

private:
    BotEntryMap m_freeBotRegistry;
    BotEntryMap m_hiredBotRegistry;
};

#define sBotsRegistry BotsRegistry::instance()

class BotMgr
{
protected:
    BotMgr() { }

public:
    static void HireBot(Unit* /*owner*/, Creature* /*bot*/);
    static bool DismissBot(Creature* /*bot*/);

    static BotAI const* GetBotAI(Creature const* /*bot*/);
    static int GetBotsCount(Unit* owner);
    static void SetBotLevel(Creature* /*bot*/, uint8 /*level*/, bool showLevelChange = true);

    //onEvent hooks
    static void OnBotSpellGo(Unit const* caster, Spell const* spell, bool ok = true);
    static void OnPlayerMoveWorldport(Player* player);

    static EventProcessor *extracted(BotAI *oldAI);
    
    static void TeleportBot(Creature* bot, Map* newMap, float x, float y, float z, float ori);
    static bool RestrictBots(Creature const* bot, bool add);

private:
    static void CleanupsBeforeBotRemove(Creature* /*bot*/);
};

#endif //_BOT_MGR_H 
