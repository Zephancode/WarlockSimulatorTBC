#ifndef WARLOCK_SIMULATOR_TBC_PET
#define WARLOCK_SIMULATOR_TBC_PET

struct Player;
#include <iostream>

#include "enums.h"
#include "pet_auras.h"
#include "pet_spells.h"
#include "pet_stats.h"

struct Pet : std::enable_shared_from_this<Pet> {
  Player& player;
  PetSpells spells;
  PetAuras auras;
  PetStats base_stats;
  PetStats buff_stats;  // Certain stats from buffs need to be separated from the main
                        // stat so we can re-calculate the pet's stats in
                        // calculateStatsFromPlayer().
  PetStats debuff_stats;
  PetStats stats;
  PetName pet;
  PetType pet_type;
  std::string name;
  double enemy_dodge_chance;
  double cast_time_remaining;
  double five_second_rule_timer_remaining;
  double spirit_tick_timer_remaining;
  double crit_multiplier;
  double glancing_blow_multiplier;
  double glancing_blow_chance;
  double crit_suppression;
  double armor_multiplier;
  double dmg;
  double base_melee_speed;

  Pet(Player& player);
  void Initialize();
  void CalculateStatsFromAuras();
  void CalculateStatsFromPlayer(bool announce_in_combat_log = true);
  void Setup();
  void Cast();
  void Reset();
  void EndAuras();
  void Tick(double t);
  int GetAttackPower();
  double GetMeleeCritChance();
  double GetMeleeHitChance();
  double GetHastePercent();
  double GetSpellCritChance();
  double GetSpellHitChance();
  int GetStamina();
  int GetIntellect();
  int GetSpirit();
  int GetAgility();
  int GetStrength();
  int GetPlayerSpellPower();
  bool IsCrit(AttackType type);
  bool IsHit(AttackType type);
};

struct Imp : public Pet {
  Imp(Player& player);
};

struct Succubus : public Pet {
  Succubus(Player& player);
};

struct Felguard : public Pet {
  Felguard(Player& player);
};

#endif