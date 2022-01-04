/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_AI_H
#define _BOT_AI_H

#include "Player.h"
#include "ScriptedCreature.h"

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

class BotAI : public ScriptedAI
{
protected:
    explicit BotAI(Creature* creature);

public:
    virtual ~BotAI();

private:
    struct BotSpell
    {
        explicit BotSpell() : cooldown(0), enabled(true) { }
        uint32 spellId;
        uint32 cooldown;
        bool enabled;

    private:
        BotSpell(BotSpell const&);
    };

public:
    Unit* GetBotOwner() const { return m_owner; }
    void SetBotOwner(Unit *owner) { m_owner = owner; }
    Creature* GetBot() const { return m_bot; }
    Creature* GetPet() const { return m_pet; }
    ObjectGuid GetLeaderGuid() const { return m_uiLeaderGUID; }
    uint32 GetBotSpellId(uint32 basespell) const;

public:
    void MovementInform(uint32 motionType, uint32 pointId) override;
    void AttackStart(Unit*) override;
    void MoveInLineOfSight(Unit*) override;
    void EnterEvadeMode() override;
    void JustDied(Unit*) override;
    void JustRespawned() override;
    void UpdateAI(uint32) override;

public:
    void OnCreatureFinishedUpdate(uint32 uiDiff);
    void OnBotOwnerMoveWorldport(Player* owner);
    void OnBotSpellGo(Spell const* spell, bool ok = true);
    virtual void OnClassSpellGo(SpellInfo const* /*spell*/) { }

    void StartFollow(Unit* leader, uint32 factionForFollower = 0);
    void SetFollowComplete();
    bool FinishTeleport();
    void TeleportHome();
    void GetHomePosition(uint16& mapid, Position* pos) const;
    bool OnBeforeOwnerTeleport(uint32 mapid, float x, float y, float z, float orientation, uint32 options, Unit* target);
    void BotStopMovement();
    Unit* GetLeaderForFollower();

    bool HasBotState(uint32 uiBotState) { return (m_uiBotState & uiBotState); }
    bool IAmFree() const;
    bool IsChanneling(Unit const* u = nullptr) const { if (!u) u = me; return u->GetCurrentSpell(CURRENT_CHANNELED_SPELL); }
    bool IsCasting(Unit const* u = nullptr) const { if (!u) u = me; return (u->HasUnitState(UNIT_STATE_CASTING) || IsChanneling(u) || u->IsNonMeleeSpellCast(false, false, true, false, false)); }
    bool IsSpellReady(uint32 basespell, uint32 diff) const;
    bool CanBotAttackOnVehicle() const;
    bool CCed(Unit const* target, bool root = false);
    virtual bool IsPetAI() = 0;

    void SetSpellCooldown(uint32 basespell, uint32 msCooldown);
    void ReduceSpellCooldown(uint32 basespell, uint32 uiDiff);

    Unit* FindAOETarget(float dist, uint32 minTargetNum = 3) const;
    Unit* FindStunTarget(float dist = 20) const;
    Unit* FindCastingTarget(float maxdist = 10, float mindist = 0, uint32 spellId = 0, uint8 minHpPct = 0) const;
    void GetNearbyTargetsInConeList(std::list<Unit*> &targets, float maxdist) const;

    virtual void SummonBotPet(Position const* /*pos*/) { }
    virtual void UnSummonBotPet() { }

    // events process
    EventProcessor* GetEvents() { return &Events; }
    void KillEvents(bool force);

public:
    uint16 Rand() const;

    typedef std::unordered_map<uint32 /*firstrankspellid*/, BotSpell* /*spell*/> BotSpellMap;
    BotSpellMap const& GetSpellMap() const { return m_spells; }

protected:
    virtual void UpdateBotAI(uint32 uiDiff);
    virtual void UpdateSpellCD(uint32 uiDiff) { }
    virtual void InitCustomeSpells() { }

    void UpdateCommonTimers(uint32 uiDiff);
    bool UpdateCommonBotAI(uint32 uiDiff);
    void BuildGrouUpdatePacket(WorldPacket* data);

    void InitSpellMap(uint32 basespell, bool forceadd = false, bool forwardRank = true);
    static uint8 GetHealthPCT(Unit const* u) { if (!u || !u->IsAlive() || u->GetMaxHealth() <= 1) return 100; return uint8(((float(u->GetHealth())) / u->GetMaxHealth()) * 100); }
    static uint8 GetManaPCT(Unit const* u) { if (!u || !u->IsAlive() || u->GetMaxPower(POWER_MANA) <= 1) return 100; return (u->GetPower(POWER_MANA) * 10 / (1 + u->GetMaxPower(POWER_MANA) / 10)); }

    void DrinkPotion(bool mana);
    bool IsPotionReady() const;
    uint32 GetPotion(bool mana) const;
    void StartPotionTimer();
    bool IsPotionSpell(uint32 spellId) const;

    bool AssistPlayerInCombat(Unit* who);

private:
    void UpdateFollowerAI(uint32 uiDiff);
    void AddBotState(uint32 uiBotState) { m_uiBotState |= uiBotState; }
    void RemoveBotState(uint32 uiBotState) { m_uiBotState &= ~uiBotState; }
    bool DelayUpdateIfNeeded();
    void GenerateRand() const;
    void Regenerate();
    void RegenerateEnergy();

protected:
    Unit* m_owner;
    Creature* m_bot;
    Creature* m_pet;
    ObjectGuid m_uiLeaderGUID;

    // events process
    EventProcessor Events;

    // timer
    uint32 m_lastUpdateDiff;
    uint32 m_potionTimer;

private:
    // timer
    uint32 m_uiFollowerTimer;
    uint32 m_uiWaitTimer;
    uint32 m_uiUpdateTimerMedium;
    uint32 m_regenTimer;

    float m_energyFraction;
    uint32 m_uiBotState;

    BotSpellMap m_spells;
};

#endif // _BOT_AI_H_
