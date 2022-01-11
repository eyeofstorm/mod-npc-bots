/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_COMMON_H
#define _BOT_COMMON_H

#include "SharedDefines.h"
#include "SpellAuraDefines.h"

#include <utility>
#include <vector>
#include <map>

#define BOT_FOLLOW_DIST  3.0f
#define BOT_FOLLOW_ANGLE (M_PI/2)

enum eFollowState
{
    STATE_FOLLOW_NONE       = 0x000,
    STATE_FOLLOW_INPROGRESS = 0x001,    //must always have this state for any follow
    STATE_FOLLOW_RETURNING  = 0x002,    //when returning to combat start after being in combat
    STATE_FOLLOW_PAUSED     = 0x004,    //disables following
    STATE_FOLLOW_COMPLETE   = 0x008     //follow is completed and may end
};

#define MAX_POTION_SPELLS 8
#define MAX_FEAST_SPELLS 11

uint32 const ManaPotionSpells[MAX_POTION_SPELLS][2] =
{
    {  5,   437 },
    { 14,   438 },
    { 22,  2023 },
    { 31, 11903 },
    { 41, 17530 },
    { 49, 17531 },
    { 55, 28499 },
    { 70, 43186 }
};

uint32 const HealingPotionSpells[MAX_POTION_SPELLS][2] =
{
    {  1,   439 },
    {  3,   440 },
    { 12,   441 },
    { 21,  2024 },
    { 35,  4042 },
    { 45, 17534 },
    { 55, 28495 },
    { 70, 43185 }
};

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

// ADVANCED
    COSMETIC_TELEPORT_EFFECT            = 52096     //visual instant cast omni
};

#define BOT_ENTRY_BASE 9000000

enum BotTypes
{
// DREADLORD
    BOT_DREADLORD                       = (BOT_ENTRY_BASE + 1),
    BOT_INFERNAL                        = (BOT_ENTRY_BASE + 2)
};

enum BotPetTypes
{
// DREADLORD
    BOT_PET_INFERNAL                    = BOT_INFERNAL
};

#define FROM_ARRAY(arr) arr, arr + sizeof(arr) / sizeof(arr[0])

#endif // _BOT_COMMON_H
