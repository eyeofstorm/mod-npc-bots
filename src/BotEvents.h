/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_EVENTS_H
#define _BOT_EVENTS_H

#include "BotAI.h"
#include "BotMgr.h"
#include "EventProcessor.h"

class TeleportHomeEvent : public BasicEvent
{
    friend class BotAI;

    protected:
        TeleportHomeEvent(BotAI* ai) : _ai(ai) {}
        ~TeleportHomeEvent() {}

        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/)
        {
            _ai->TeleportHome();
            return true;
        }

    private:
        BotAI* _ai;
};

//Delayed teleport finish: adds bot back to world on new location
class TeleportFinishEvent : public BasicEvent
{
    friend class BotAI;
    friend class BotMgr;

    protected:
        TeleportFinishEvent(BotAI* ai) : _ai(ai) {}
        ~TeleportFinishEvent() {}

        //Execute is always called while creature is out of world so ai is never deleted
        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/)
        {
            _ai->FinishTeleport();
            return true;
        }

    private:
        BotAI* _ai;
};

#endif  //_BOT_EVENTS_H
