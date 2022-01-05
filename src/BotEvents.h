/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_EVENTS_H
#define _BOT_EVENTS_H

#include "BotAI.h"
#include "BotMgr.h"
#include "Creature.h"
#include "EventProcessor.h"

//class TeleportHomeEvent : public BasicEvent
//{
//    friend class BotMgr;
//
//    protected:
//        TeleportHomeEvent(Creature* bot) : m_bot(bot) {}
//        ~TeleportHomeEvent() {}
//
//        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/)
//        {
//            BotMgr::TeleportHome(m_bot);
//            return true;
//        }
//
//    private:
//        Creature* m_bot;
//};

class TeleportFinishEvent : public BasicEvent
{
    friend class BotMgr;

    protected:
        TeleportFinishEvent(Creature* bot, BotAI* botAI) : m_bot(bot), m_botAI(botAI) {  }
        ~TeleportFinishEvent() {  }

        //Execute is always called while creature is out of world so ai is never deleted
        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/)
        {
            return BotMgr::FinishTeleport(m_bot, m_botAI);
        }

    private:
        Creature* m_bot;
        BotAI* m_botAI;
};

#endif  //_BOT_EVENTS_H
