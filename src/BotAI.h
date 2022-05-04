/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_AI_H
#define _BOT_AI_H

#include "BotCommon.h"
#include "EventProcessor.h"
#include "ScriptedCreature.h"
#include "Player.h"

class BotAI : public ScriptedAI
{
    friend class BotMgr;

protected:
    explicit BotAI(Creature* creature);

public:
    virtual ~BotAI();

private:
    struct BotSpell
    {
        explicit BotSpell() : spellId(0), cooldown(0), enabled(true) { }

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
    ObjectGuid GetLeaderGUID() const { return m_uiLeaderGUID; }
    void SetLeaderGUID(ObjectGuid leaderGUID) { m_uiLeaderGUID = leaderGUID; }
    uint32 GetBotSpellId(uint32 basespell) const;
    virtual uint32 GetBotClass() const;
    virtual uint8 GetBotStance() const;
    float GetTotalBotStat(uint8 stat) const;

public:
    void MovementInform(uint32 motionType, uint32 pointId) override;
    void AttackStart(Unit*) override;
    void MoveInLineOfSight(Unit*) override;
    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override;
    void EnterCombat(Unit* /*victim*/) override;
    void JustDied(Unit*) override;
    void JustRespawned() override;
    void UpdateAI(uint32) override;
    void UpdateMana() { m_isDoUpdateMana= true; }
    void SpellHit(Unit* /*caster*/, SpellInfo const* /*spell*/) override;

public:
    bool OnBeforeCreatureUpdate(uint32 uiDiff);
    void OnBotOwnerMoveWorldport(Player* owner);
    void OnBotSpellGo(Spell const* spell, bool ok = true);
    void OnBotOwnerLevelChanged(uint8 /*newLevel*/, bool showLevelChange = true);
    virtual void OnClassSpellGo(SpellInfo const* /*spell*/) { }

    bool BotFinishTeleport();

    void StartFollow(Unit* leader, uint32 factionForFollower = 0);
    void SetFollowComplete();
    void BotStopMovement();
    Unit* GetLeaderForFollower();
    void BotMovement(BotMovementType type, Position const* pos, Unit* target = nullptr, bool generatePath = true) const;

    bool HasBotState(uint32 uiBotState) { return (m_uiBotState & uiBotState); }
    bool IAmFree() const;
    bool IsChanneling(Unit const* u = nullptr) const { if (!u) u = m_bot; return u->GetCurrentSpell(CURRENT_CHANNELED_SPELL); }
    bool IsCasting(Unit const* u = nullptr) const { if (!u) u = m_bot; return (u->HasUnitState(UNIT_STATE_CASTING) || IsChanneling(u) || u->IsNonMeleeSpellCast(false, false, true, false, false)); }
    bool IsSpellReady(uint32 basespell, uint32 diff, bool checkGCD = true) const;
    bool CanBotAttackOnVehicle() const;
    bool CCed(Unit const* target, bool root = false);
    static bool IsHeroExClass(uint8 botClass);
    bool JumpingOrFalling() const;
    bool Jumping() const;

    void SetGlobalCooldown(uint32 gcd);
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
    virtual void UpdateBotCombatAI(uint32 uiDiff);
    virtual void UpdateSpellCD(uint32 /*uiDiff*/) { }
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
    uint32 GetRation(bool drink) const;
    void StartPotionTimer();
    bool IsPotionSpell(uint32 spellId) const;
    virtual bool CanEat() const;
    virtual bool CanDrink() const;
    virtual bool CanSit() const;
    bool CanMount() const;

    bool AssistPlayerInCombat(Unit* who);

    bool DoCastSpell(Unit* victim, uint32 spellId, bool triggered = false);
    bool DoCastSpell(Unit* victim, uint32 spellId, TriggerCastFlags flags);
    SpellCastResult CheckBotCast(Unit const* victim, uint32 spellId) const;
    virtual bool RemoveShapeshiftForm() { return true; }

private:
    void UpdateFollowerAI(uint32 uiDiff);
    void UpdateBotRations();
    void UpdateMountedState();
    void UpdateStandState() const;
    void OnManaUpdate() const;
    void OnManaRegenUpdate() const;
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

    // leader guid of follower
    ObjectGuid m_uiLeaderGUID;

    // events process
    EventProcessor Events;

    // timer
    uint32 m_gcdTimer;
    uint32 m_lastUpdateDiff;
    uint32 m_potionTimer;

    uint32 m_botClass;
    CreatureBaseStats const* m_classLevelInfo;
    uint8 m_botSpec;

private:
    // timer
    uint32 m_uiFollowerTimer;
    uint32 m_uiWaitTimer;
    uint32 m_uiUpdateTimerMedium;
    uint32 m_regenTimer;

    float m_energyFraction;
    uint32 m_uiBotState;

    BotSpellMap m_spells;

    bool m_isDoUpdateMana;
    bool m_isFeastMana;
    bool m_isFeastHealth;

    //stats
    float m_hit, m_parry, m_dodge, m_block, m_crit, m_dmgTakenPhy, m_dmgTakenMag, m_armorPen;
    uint32 m_expertise, m_spellPower, m_spellPen, m_defense, m_blockValue;
    int32 m_haste, m_resistbonus[6];
};

#endif // _BOT_AI_H_
