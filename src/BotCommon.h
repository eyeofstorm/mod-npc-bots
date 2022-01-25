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

uint32 const DrinkSpells[MAX_FEAST_SPELLS][2] =
{
    {  1,   430 },
    {  5,   431 },
    { 15,   432 },
    { 25,  1133 },
    { 35,  1135 },
    { 45,  1137 },
    { 60, 34291 },
    { 65, 27089 },
    { 70, 43182 },
    { 75, 43183 },
    { 80, 57073 }
};

uint32 const EatSpells[MAX_FEAST_SPELLS][2] =
{
    {  1,   433 },
    {  5,   434 },
    { 15,   435 },
    { 25,  1127 },
    { 35,  1129 },
    { 45,  1131 },
    { 55, 27094 },
    { 65, 35270 },
    { 70, 43180 }, //req 65 but
    { 75, 45548 },
    { 80, 45548 }
};

enum BotClasses : uint32
{
    BOT_CLASS_NONE                      = CLASS_NONE,
    BOT_CLASS_WARRIOR                   = CLASS_WARRIOR,
    BOT_CLASS_PALADIN                   = CLASS_PALADIN,
    BOT_CLASS_HUNTER                    = CLASS_HUNTER,
    BOT_CLASS_ROGUE                     = CLASS_ROGUE,
    BOT_CLASS_PRIEST                    = CLASS_PRIEST,
    BOT_CLASS_DEATH_KNIGHT              = CLASS_DEATH_KNIGHT,
    BOT_CLASS_SHAMAN                    = CLASS_SHAMAN,
    BOT_CLASS_MAGE                      = CLASS_MAGE,
    BOT_CLASS_WARLOCK                   = CLASS_WARLOCK,
    BOT_CLASS_DRUID                     = CLASS_DRUID,

    BOT_CLASS_BM,
    BOT_CLASS_SPHYNX,
    BOT_CLASS_ARCHMAGE,
    BOT_CLASS_SPELLBREAKER,
    BOT_CLASS_DARK_RANGER,
    BOT_CLASS_NECROMANCER,
    BOT_CLASS_DREADLORD,

    BOT_CLASS_END
};

#define BOT_CLASS_EX_START BOT_CLASS_BM

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
    COSMETIC_TELEPORT_EFFECT            = 52096,    //visual instant cast omni

// OTHERS
    BASE_MANA_SPHYNX                    = 400 * 5,
    BASE_MANA_SPELLBREAKER              = 250 * 5,
    BASE_MANA_NECROMANCER               = 400 * 5,
    //base mana at 10
    BASE_MANA_10_BM                     = 540 * 5,
    BASE_MANA_10_ARCHMAGE               = 705 * 5,
    BASE_MANA_10_DREADLORD              = 600 * 5,
    BASE_MANA_10_DARK_RANGER            = 570 * 5,
    //base mana at 1
    BASE_MANA_1_BM                      = 240 * 5,
    BASE_MANA_1_ARCHMAGE                = 285 * 5,
    BASE_MANA_1_DREADLORD               = 270 * 5,
    BASE_MANA_1_DARK_RANGER             = 225 * 5,
};

enum BotStatMods
{
    //ItemProtoType.h
    BOT_STAT_MOD_MANA                       = 0,
    BOT_STAT_MOD_HEALTH                     = 1,
    BOT_STAT_MOD_AGILITY                    = 3,
    BOT_STAT_MOD_STRENGTH                   = 4,
    BOT_STAT_MOD_INTELLECT                  = 5,
    BOT_STAT_MOD_SPIRIT                     = 6,
    BOT_STAT_MOD_STAMINA                    = 7,
    BOT_STAT_MOD_DEFENSE_SKILL_RATING       = 12,
    BOT_STAT_MOD_DODGE_RATING               = 13,
    BOT_STAT_MOD_PARRY_RATING               = 14,
    BOT_STAT_MOD_BLOCK_RATING               = 15,
    BOT_STAT_MOD_HIT_MELEE_RATING           = 16,
    BOT_STAT_MOD_HIT_RANGED_RATING          = 17,
    BOT_STAT_MOD_HIT_SPELL_RATING           = 18,
    BOT_STAT_MOD_CRIT_MELEE_RATING          = 19,
    BOT_STAT_MOD_CRIT_RANGED_RATING         = 20,
    BOT_STAT_MOD_CRIT_SPELL_RATING          = 21,
    BOT_STAT_MOD_HIT_TAKEN_MELEE_RATING     = 22,
    BOT_STAT_MOD_HIT_TAKEN_RANGED_RATING    = 23,
    BOT_STAT_MOD_HIT_TAKEN_SPELL_RATING     = 24,
    BOT_STAT_MOD_CRIT_TAKEN_MELEE_RATING    = 25,
    BOT_STAT_MOD_CRIT_TAKEN_RANGED_RATING   = 26,
    BOT_STAT_MOD_CRIT_TAKEN_SPELL_RATING    = 27,
    BOT_STAT_MOD_HASTE_MELEE_RATING         = 28,
    BOT_STAT_MOD_HASTE_RANGED_RATING        = 29,
    BOT_STAT_MOD_HASTE_SPELL_RATING         = 30,
    BOT_STAT_MOD_HIT_RATING                 = 31,
    BOT_STAT_MOD_CRIT_RATING                = 32,
    BOT_STAT_MOD_HIT_TAKEN_RATING           = 33,
    BOT_STAT_MOD_CRIT_TAKEN_RATING          = 34,
    BOT_STAT_MOD_RESILIENCE_RATING          = 35,
    BOT_STAT_MOD_HASTE_RATING               = 36,
    BOT_STAT_MOD_EXPERTISE_RATING           = 37,
    BOT_STAT_MOD_ATTACK_POWER               = 38,
    BOT_STAT_MOD_RANGED_ATTACK_POWER        = 39,
    BOT_STAT_MOD_FERAL_ATTACK_POWER         = 40,
    BOT_STAT_MOD_SPELL_HEALING_DONE         = 41,                 // deprecated
    BOT_STAT_MOD_SPELL_DAMAGE_DONE          = 42,                 // deprecated
    BOT_STAT_MOD_MANA_REGENERATION          = 43,
    BOT_STAT_MOD_ARMOR_PENETRATION_RATING   = 44,
    BOT_STAT_MOD_SPELL_POWER                = 45,
    BOT_STAT_MOD_HEALTH_REGEN               = 46,
    BOT_STAT_MOD_SPELL_PENETRATION          = 47,
    BOT_STAT_MOD_BLOCK_VALUE                = 48,
    //END ItemProtoType.h

