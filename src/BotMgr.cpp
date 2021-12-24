/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BotMgr.h"
#include "Group.h"
#include "Item.h"
#include "Log.h"
#include "Map.h"

void BotMgr::PlayerHireBot(Player* owner, Creature* bot)
{
    if (!bot || !owner)
    {
        return;
    }

    LOG_DEBUG("npcbots", "player [%s] begin hire the bot [%s]", owner->GetGUID().ToString().c_str(), bot->GetGUID().ToString().c_str());

    if (!bot->GetAI())
    {
        LOG_WARN("npcbots", "creature [%s] has no AI.", bot->GetGUID().ToString().c_str());
    }
    else 
    {
        LOG_DEBUG("npcbots", "creature [%s] has AI(%s).", bot->GetGUID().ToString().c_str(), bot->GetScriptName().c_str());
    }

    bot->SetOwnerGUID(owner->GetGUID());
    bot->SetCreatorGUID(owner->GetGUID());
    bot->SetByteValue(UNIT_FIELD_BYTES_2, 1, owner->GetByteValue(UNIT_FIELD_BYTES_2, 1));
    bot->SetFaction(owner->GetFaction());
    bot->SetPhaseMask(owner->GetPhaseMask(), true);

    owner->m_Controlled.insert(bot);

    bot->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
    bot->m_ControlledByPlayer = true;

    BotMgr::SetBotLevel(bot, owner->getLevel());

    LOG_DEBUG("npcbots", "begin register bot [%s]", bot->GetGUID().ToString().c_str());
    sBotsRegistry->RegisterBot(bot);
    LOG_DEBUG("npcbots", "end register bot [%s]", bot->GetGUID().ToString().c_str());

    BotAI* ai = GetBotAI(bot);

    if (ai)
    {
        ai->SetBotOwner(owner);
        ai->StartFollow(owner);
    }

    LOG_DEBUG("npcbots", "bot [%s] hired!", bot->GetGUID().ToString().c_str());
}

bool BotMgr::DismissBot(Creature* bot)
{
    if (!bot)
    {
        return false;
    }

    PlayerBotsEntry* entry = sBotsRegistry->GetEntry(bot);

    if (!entry)
    {
        return false;
    }

    Player* owner = const_cast<Player*>(entry->GetBotsOwner());
    BotAI* ai = GetBotAI(bot);

    if (ai)
    {
        ai->SetFollowComplete();
        ai->SetBotOwner(nullptr);
        ai->UnSummonBotPet();
    }

    CleanupsBeforeBotRemove(owner, bot);

    LOG_DEBUG("npcbots", "begin unregister bot [%s]", bot->GetGUID().ToString().c_str());
    sBotsRegistry->UnregisterBot(bot);
    LOG_DEBUG("npcbots", "end unregister bot [%s]", bot->GetGUID().ToString().c_str());

    return true;
}

void BotMgr::CleanupsBeforeBotRemove(Player* owner, Creature* bot)
{
    if (bot->GetVehicle())
    {
        bot->ExitVehicle();
    }

    bot->SetOwnerGUID(ObjectGuid::Empty);
    bot->SetCreatorGUID(ObjectGuid::Empty);
    bot->SetByteValue(UNIT_FIELD_BYTES_2, 1, 0);
    bot->SetPhaseMask(0, true);
    bot->SetFaction(bot->GetCreatureTemplate()->faction);

    bot->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
    owner->m_Controlled.erase(bot);
    bot->m_ControlledByPlayer = false;

    SetBotLevel(bot, bot->GetCreatureTemplate()->minlevel);

    Map* map = bot->FindMap();

    if (!map || map->IsDungeon())
    {
        LOG_DEBUG("npcbots", "CleanupsBeforeBotRemove calls Creature::RemoveFromWorld()");
        bot->RemoveFromWorld();
    }
}

BotAI* BotMgr::GetBotAI(Creature* bot)
{
    if (!bot)
    {
        return nullptr;
    }

    if (bot->GetCreatureTemplate()->Entry < 9000000)
    {
        return nullptr;
    }

    BotAI* ai = (BotAI*)bot->AI();

    return ai;
}

void BotMgr::SetBotLevel(Creature* bot, uint8 level)
{
    CreatureTemplate const* cInfo = bot->GetCreatureTemplate();
    uint32 rank = 0;

    bot->SetLevel(level, true);

    CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(level, cInfo->unit_class);

    // health
    float healthmod = sWorld->getRate(RATE_CREATURE_ELITE_RAREELITE_HP);

    uint32 basehp = std::max<uint32>(1, stats->GenerateHealth(cInfo));
    uint32 health = uint32(basehp * healthmod);

    bot->SetCreateHealth(health);
    bot->SetMaxHealth(health);
    bot->SetHealth(health);
    bot->ResetPlayerDamageReq();

    // mana
    uint32 mana = stats->GenerateMana(cInfo);

    bot->SetCreateMana(mana);
    bot->SetMaxPower(POWER_MANA, mana);
    bot->SetPower(POWER_MANA, mana);

    bot->SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, (float)health);
    bot->SetModifierValue(UNIT_MOD_MANA, BASE_VALUE, (float)mana);

    // damage
    float basedamage = stats->GenerateBaseDamage(cInfo);

    float weaponBaseMinDamage = basedamage;
    float weaponBaseMaxDamage = basedamage * 1.5;

    bot->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    bot->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);

    bot->SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    bot->SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);

    bot->SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    bot->SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);

    bot->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, stats->AttackPower);
    bot->SetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, stats->RangedAttackPower);
}

int BotMgr::GetPlayerBotsCount(Player* player)
{
    PlayerBotsEntry* entry = sBotsRegistry->GetEntry(player);

    if (entry)
    {
        return entry->GetBots().size();
    }

    return 0;
}

void BotMgr::OnBotSpellGo(Unit const* caster, Spell const* spell, bool ok)
{
    BotAI* ai = GetBotAI(const_cast<Creature*>(caster->ToCreature()));

    if (ai)
    {
        ai->OnBotSpellGo(spell, ok);
    }
}
