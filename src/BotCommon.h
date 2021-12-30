/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_COMMON_H
#define _BOT_COMMON_H

enum BotCommonValues
{
// DREADLORD
    SPELL_VAMPIRIC_AURA                 = 20810,
    SPELL_SLEEP                         = 20663,
    SPELL_CARRION_SWARM                 = 34240,
    SPELL_INFERNO                       = 12740,    // summon infernal servant
    SPELL_INFERNO_METEOR_VISUAL         = 5739,     // meteor strike infernal
    SPELL_INFERNO_EFFECT                = 22703,    // stun, damage (warlock spell)

// CUSTOM SPELLS - UNUSED IN CODE AND DB
    SPELL_TRIGGERED_HEAL                = 25155,    // hidden,
    SPELL_ATTACK_MELEE_1H               = 42880,

// COMMON CDs
    POTION_CD                           = 60000,    //default 60sec potion cd
    REGEN_CD                            = 1000,     // update hp/mana every X milliseconds
};

enum BotPetTypes
{
// DREADLORD
    BOT_PET_INFERNAL                    = 9000001,
};

#define FROM_ARRAY(arr) arr, arr + sizeof(arr) / sizeof(arr[0])

#endif // _BOT_COMMON_H
