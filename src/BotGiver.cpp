/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BotGiver.h"
#include "BotMgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "Unit.h"

BotGiverAI::BotGiverAI(Creature* creature) : BotAI(creature)
{
}

void BotGiverAI::UpdateBotAI(uint32 uiDiff)
{
    if (!UpdateVictim())
    {
        return;
    }

    DoMeleeAttackIfReady();
}

bool BotGiver::OnGossipHello(Player* player, Creature* botGiver)
{
    if (botGiver)
    {
        player->PlayerTalkClass->ClearMenus();

        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "I need your services.", GOSSIP_BOT_GIVER_SENDER_HIRE, GOSSIP_ACTION_INFO_DEF + 1);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind", GOSSIP_BOT_GIVER_SENDER_EXIT_MENU, GOSSIP_ACTION_INFO_DEF + 2);

        player->PlayerTalkClass->SendGossipMenu(GOSSIP_BOT_GIVER_GREET, botGiver->GetGUID());
    }

    return true;
}

bool BotGiver::OnGossipSelect(Player* player, Creature* botGiver, uint32 sender, uint32 action)
{
    player->PlayerTalkClass->ClearMenus();
    bool subMenu = false;
    uint32 gossipTextId = GOSSIP_BOT_GIVER_GREET;

    switch (sender)
    {
        case GOSSIP_BOT_GIVER_SENDER_EXIT_MENU: // exit
        {
            break;
        }

        case GOSSIP_BOT_GIVER_SENDER_MAIN_MENU: // return to main menu
        {
            return OnGossipHello(player, botGiver);
        }

        case GOSSIP_BOT_GIVER_SENDER_HIRE:
        {
            gossipTextId = GOSSIP_BOT_GIVER_HIRE;
            subMenu = true;

            for (uint8 botclass = BOT_CLASS_DREADLORD; botclass < BOT_CLASS_END; botclass++)
            {
                uint8 textId = botclass;

                switch (botclass)
                {
                    case BOT_CLASS_DREADLORD:
                    {
                        AddGossipItemFor(
                             player,
                             GOSSIP_ICON_BATTLE,
                             "Dreadlord",
                             GOSSIP_BOT_GIVER_SENDER_HIRE_CLASS,
                             GOSSIP_ACTION_INFO_DEF + botclass);

                        break;
                    }

                    case BOT_CLASS_DEATH_KNIGHT:
                    {
                        AddGossipItemFor(
                             player,
                             GOSSIP_ICON_BATTLE,
                             "Death knight",
                             GOSSIP_BOT_GIVER_SENDER_HIRE_CLASS,
                             GOSSIP_ACTION_INFO_DEF + botclass);

                        break;
                    }

                    case BOT_CLASS_LICH:
                    {
                        AddGossipItemFor(
                             player,
                             GOSSIP_ICON_BATTLE,
                             "Lich",
                             GOSSIP_BOT_GIVER_SENDER_HIRE_CLASS,
                             GOSSIP_ACTION_INFO_DEF + botclass);

                        break;
                    }

                    default:
                    {
                        textId = 0;
                        break;
                    }
                }

                if (!textId)
                {
                    continue;
                }
            }

            AddGossipItemFor(
                 player,
                 GOSSIP_ICON_CHAT,
                 "Nevermind",
                 GOSSIP_BOT_GIVER_SENDER_MAIN_MENU,
                 GOSSIP_ACTION_INFO_DEF + 1);

            break;
        }

        case GOSSIP_BOT_GIVER_SENDER_HIRE_CLASS:
        {
            gossipTextId = GOSSIP_BOT_GIVER_HIRE_CLASS;
            uint8 botClass = action - GOSSIP_ACTION_INFO_DEF;

            switch (botClass)
            {
                case BOT_CLASS_DREADLORD:
                {
                    LOG_WARN("npcbots", "★★★★ menu: [dreadload   ] selected! ★★★★");

                    Creature* dreadlord = player->SummonCreature(
                                                        BOT_DREADLORD,
                                                        *player,
                                                        TEMPSUMMON_MANUAL_DESPAWN);

                    if (dreadlord)
                    {
                        BotMgr::HireBot(player, dreadlord);
                    }

                    break;
                }

                case BOT_CLASS_DEATH_KNIGHT:
                {
                    // TODO: hire death knight
                    LOG_WARN("npcbots", "★★★★ menu: [death knight] selected! ★★★★");

                    break;
                }

                case BOT_CLASS_LICH:
                {
                    // TODO: hire lich
                    LOG_WARN("npcbots", "★★★★ menu: [lich        ] selected! ★★★★");

                    break;
                }
            }

            break;
        }
    }

    if (subMenu)
    {
        player->PlayerTalkClass->SendGossipMenu(gossipTextId, botGiver->GetGUID());
    }
    else
    {
        player->PlayerTalkClass->SendCloseGossip();
    }

    return true;
}
