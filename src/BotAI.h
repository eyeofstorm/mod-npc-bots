/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 
 * License: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _BOT_AI_H
#define _BOT_AI_H

#include "Player.h"
#include "ScriptedCreature.h"

#define BOT_FOLLOW_DIST  4.0f
#define BOT_FOLLOW_ANGLE (M_PI/2)

enum eFollowState
{
    STATE_FOLLOW_NONE       = 0x000,
    STATE_FOLLOW_INPROGRESS = 0x001,    //must always have this state for any follow
    STATE_FOLLOW_RETURNING  = 0x002,    //when returning to combat start after being in combat
    STATE_FOLLOW_PAUSED     = 0x004,    //disables following
    STATE_FOLLOW_COMPLETE   = 0x008     //follow is completed and may end
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
    void MovementInform(uint32 motionType, uint32 pointId) override;
    void AttackStart(Unit*) override;
    void MoveInLineOfSight(Unit*) override;
    void EnterEvadeMode() override;
    void JustDied(Unit*) override;
    void JustRespawned() override;
    void UpdateAI(uint32) override;

public:
    void OnCreatureFinishedUpdate(uint32 uiDiff);
    void StartFollow(Player* player, uint32 factionForFollower = 0);
    void SetFollowComplete();

    bool HasBotState(uint32 uiBotState) { return (m_uiBotState & uiBotState); }
    bool IAmFree() const;

    bool IsChanneling(Unit const* u = nullptr) const { if (!u) u = me; return u->GetCurrentSpell(CURRENT_CHANNELED_SPELL); }
    bool IsCasting(Unit const* u = nullptr) const { if (!u) u = me; return (u->HasUnitState(UNIT_STATE_CASTING) || IsChanneling(u) || u->IsNonMeleeSpellCast(false, false, true, false, false)); }
    bool IsSpellReady(uint32 basespell, uint32 diff) const;
    void SetSpellCooldown(uint32 basespell, uint32 msCooldown);
    void ReduceSpellCooldown(uint32 basespell, uint32 uiDiff);

    Player* GetBotOwner() const { return m_master; }
    void SetBotOwner(Player* newOwner) { m_master = newOwner; }

    bool CanBotAttackOnVehicle() const;

    void OnBotSpellGo(Spell const* spell, bool ok = true);

    virtual void SummonBotPet(Position const* /*pos*/) { }
    virtual void UnSummonBotPet() { }
    virtual void OnClassSpellGo(SpellInfo const* /*spell*/) { }

    uint16 Rand() const;

public:
    typedef std::unordered_map<uint32 /*firstrankspellid*/, BotSpell* /*spell*/> BotSpellMap;

    BotSpellMap const& GetSpellMap() const { return m_spells; }
    uint32 GetBotSpellId(uint32 basespell) const;
    float CalcSpellMaxRange(uint32 spellId, bool enemy = true) const;

protected:
    virtual void UpdateBotAI(uint32 uiDiff);
    virtual void ReduceCooldown(uint32 uiDiff) { }
    virtual void InitCustomeSpells() { }

    bool UpdateCommonBotAI(uint32 uiDiff);
    Player* GetLeaderForFollower();
    void BuildGrouUpdatePacket(WorldPacket* data);

    void InitSpellMap(uint32 basespell, bool forceadd = false, bool forwardRank = true);

private:
    void UpdateFollowerAI(uint32 uiDiff);
    void AddBotState(uint32 uiBotState) { m_uiBotState |= uiBotState; }
    void RemoveBotState(uint32 uiBotState) { m_uiBotState &= ~uiBotState; }
    bool AssistPlayerInCombat(Unit* who);
    bool DelayUpdateIfNeeded();
    void GenerateRand() const;
    void Regenerate();
    void RegenerateEnergy();

protected:
    Player* m_master;
    Creature* m_bot;
    Creature* m_pet;

private:
    ObjectGuid m_uiLeaderGUID;

    uint32 m_lastUpdateDiff;
    uint32 m_uiFollowerTimer;
    uint32 m_uiWaitTimer;
    uint32 m_uiUpdateTimerMedium;
    uint32 m_regenTimer;

    float m_energyFraction;
    uint32 m_uiBotState;

    BotSpellMap m_spells;
};

#endif // _BOT_AI_H_
