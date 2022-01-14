/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_EVENTS_H
#define _BOT_EVENTS_H

#include "EventProcessor.h"

class TeleportFinishEvent : public BasicEvent
{
    friend class BotAI;
    friend class BotMgr;

    protected:
        TeleportFinishEvent(BotAI* botAI) : m_botAI(botAI) {  }
        ~TeleportFinishEvent() {  }

        // Execute is always called while creature is out of world so ai is never deleted
        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/)
        {
            return m_botAI->BotFinishTeleport();
        }

    private:
        BotAI* m_botAI;
};

#endif  //_BOT_EVENTS_H