    BOT_STAT_MOD_DAMAGE_MIN                 = BOT_STAT_MOD_BLOCK_VALUE + 1,
    BOT_STAT_MOD_DAMAGE_MAX,
    BOT_STAT_MOD_ARMOR,
    BOT_STAT_MOD_RESIST_HOLY,
    BOT_STAT_MOD_RESIST_FIRE,
    BOT_STAT_MOD_RESIST_NATURE,
    BOT_STAT_MOD_RESIST_FROST,
    BOT_STAT_MOD_RESIST_SHADOW,
    BOT_STAT_MOD_RESIST_ARCANE,
    BOT_STAT_MOD_EX,
    MAX_BOT_ITEM_MOD,

    BOT_STAT_MOD_RESISTANCE_START           = BOT_STAT_MOD_ARMOR
};

enum BotTalentSpecs
{
    BOT_SPEC_WARRIOR_ARMS               = 1,
    BOT_SPEC_WARRIOR_FURY               = 2,
    BOT_SPEC_WARRIOR_PROTECTION         = 3,
    BOT_SPEC_PALADIN_HOLY               = 4,
    BOT_SPEC_PALADIN_PROTECTION         = 5,
    BOT_SPEC_PALADIN_RETRIBUTION        = 6,
    BOT_SPEC_HUNTER_BEASTMASTERY        = 7,
    BOT_SPEC_HUNTER_MARKSMANSHIP        = 8,
    BOT_SPEC_HUNTER_SURVIVAL            = 9,
    BOT_SPEC_ROGUE_ASSASINATION         = 10,
    BOT_SPEC_ROGUE_COMBAT               = 11,
    BOT_SPEC_ROGUE_SUBTLETY             = 12,
    BOT_SPEC_PRIEST_DISCIPLINE          = 13,
    BOT_SPEC_PRIEST_HOLY                = 14,
    BOT_SPEC_PRIEST_SHADOW              = 15,
    BOT_SPEC_DK_BLOOD                   = 16,
    BOT_SPEC_DK_FROST                   = 17,
    BOT_SPEC_DK_UNHOLY                  = 18,
    BOT_SPEC_SHAMAN_ELEMENTAL           = 19,
    BOT_SPEC_SHAMAN_ENHANCEMENT         = 20,
    BOT_SPEC_SHAMAN_RESTORATION         = 21,
    BOT_SPEC_MAGE_ARCANE                = 22,
    BOT_SPEC_MAGE_FIRE                  = 23,
    BOT_SPEC_MAGE_FROST                 = 24,
    BOT_SPEC_WARLOCK_AFFLICTION         = 25,
    BOT_SPEC_WARLOCK_DEMONOLOGY         = 26,
    BOT_SPEC_WARLOCK_DESTRUCTION        = 27,
    BOT_SPEC_DRUID_BALANCE              = 28,
    BOT_SPEC_DRUID_FERAL                = 29,
    BOT_SPEC_DRUID_RESTORATION          = 30,
    BOT_SPEC_DEFAULT                    = 31,

    BOT_SPEC_BEGIN                      = BOT_SPEC_WARRIOR_ARMS,
    BOT_SPEC_END                        = BOT_SPEC_DEFAULT
};

enum BotStances
{
    BOT_STANCE_NONE                     = 0,
    WARRIOR_BATTLE_STANCE               = BOT_CLASS_END,
    WARRIOR_DEFENSIVE_STANCE,
    WARRIOR_BERSERKER_STANCE,
    DEATH_KNIGHT_BLOOD_PRESENCE,
    DEATH_KNIGHT_FROST_PRESENCE,
    DEATH_KNIGHT_UNHOLY_PRESENCE,
    DRUID_BEAR_FORM,
    DRUID_CAT_FORM,
    DRUID_MOONKIN_FORM,
    DRUID_TREE_FORM,
    DRUID_TRAVEL_FORM,
    DRUID_AQUATIC_FORM,
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
