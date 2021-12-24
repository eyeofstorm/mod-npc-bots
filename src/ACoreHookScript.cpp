/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ACoreHookScript.h"
#include "BotMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "Player.h"
#include "ScriptMgr.h"
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
    PlayerBotsEntry* entry = sBotsRegistry->GetEntry(player);

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

void PlayerHookScript::OnLevelChanged(Player* player, uint8 oldlevel)
{
    if (player)
    {
        uint8 newLevel = player->getLevel();

        PlayerBotsEntry* entry = sBotsRegistry->GetEntry(player);
        
        if (entry)
        {
            BotsMap botsMap = entry->GetBots();

            for (BotsMap::iterator itr = botsMap.begin(); itr != botsMap.end(); itr++)
            {
                Creature const* bot = itr->second;

                BotMgr::SetBotLevel(const_cast<Creature*>(bot), newLevel);
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

    if (caster->GetTypeId() == TYPEID_UNIT)
    {
        Creature const* creature = caster->ToCreature();

        if (creature && creature->GetEntry() >= 9000000)
        {
            BotMgr::OnBotSpellGo(creature, spell, ok);
        }
    }
}
