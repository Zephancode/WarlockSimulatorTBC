#ifndef WARLOCK_SIMULATOR_TBC_CHARACTER_STATS
#define WARLOCK_SIMULATOR_TBC_CHARACTER_STATS

struct CharacterStats {
  int health;
  int mana;
  int max_mana;
  double stamina;
  double intellect;
  double spirit;
  double spell_power;
  int shadow_power;
  int fire_power;
  int haste_rating;
  int hit_rating;
  int crit_rating;
  double crit_chance;
  double hit_chance;
  double extra_hit_chance;
  int mp5;
  int spell_penetration;
  double fire_modifier;
  double haste_percent;
  double shadow_modifier;
  double stamina_modifier;
  double intellect_modifier;
  double spirit_modifier;
  double mana_cost_modifier;

  inline int GetStamina() { return stamina * stamina_modifier; }
  inline int GetIntellect() { return intellect * intellect_modifier; }
  inline int GetSpirit() { return spirit * spirit_modifier; }
};

#endif