/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_PET_AI_H
#define _BOT_PET_AI_H

#include "CreatureAI.h"

class BotPetAI : public CreatureAI
{
public:
    virtual ~BotPetAI();

protected:
    explicit BotPetAI(Creature* creature);
};

#endif // _BOT_PET_AI_H