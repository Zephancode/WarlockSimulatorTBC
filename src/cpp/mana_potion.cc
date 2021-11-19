#include "mana_potion.h"

#include "common.h"
#include "player.h"

ManaPotion::ManaPotion(Player& player) : Spell(player) {
  cooldown = 120;
  is_item = true;
  on_gcd = false;
}

void ManaPotion::Cast() {
  Spell::Cast();
  const int kCurrentPlayerMana = player.stats.mana;
  // todo check for the randomize values option
  const int kManaGain = player.rng.range(min_mana, max_mana);
  player.stats.mana = std::min(player.stats.max_mana, kCurrentPlayerMana + kManaGain);
  if (player.recording_combat_log_breakdown) {
    player.AddIterationDamageAndMana(name, kManaGain, 0);
  }
  if (player.ShouldWriteToCombatLog()) {
    player.CombatLog("Player gains " + DoubleToString(round(player.stats.mana - kCurrentPlayerMana)) + " mana from " +
                     name + " (" + DoubleToString(round(kCurrentPlayerMana)) + " -> " +
                     DoubleToString(round(player.stats.mana)) + ")");
  }
}

SuperManaPotion::SuperManaPotion(Player& player) : ManaPotion(player) {
  name = "Super Mana Potion";
  min_mana = 1800;
  max_mana = 3000;
  Setup();
}

DemonicRune::DemonicRune(Player& player) : ManaPotion(player) {
  name = "Demonic Rune";
  min_mana = 900;
  max_mana = 1500;
  Setup();
}

void DemonicRune::Cast() {
  ManaPotion::Cast();
  if (player.spells->chipped_power_core != NULL) {
    player.spells->chipped_power_core->cooldown_remaining = cooldown;
  }
  if (player.spells->cracked_power_core != NULL) {
    player.spells->cracked_power_core->cooldown_remaining = cooldown;
  }
}