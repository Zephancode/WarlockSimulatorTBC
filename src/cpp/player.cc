#include "player.h"

#include <math.h>

#include "bindings.h"
#include "common.h"
#include "damage_over_time.h"
#include "mana_over_time.h"
#include "mana_potion.h"
#include "spell.h"

Player::Player(PlayerSettings& player_settings)
    : selected_auras(player_settings.auras),
      talents(player_settings.talents),
      sets(player_settings.sets),
      stats(player_settings.stats),
      items(player_settings.items),
      settings(player_settings) {
  spells = std::make_unique<PlayerSpells>();
  auras = std::make_unique<PlayerAuras>();
  combat_log_entries = {};
  cast_time_remaining = 0;
  gcd_remaining = 0;
  // I don't know if this formula only works for bosses or not, so for the
  // moment I'm only using it for lvl >=73 targets.
  const double enemy_base_resistance = settings.enemy_level >= 73 ? (6 * kLevel * 5) / 75.0 : 0;
  settings.enemy_shadow_resist = std::max(static_cast<double>(settings.enemy_shadow_resist), enemy_base_resistance);
  settings.enemy_fire_resist = std::max(static_cast<double>(settings.enemy_fire_resist), enemy_base_resistance);
  recording_combat_log_breakdown = settings.recording_combat_log_breakdown && settings.equipped_item_simulation;

  if (recording_combat_log_breakdown) {
    combat_log_breakdown.insert(std::make_pair("Mp5", std::make_unique<CombatLogBreakdown>("Mp5")));
  }

  if (settings.custom_stat == EmbindConstant::kStamina)
    custom_stat = "stamina";
  else if (settings.custom_stat == EmbindConstant::kIntellect)
    custom_stat = "intellect";
  else if (settings.custom_stat == EmbindConstant::kSpirit)
    custom_stat = "spirit";
  else if (settings.custom_stat == EmbindConstant::kSpellPower)
    custom_stat = "spellPower";
  else if (settings.custom_stat == EmbindConstant::kShadowPower)
    custom_stat = "shadowPower";
  else if (settings.custom_stat == EmbindConstant::kFirePower)
    custom_stat = "firePower";
  else if (settings.custom_stat == EmbindConstant::kCritRating)
    custom_stat = "critRating";
  else if (settings.custom_stat == EmbindConstant::kHitRating)
    custom_stat = "hitRating";
  else if (settings.custom_stat == EmbindConstant::kHasteRating)
    custom_stat = "hasteRating";
  else if (settings.custom_stat == EmbindConstant::kMp5)
    custom_stat = "mp5";
  else
    custom_stat = "normal";

  // Crit chance
  if (selected_auras.atiesh_mage) {
    stats.crit_rating += 28 * settings.mage_atiesh_amount;
  }
  stats.crit_chance = kBaseCritChancePercent + talents.devastation + talents.backlash + talents.demonic_tactics;
  if (selected_auras.moonkin_aura) {
    stats.crit_chance += 5;
  }
  if (selected_auras.judgement_of_the_crusader) {
    stats.crit_chance += 3;
  }
  if (selected_auras.totem_of_wrath) {
    stats.crit_chance += (3 * settings.totem_of_wrath_amount);
  }
  if (selected_auras.chain_of_the_twilight_owl) {
    stats.crit_chance += 2;
  }

  // Hit chance
  if (sets.mana_etched >= 2) {
    stats.hit_rating += 35;
  }
  stats.extra_hit_chance = stats.hit_rating / kHitRatingPerPercent;
  if (selected_auras.totem_of_wrath) {
    stats.extra_hit_chance += (3 * settings.totem_of_wrath_amount);
  }
  if (selected_auras.inspiring_presence) {
    stats.extra_hit_chance += 1;
  }
  stats.hit_chance = round(GetBaseHitChance(kLevel, settings.enemy_level));

  // Add bonus damage % from Demonic Sacrifice
  if (talents.demonic_sacrifice == 1 && settings.sacrificing_pet) {
    if (settings.selected_pet == EmbindConstant::kImp) {
      stats.fire_modifier *= 1.15;
    } else if (settings.selected_pet == EmbindConstant::kSuccubus) {
      stats.shadow_modifier *= 1.15;
    } else if (settings.selected_pet == EmbindConstant::kFelguard) {
      stats.shadow_modifier *= 1.1;
    }
    // todo add felhunter mana regen maybe
  } else {
    // Add damage % multiplier from Master Demonologist and Soul Link
    if (talents.soul_link == 1) {
      stats.shadow_modifier *= 1.05;
      stats.fire_modifier *= 1.05;
    }
    if (talents.master_demonologist > 0) {
      if (settings.selected_pet == EmbindConstant::kSuccubus) {
        stats.shadow_modifier *= (1 + 0.02 * talents.master_demonologist);
        stats.fire_modifier *= (1 + 0.02 * talents.master_demonologist);
      } else if (settings.selected_pet == EmbindConstant::kFelguard) {
        stats.shadow_modifier *= (1 + 0.01 * talents.master_demonologist);
        stats.fire_modifier *= (1 + 0.01 * talents.master_demonologist);
      }
    }
  }
  // Shadow Mastery
  stats.shadow_modifier *= (1 + (0.02 * talents.shadow_mastery));
  // Ferocious Inspiration
  if (selected_auras.ferocious_inspiration) {
    stats.shadow_modifier *= std::pow(1.03, settings.ferocious_inspiration_amount);
    stats.fire_modifier *= std::pow(1.03, settings.ferocious_inspiration_amount);
  }
  // Add % dmg modifiers from Curse of the Elements + Malediction
  if (selected_auras.curse_of_the_elements) {
    stats.shadow_modifier *= 1.1 + (0.01 * settings.improved_curse_of_the_elements);
    stats.fire_modifier *= 1.1 + (0.01 * settings.improved_curse_of_the_elements);
  }
  // Add fire dmg % from Emberstorm
  if (talents.emberstorm > 0) {
    stats.fire_modifier *= 1 + (0.02 * talents.emberstorm);
  }
  // Add spell power from Fel Armor
  if (selected_auras.fel_armor) {
    stats.spell_power += 100 * (0 + 0.1 * talents.demonic_aegis);
  }
  // If using a custom isb uptime % then just add to the shadow modifier % (this
  // assumes 5/5 ISB giving 20% shadow Damage)
  if (settings.using_custom_isb_uptime) {
    stats.shadow_modifier *= (1.0 + 0.2 * (settings.custom_isb_uptime_value / 100.0));
  }
  // Add spell power from Improved Divine Spirit
  stats.spirit_modifier *= (1 - (0.01 * talents.demonic_embrace));
  if (selected_auras.prayer_of_spirit && settings.improved_divine_spirit > 0) {
    stats.spell_power += stats.GetSpirit() * (0 + (static_cast<double>(settings.improved_divine_spirit) / 20.0));
  }
  // Elemental shaman t4 2pc bonus
  if (selected_auras.wrath_of_air_totem && settings.has_elemental_shaman_t4_bonus) {
    stats.spell_power += 20;
  }
  // Add extra stamina from Blood Pact from Improved Imp
  if (selected_auras.blood_pact) {
    int improved_imp_points = settings.improved_imp;

    if (settings.selected_pet == EmbindConstant::kImp &&
        (!settings.sacrificing_pet || talents.demonic_sacrifice == 0) && talents.improved_imp > improved_imp_points) {
      improved_imp_points = talents.improved_imp;
    }

    stats.stamina += 70 * 0.1 * improved_imp_points;
  }
  // Add stamina from Demonic Embrace
  stats.stamina_modifier *= 1 + (0.03 * talents.demonic_embrace);
  // Add mp5 from Vampiric Touch (add 25% instead of 5% since we're adding it to
  // the mana per 5 seconds variable)
  if (selected_auras.vampiric_touch) {
    stats.mp5 += settings.shadow_priest_dps * 0.25;
  }
  if (selected_auras.atiesh_warlock) {
    stats.spell_power += 33 * settings.warlock_atiesh_amount;
  }
  if (sets.twin_stars == 2) {
    stats.spell_power += 15;
  }

  // Enemy Armor Reduction
  if (selected_auras.faerie_fire) {
    settings.enemy_armor -= 610;
  }
  if ((selected_auras.sunder_armor && selected_auras.expose_armor && settings.improved_expose_armor == 2) ||
      (selected_auras.expose_armor && !selected_auras.sunder_armor)) {
    settings.enemy_armor -= 2050 * (1 + 0.25 * settings.improved_expose_armor);
  } else if (selected_auras.sunder_armor) {
    settings.enemy_armor -= 520 * 5;
  }
  if (selected_auras.curse_of_recklessness) {
    settings.enemy_armor -= 800;
  }
  if (selected_auras.annihilator) {
    settings.enemy_armor -= 600;
  }
  settings.enemy_armor = std::max(0, settings.enemy_armor);

  // Health & Mana
  stats.health =
      (stats.health + stats.GetStamina() * kHealthPerStamina) * (1 + (0.01 * static_cast<double>(talents.fel_stamina)));
  stats.max_mana = (stats.mana + stats.GetIntellect() * kManaPerIntellect) *
                   (1 + (0.01 * static_cast<double>(talents.fel_intellect)));
}

void Player::Initialize() {
  demonic_knowledge_spell_power = 0;
  if (!settings.sacrificing_pet || talents.demonic_sacrifice == 0) {
    if (settings.selected_pet == EmbindConstant::kImp) {
      pet = std::make_shared<Imp>(*this);
    } else if (settings.selected_pet == EmbindConstant::kSuccubus) {
      pet = std::make_shared<Succubus>(*this);
    } else if (settings.selected_pet == EmbindConstant::kFelguard) {
      pet = std::make_shared<Felguard>(*this);
    }
    if (pet != NULL) {
      pet->Initialize();
    }
  }

  std::vector<int> equipped_trinket_ids{items.trinket_1, items.trinket_2};
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 32483) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<SkullOfGuldan>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 34429) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<ShiftingNaaruSliver>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 33829) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<HexShrunkenHead>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 29370) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<IconOfTheSilverCrescent>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 29132) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<ScryersBloodgem>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 23046) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<RestrainedEssenceOfSapphiron>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 29179) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<XirisGift>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 25620) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<AncientCrystalTalisman>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 28223) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<ArcanistsStone>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 25936) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<TerokkarTabletOfVim>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 28040) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<VengeanceOfTheIllidari>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 24126) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<FigurineLivingRubySerpent>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 29376) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<EssenceOfTheMartyr>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 30340) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<StarkillersBauble>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 38290) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<DarkIronSmokingPipe>(*this));
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 19957) != equipped_trinket_ids.end())
    trinkets.push_back(std::make_unique<HazzarahsCharmOfDestruction>(*this));

  // Auras
  if (settings.fight_type == EmbindConstant::kSingleTarget) {
    if (talents.improved_shadow_bolt > 0) auras->improved_shadow_bolt = std::make_shared<ImprovedShadowBoltAura>(*this);
    if (settings.has_corruption || settings.rotation_option == EmbindConstant::kSimChooses)
      auras->corruption = std::make_shared<CorruptionDot>(*this);
    if (talents.unstable_affliction == 1 &&
        (settings.has_unstable_affliction || settings.rotation_option == EmbindConstant::kSimChooses))
      auras->unstable_affliction = std::make_shared<UnstableAfflictionDot>(*this);
    if (talents.siphon_life == 1 &&
        (settings.has_siphon_life || settings.rotation_option == EmbindConstant::kSimChooses))
      auras->siphon_life = std::make_shared<SiphonLifeDot>(*this);
    if (settings.has_immolate || settings.rotation_option == EmbindConstant::kSimChooses)
      auras->immolate = std::make_shared<ImmolateDot>(*this);
    if (settings.has_curse_of_agony || settings.has_curse_of_doom)
      auras->curse_of_agony = std::make_shared<CurseOfAgonyDot>(*this);
    if (settings.has_curse_of_the_elements)
      auras->curse_of_the_elements = std::make_shared<CurseOfTheElementsAura>(*this);
    if (settings.has_curse_of_recklessness)
      auras->curse_of_recklessness = std::make_shared<CurseOfRecklessnessAura>(*this);
    if (settings.has_curse_of_doom) auras->curse_of_doom = std::make_shared<CurseOfDoomDot>(*this);
    if (talents.nightfall > 0) auras->shadow_trance = std::make_shared<ShadowTranceAura>(*this);
    if (talents.amplify_curse == 1 &&
        (settings.has_amplify_curse || settings.rotation_option == EmbindConstant::kSimChooses))
      auras->amplify_curse = std::make_shared<AmplifyCurseAura>(*this);
  }
  if (selected_auras.mana_tide_totem) auras->mana_tide_totem = std::make_shared<ManaTideTotemAura>(*this);
  if (selected_auras.chipped_power_core) auras->chipped_power_core = std::make_shared<ChippedPowerCoreAura>(*this);
  if (selected_auras.cracked_power_core) auras->cracked_power_core = std::make_shared<CrackedPowerCoreAura>(*this);
  if (selected_auras.power_infusion) auras->power_infusion = std::make_shared<PowerInfusionAura>(*this);
  if (selected_auras.innervate) auras->innervate = std::make_shared<InnervateAura>(*this);
  if (selected_auras.bloodlust) auras->bloodlust = std::make_shared<BloodlustAura>(*this);
  if (selected_auras.destruction_potion) auras->destruction_potion = std::make_shared<DestructionPotionAura>(*this);
  if (selected_auras.flame_cap) auras->flame_cap = std::make_shared<FlameCapAura>(*this);
  if (settings.race == EmbindConstant::kOrc) auras->blood_fury = std::make_shared<BloodFuryAura>(*this);
  if (selected_auras.drums_of_battle)
    auras->drums_of_battle = std::make_shared<DrumsOfBattleAura>(*this);
  else if (selected_auras.drums_of_war)
    auras->drums_of_war = std::make_shared<DrumsOfWarAura>(*this);
  else if (selected_auras.drums_of_restoration)
    auras->drums_of_restoration = std::make_shared<DrumsOfRestorationAura>(*this);
  if (items.main_hand == 31336) auras->blade_of_wizardry = std::make_shared<BladeOfWizardryAura>(*this);
  if (items.neck == 34678)
    auras->shattered_sun_pendant_of_acumen = std::make_shared<ShatteredSunPendantOfAcumenAura>(*this);
  if (items.chest == 28602) auras->robe_of_the_elder_scribes = std::make_shared<RobeOfTheElderScribesAura>(*this);
  if (settings.meta_gem_id == 25893)
    auras->mystical_skyfire_diamond = std::make_shared<MysticalSkyfireDiamondAura>(*this);
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 28789) != equipped_trinket_ids.end())
    auras->eye_of_magtheridon = std::make_shared<EyeOfMagtheridonAura>(*this);
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 32493) != equipped_trinket_ids.end())
    auras->ashtongue_talisman_of_shadows = std::make_shared<AshtongueTalismanOfShadowsAura>(*this);
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 31856) != equipped_trinket_ids.end())
    auras->darkmoon_card_crusade = std::make_shared<DarkmoonCardCrusadeAura>(*this);
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 28785) != equipped_trinket_ids.end())
    auras->the_lightning_capacitor = std::make_shared<TheLightningCapacitorAura>(*this);
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 27683) != equipped_trinket_ids.end())
    auras->quagmirrans_eye = std::make_shared<QuagmirransEyeAura>(*this);
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 28418) != equipped_trinket_ids.end())
    auras->shiffars_nexus_horn = std::make_shared<ShiffarsNexusHornAura>(*this);
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 30626) != equipped_trinket_ids.end())
    auras->sextant_of_unstable_currents = std::make_shared<SextantOfUnstableCurrentsAura>(*this);
  if (items.ring_1 == 29305 || items.ring_2 == 29305)
    auras->band_of_the_eternal_sage = std::make_shared<BandOfTheEternalSageAura>(*this);
  if (items.ring_1 == 21190 || items.ring_2 == 21190)
    auras->wrath_of_cenarius = std::make_shared<WrathOfCenariusAura>(*this);
  if (sets.t4 >= 2) {
    auras->flameshadow = std::make_shared<FlameshadowAura>(*this);
    auras->shadowflame = std::make_shared<ShadowflameAura>(*this);
  }
  if (sets.spellstrike >= 2) auras->spellstrike = std::make_shared<SpellstrikeAura>(*this);
  if (sets.mana_etched >= 4) auras->mana_etched_4_set = std::make_shared<ManaEtched4SetAura>(*this);

  // Spells
  spells->life_tap = std::make_shared<LifeTap>(*this);
  if (settings.fight_type == EmbindConstant::kAoe) {
    spells->seed_of_corruption = std::make_shared<SeedOfCorruption>(*this);
  } else {
    if (settings.has_shadow_bolt || talents.nightfall > 0 || settings.rotation_option == EmbindConstant::kSimChooses)
      spells->shadow_bolt = std::make_shared<ShadowBolt>(*this);
    if (settings.has_incinerate || settings.rotation_option == EmbindConstant::kSimChooses)
      spells->incinerate = std::make_shared<Incinerate>(*this);
    if (settings.has_searing_pain || settings.rotation_option == EmbindConstant::kSimChooses)
      spells->searing_pain = std::make_shared<SearingPain>(*this);
    if (settings.has_death_coil || settings.rotation_option == EmbindConstant::kSimChooses)
      spells->death_coil = std::make_shared<DeathCoil>(*this);
    if (talents.conflagrate == 1 &&
        (settings.has_conflagrate || settings.rotation_option == EmbindConstant::kSimChooses))
      spells->conflagrate = std::make_shared<Conflagrate>(*this);
    if (talents.shadowburn == 1 &&
        (settings.has_shadow_burn || settings.rotation_option == EmbindConstant::kSimChooses))
      spells->shadowburn = std::make_shared<Shadowburn>(*this);
    if (talents.shadowfury == 1 && (settings.has_shadowfury || settings.rotation_option == EmbindConstant::kSimChooses))
      spells->shadowfury = std::make_shared<Shadowfury>(*this);
    if (auras->corruption != NULL) spells->corruption = std::make_shared<Corruption>(*this, nullptr, auras->corruption);
    if (auras->unstable_affliction != NULL)
      spells->unstable_affliction = std::make_shared<UnstableAffliction>(*this, nullptr, auras->unstable_affliction);
    if (auras->siphon_life != NULL)
      spells->siphon_life = std::make_shared<SiphonLife>(*this, nullptr, auras->siphon_life);
    if (auras->immolate != NULL) spells->immolate = std::make_shared<Immolate>(*this, nullptr, auras->immolate);
    if (auras->curse_of_agony != NULL || auras->curse_of_doom != NULL)
      spells->curse_of_agony = std::make_shared<CurseOfAgony>(*this, nullptr, auras->curse_of_agony);
    if (auras->curse_of_the_elements != NULL)
      spells->curse_of_the_elements = std::make_shared<CurseOfTheElements>(*this, auras->curse_of_the_elements);
    if (auras->curse_of_recklessness != NULL)
      spells->curse_of_recklessness = std::make_shared<CurseOfRecklessness>(*this, auras->curse_of_recklessness);
    if (auras->curse_of_doom != NULL)
      spells->curse_of_doom = std::make_shared<CurseOfDoom>(*this, nullptr, auras->curse_of_doom);
    if (auras->amplify_curse != NULL)
      spells->amplify_curse = std::make_shared<AmplifyCurse>(*this, auras->amplify_curse);
  }
  if (auras->mana_tide_totem != NULL)
    spells->mana_tide_totem = std::make_shared<ManaTideTotem>(*this, auras->mana_tide_totem);
  if (auras->chipped_power_core != NULL)
    spells->chipped_power_core = std::make_shared<ChippedPowerCore>(*this, auras->chipped_power_core);
  if (auras->cracked_power_core != NULL)
    spells->cracked_power_core = std::make_shared<CrackedPowerCore>(*this, auras->cracked_power_core);
  if (selected_auras.super_mana_potion) spells->super_mana_potion = std::make_shared<SuperManaPotion>(*this);
  if (selected_auras.demonic_rune) spells->demonic_rune = std::make_shared<DemonicRune>(*this);
  if (talents.dark_pact == 1 && (settings.has_dark_pact || settings.rotation_option == EmbindConstant::kSimChooses))
    spells->dark_pact = std::make_shared<DarkPact>(*this);
  if (auras->destruction_potion != NULL)
    spells->destruction_potion = std::make_shared<DestructionPotion>(*this, auras->destruction_potion);
  if (auras->flame_cap != NULL) spells->flame_cap = std::make_shared<FlameCap>(*this, auras->flame_cap);
  if (auras->blood_fury != NULL) spells->blood_fury = std::make_shared<BloodFury>(*this, auras->blood_fury);
  if (auras->drums_of_battle != NULL)
    spells->drums_of_battle = std::make_shared<DrumsOfBattle>(*this, auras->drums_of_battle);
  else if (auras->drums_of_war != NULL)
    spells->drums_of_war = std::make_shared<DrumsOfWar>(*this, auras->drums_of_war);
  else if (auras->drums_of_restoration != NULL)
    spells->drums_of_restoration = std::make_shared<DrumsOfRestoration>(*this, auras->drums_of_restoration);
  if (auras->blade_of_wizardry != NULL)
    spells->blade_of_wizardry = std::make_shared<BladeOfWizardry>(*this, auras->blade_of_wizardry);
  if (auras->shattered_sun_pendant_of_acumen != NULL)
    spells->shattered_sun_pendant_of_acumen =
        std::make_shared<ShatteredSunPendantOfAcumen>(*this, auras->shattered_sun_pendant_of_acumen);
  if (auras->robe_of_the_elder_scribes != NULL)
    spells->robe_of_the_elder_scribes =
        std::make_shared<RobeOfTheElderScribes>(*this, auras->robe_of_the_elder_scribes);
  if (auras->mystical_skyfire_diamond != NULL)
    spells->mystical_skyfire_diamond = std::make_shared<MysticalSkyfireDiamond>(*this, auras->mystical_skyfire_diamond);
  if (settings.meta_gem_id == 25901)
    spells->insightful_earthstorm_diamond = std::make_shared<InsightfulEarthstormDiamond>(*this);
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 34470) != equipped_trinket_ids.end())
    spells->timbals_focusing_crystal = std::make_shared<TimbalsFocusingCrystal>(*this);
  if (std::find(equipped_trinket_ids.begin(), equipped_trinket_ids.end(), 27922) != equipped_trinket_ids.end())
    spells->mark_of_defiance = std::make_shared<MarkOfDefiance>(*this);
  if (auras->the_lightning_capacitor != NULL)
    spells->the_lightning_capacitor = std::make_shared<TheLightningCapacitor>(*this, auras->the_lightning_capacitor);
  if (auras->quagmirrans_eye != NULL)
    spells->quagmirrans_eye = std::make_shared<QuagmirransEye>(*this, auras->quagmirrans_eye);
  if (auras->shiffars_nexus_horn != NULL)
    spells->shiffars_nexus_horn = std::make_shared<ShiffarsNexusHorn>(*this, auras->shiffars_nexus_horn);
  if (auras->sextant_of_unstable_currents != NULL)
    spells->sextant_of_unstable_currents =
        std::make_shared<SextantOfUnstableCurrents>(*this, auras->sextant_of_unstable_currents);
  if (items.ring_1 == 29305 || items.ring_2 == 29305)
    spells->band_of_the_eternal_sage = std::make_shared<BandOfTheEternalSage>(*this, auras->band_of_the_eternal_sage);
  if (selected_auras.judgement_of_wisdom) spells->judgement_of_wisdom = std::make_shared<JudgementOfWisdom>(*this);
  if (auras->power_infusion != NULL) {
    for (int i = 0; i < settings.power_infusion_amount; i++) {
      spells->power_infusion.push_back(std::make_shared<PowerInfusion>(*this, auras->power_infusion));
    }
  }
  if (auras->bloodlust != NULL) {
    for (int i = 0; i < settings.bloodlust_amount; i++) {
      spells->bloodlust.push_back(std::make_shared<Bloodlust>(*this, auras->bloodlust));
    }
  }
  if (auras->innervate != NULL) {
    for (int i = 0; i < settings.innervate_amount; i++) {
      spells->innervate.push_back(std::make_shared<Innervate>(*this, auras->innervate));
    }
  }

  // Set the filler property
  if (settings.has_incinerate) {
    filler = spells->incinerate;
  } else if (settings.has_searing_pain) {
    filler = spells->searing_pain;
  } else {
    filler = spells->shadow_bolt;
  }

  // Set the curseSpell and curseAura properties
  if (spells->curse_of_the_elements != NULL) {
    curse_spell = spells->curse_of_the_elements;
    curse_aura = auras->curse_of_the_elements;
  } else if (spells->curse_of_recklessness != NULL) {
    curse_spell = spells->curse_of_recklessness;
    curse_aura = auras->curse_of_recklessness;
  } else if (spells->curse_of_doom != NULL) {
    curse_spell = spells->curse_of_doom;
  } else if (spells->curse_of_agony != NULL) {
    curse_spell = spells->curse_of_agony;
  }

  SendPlayerInfoToCombatLog();
}

void Player::Reset() {
  cast_time_remaining = 0;
  gcd_remaining = 0;
  mp5_timer = 5;
  five_second_rule_timer = 5;
  stats.mana = stats.max_mana;
  power_infusions_ready = settings.power_infusion_amount;

  // Reset trinkets
  for (auto& trinket : trinkets) {
    trinket->Reset();
  }

  // Reset spells
  if (spells->life_tap != NULL) spells->life_tap->Reset();
  if (spells->seed_of_corruption != NULL) spells->seed_of_corruption->Reset();
  if (spells->shadow_bolt != NULL) spells->shadow_bolt->Reset();
  if (spells->incinerate != NULL) spells->incinerate->Reset();
  if (spells->searing_pain != NULL) spells->searing_pain->Reset();
  if (spells->corruption != NULL) spells->corruption->Reset();
  if (spells->unstable_affliction != NULL) spells->unstable_affliction->Reset();
  if (spells->siphon_life != NULL) spells->siphon_life->Reset();
  if (spells->immolate != NULL) spells->immolate->Reset();
  if (spells->curse_of_agony != NULL) spells->curse_of_agony->Reset();
  if (spells->curse_of_the_elements != NULL) spells->curse_of_the_elements->Reset();
  if (spells->curse_of_recklessness != NULL) spells->curse_of_recklessness->Reset();
  if (spells->curse_of_doom != NULL) spells->curse_of_doom->Reset();
  if (spells->conflagrate != NULL) spells->conflagrate->Reset();
  if (spells->shadowburn != NULL) spells->shadowburn->Reset();
  if (spells->death_coil != NULL) spells->death_coil->Reset();
  if (spells->shadowfury != NULL) spells->shadowfury->Reset();
  if (spells->amplify_curse != NULL) spells->amplify_curse->Reset();
  if (spells->dark_pact != NULL) spells->dark_pact->Reset();
  if (spells->destruction_potion != NULL) spells->destruction_potion->Reset();
  if (spells->super_mana_potion != NULL) spells->super_mana_potion->Reset();
  if (spells->demonic_rune != NULL) spells->demonic_rune->Reset();
  if (spells->flame_cap != NULL) spells->flame_cap->Reset();
  if (spells->blood_fury != NULL) spells->blood_fury->Reset();
  if (spells->drums_of_battle != NULL) spells->drums_of_battle->Reset();
  if (spells->drums_of_war != NULL) spells->drums_of_war->Reset();
  if (spells->drums_of_restoration != NULL) spells->drums_of_restoration->Reset();
  if (spells->blade_of_wizardry != NULL) spells->blade_of_wizardry->Reset();
  if (spells->shattered_sun_pendant_of_acumen != NULL) spells->shattered_sun_pendant_of_acumen->Reset();
  if (spells->robe_of_the_elder_scribes != NULL) spells->robe_of_the_elder_scribes->Reset();
  if (spells->mystical_skyfire_diamond != NULL) spells->mystical_skyfire_diamond->Reset();
  if (spells->insightful_earthstorm_diamond != NULL) spells->insightful_earthstorm_diamond->Reset();
  if (spells->timbals_focusing_crystal != NULL) spells->timbals_focusing_crystal->Reset();
  if (spells->mark_of_defiance != NULL) spells->mark_of_defiance->Reset();
  if (spells->the_lightning_capacitor != NULL) spells->the_lightning_capacitor->Reset();
  if (spells->quagmirrans_eye != NULL) spells->quagmirrans_eye->Reset();
  if (spells->shiffars_nexus_horn != NULL) spells->shiffars_nexus_horn->Reset();
  if (spells->sextant_of_unstable_currents != NULL) spells->sextant_of_unstable_currents->Reset();
  if (spells->band_of_the_eternal_sage != NULL) spells->band_of_the_eternal_sage->Reset();
  if (spells->chipped_power_core != NULL) spells->chipped_power_core->Reset();
  if (spells->cracked_power_core != NULL) spells->cracked_power_core->Reset();
  if (spells->mana_tide_totem != NULL) spells->mana_tide_totem->Reset();
  for (auto& pi : spells->power_infusion) {
    pi->Reset();
  }
  for (auto& bl : spells->bloodlust) {
    bl->Reset();
  }
  for (auto& innervate : spells->innervate) {
    innervate->Reset();
  }
}

void Player::EndAuras() {
  for (auto& trinket : trinkets) {
    if (trinket->active) {
      trinket->Fade();
    }
  }
  if (auras->corruption != NULL && auras->corruption->active) auras->corruption->Fade();
  if (auras->unstable_affliction != NULL && auras->unstable_affliction->active) auras->unstable_affliction->Fade();
  if (auras->siphon_life != NULL && auras->siphon_life->active) auras->siphon_life->Fade();
  if (auras->immolate != NULL && auras->immolate->active) auras->immolate->Fade();
  if (auras->curse_of_agony != NULL && auras->curse_of_agony->active) auras->curse_of_agony->Fade();
  if (auras->curse_of_doom != NULL && auras->curse_of_doom->active) auras->curse_of_doom->Fade();
  if (auras->improved_shadow_bolt != NULL && auras->improved_shadow_bolt->active) auras->improved_shadow_bolt->Fade();
  if (auras->curse_of_the_elements != NULL && auras->curse_of_the_elements->active)
    auras->curse_of_the_elements->Fade();
  if (auras->curse_of_recklessness != NULL && auras->curse_of_recklessness->active)
    auras->curse_of_recklessness->Fade();
  if (auras->shadow_trance != NULL && auras->shadow_trance->active) auras->shadow_trance->Fade();
  if (auras->amplify_curse != NULL && auras->amplify_curse->active) auras->amplify_curse->Fade();
  if (auras->power_infusion != NULL && auras->power_infusion->active) auras->power_infusion->Fade();
  if (auras->innervate != NULL && auras->innervate->active) auras->innervate->Fade();
  if (auras->blood_fury != NULL && auras->blood_fury->active) auras->blood_fury->Fade();
  if (auras->destruction_potion != NULL && auras->destruction_potion->active) auras->destruction_potion->Fade();
  if (auras->flame_cap != NULL && auras->flame_cap->active) auras->flame_cap->Fade();
  if (auras->bloodlust != NULL && auras->bloodlust->active) auras->bloodlust->Fade();
  if (auras->drums_of_battle != NULL && auras->drums_of_battle->active) auras->drums_of_battle->Fade();
  if (auras->drums_of_war != NULL && auras->drums_of_war->active) auras->drums_of_war->Fade();
  if (auras->drums_of_restoration != NULL && auras->drums_of_restoration->active) auras->drums_of_restoration->Fade();
  if (auras->band_of_the_eternal_sage != NULL && auras->band_of_the_eternal_sage->active)
    auras->band_of_the_eternal_sage->Fade();
  if (auras->wrath_of_cenarius != NULL && auras->wrath_of_cenarius->active) auras->wrath_of_cenarius->Fade();
  if (auras->blade_of_wizardry != NULL && auras->blade_of_wizardry->active) auras->blade_of_wizardry->Fade();
  if (auras->shattered_sun_pendant_of_acumen != NULL && auras->shattered_sun_pendant_of_acumen->active)
    auras->shattered_sun_pendant_of_acumen->Fade();
  if (auras->robe_of_the_elder_scribes != NULL && auras->robe_of_the_elder_scribes->active)
    auras->robe_of_the_elder_scribes->Fade();
  if (auras->mystical_skyfire_diamond != NULL && auras->mystical_skyfire_diamond->active)
    auras->mystical_skyfire_diamond->Fade();
  if (auras->eye_of_magtheridon != NULL && auras->eye_of_magtheridon->active) auras->eye_of_magtheridon->Fade();
  if (auras->sextant_of_unstable_currents != NULL && auras->sextant_of_unstable_currents->active)
    auras->sextant_of_unstable_currents->Fade();
  if (auras->quagmirrans_eye != NULL && auras->quagmirrans_eye->active) auras->quagmirrans_eye->Fade();
  if (auras->shiffars_nexus_horn != NULL && auras->shiffars_nexus_horn->active) auras->shiffars_nexus_horn->Fade();
  if (auras->ashtongue_talisman_of_shadows != NULL && auras->ashtongue_talisman_of_shadows->active)
    auras->ashtongue_talisman_of_shadows->Fade();
  if (auras->darkmoon_card_crusade != NULL && auras->darkmoon_card_crusade->active)
    auras->darkmoon_card_crusade->Fade();
  if (auras->the_lightning_capacitor != NULL && auras->the_lightning_capacitor->active)
    auras->the_lightning_capacitor->Fade();
  if (auras->flameshadow != NULL && auras->flameshadow->active) auras->flameshadow->Fade();
  if (auras->shadowflame != NULL && auras->shadowflame->active) auras->shadowflame->Fade();
  if (auras->spellstrike != NULL && auras->spellstrike->active) auras->spellstrike->Fade();
  if (auras->mana_etched_4_set != NULL && auras->mana_etched_4_set->active) auras->mana_etched_4_set->Fade();
  if (auras->chipped_power_core != NULL && auras->chipped_power_core->active) auras->chipped_power_core->Fade();
  if (auras->cracked_power_core != NULL && auras->cracked_power_core->active) auras->cracked_power_core->Fade();
  if (auras->mana_tide_totem != NULL && auras->mana_tide_totem->active) auras->mana_tide_totem->Fade();
}

double Player::GetHastePercent() {
  double haste_percent = stats.haste_percent;

  // If both Bloodlust and Power Infusion are active then remove the 20% PI
  // bonus since they don't stack
  if (auras->bloodlust != NULL && auras->power_infusion != NULL && auras->bloodlust->active &&
      auras->power_infusion->active) {
    haste_percent /= (1 + auras->power_infusion->stats->haste_percent / 100);
  }

  return haste_percent * (1 + stats.haste_rating / kHasteRatingPerPercent / 100.0);
}

double Player::GetGcdValue(const std::shared_ptr<Spell>& spell) {
  if (spells->shadowfury == NULL || spell != spells->shadowfury) {
    return std::max(kMinimumGcdValue, round((kGcdValue / GetHastePercent()) * 10000) / 10000);
  }
  return 0;
}

double Player::GetSpellPower(SpellSchool school) {
  double spell_power = stats.spell_power + demonic_knowledge_spell_power;
  if (sets.spellfire == 3) {
    spell_power += stats.GetIntellect() * 0.07;
  }
  if (school == SpellSchool::kShadow) {
    spell_power += stats.shadow_power;
  } else if (school == SpellSchool::kFire) {
    spell_power += stats.fire_power;
  }
  return spell_power;
}

double Player::GetCritChance(SpellType spell_type) {
  double crit_chance = stats.crit_chance + (stats.GetIntellect() * kCritChancePerIntellect) +
                       (stats.crit_rating / kCritRatingPerPercent);
  if (spell_type != SpellType::kDestruction) {
    crit_chance -= talents.devastation;
  }
  return crit_chance;
}

double Player::GetHitChance(SpellType spell_type) {
  double hit_chance = stats.hit_chance + stats.extra_hit_chance;
  if (spell_type == SpellType::kAffliction) {
    hit_chance += talents.suppression * 2;
  }
  return std::min(99.0, hit_chance);
}

bool Player::IsCrit(SpellType spell_type, double extra_crit) { return RollRng(GetCritChance(spell_type) + extra_crit); }

bool Player::IsHit(SpellType spell_type) {
  const bool kIsHit = RollRng(GetHitChance(spell_type));
  if (!kIsHit && auras->eye_of_magtheridon != NULL) {
    auras->eye_of_magtheridon->Apply();
  }
  return kIsHit;
}

int Player::GetRand() { return rng.range(0, 100 * kFloatNumberMultiplier); }

bool Player::RollRng(double chance) { return GetRand() <= chance * kFloatNumberMultiplier; }

// formula from
// https://web.archive.org/web/20161015101615/https://dwarfpriest.wordpress.com/2008/01/07/spell-hit-spell-penetration-and-resistances/
// && https://royalgiraffe.github.io/resist-guide
double Player::GetBaseHitChance(int player_level, int enemy_level) {
  const int kLevelDifference = enemy_level - player_level;
  return kLevelDifference <= 2   ? std::min(99, 100 - kLevelDifference - 4)
         : kLevelDifference == 3 ? 83
         : kLevelDifference >= 4 ? 83 - 11 * kLevelDifference
                                 : 0;
}

void Player::UseCooldowns(double fight_time_remaining) {
  // Only use PI if Bloodlust isn't selected or if Bloodlust isn't active since they don't stack, or if there are enough
  // Power Infusions available to last until the end of the fight for the mana cost reduction
  if (!spells->power_infusion.empty() && !auras->power_infusion->active &&
      (spells->bloodlust.empty() || !auras->bloodlust->active ||
       power_infusions_ready * auras->power_infusion->duration >= fight_time_remaining)) {
    for (auto pi : spells->power_infusion) {
      if (pi->Ready()) {
        pi->StartCast();
        break;
      }
    }
  }
  // TODO don't use innervate until x% mana
  if (!spells->innervate.empty() && !auras->innervate->active) {
    for (auto innervate : spells->innervate) {
      if (innervate->Ready()) {
        innervate->StartCast();
        break;
      }
    }
  }
  if (spells->chipped_power_core != NULL && spells->chipped_power_core->Ready()) {
    spells->chipped_power_core->StartCast();
  } else if (spells->cracked_power_core != NULL && spells->cracked_power_core->Ready()) {
    spells->cracked_power_core->StartCast();
  }
  if (spells->destruction_potion != NULL && spells->destruction_potion->Ready()) {
    spells->destruction_potion->StartCast();
  }
  if (spells->flame_cap != NULL && spells->flame_cap->Ready()) {
    spells->flame_cap->StartCast();
  }
  if (spells->blood_fury != NULL && spells->blood_fury->Ready()) {
    spells->blood_fury->StartCast();
  }
  for (int i = 0; i < trinkets.size(); i++) {
    if (trinkets[i]->Ready()) {
      trinkets[i]->Use();
      // Set the other on-use trinket (if another is equipped) on cooldown for
      // the duration of the trinket just used if the trinkets share cooldown
      const int kOtherTrinketSlot = i == 1 ? 0 : 1;
      if (trinkets.size() > kOtherTrinketSlot && trinkets[kOtherTrinketSlot] != NULL &&
          trinkets[kOtherTrinketSlot]->shares_cooldown && trinkets[i]->shares_cooldown) {
        trinkets[kOtherTrinketSlot]->cooldown_remaining =
            std::max(trinkets[kOtherTrinketSlot]->cooldown_remaining, static_cast<double>(trinkets[i]->duration));
      }
    }
  }
}

void Player::CastLifeTapOrDarkPact() {
  if (spells->dark_pact != NULL && spells->dark_pact->Ready()) {
    spells->dark_pact->StartCast();
  } else {
    spells->life_tap->StartCast();
  }
}

double Player::GetPartialResistMultiplier(SpellSchool school) {
  if (school != SpellSchool::kShadow && school != SpellSchool::kFire) {
    return 1;
  }

  const int kEnemyResist = school == SpellSchool::kShadow ? settings.enemy_shadow_resist : settings.enemy_fire_resist;

  return 1.0 - ((75 * kEnemyResist) / (kLevel * 5)) / 100.0;
}

void Player::AddIterationDamageAndMana(const std::string& spell_name, int mana_gain, int damage) {
  const int kCurrentManaGain = combat_log_breakdown.at(spell_name)->iteration_mana_gain;
  const int kCurrentDamage = combat_log_breakdown.at(spell_name)->iteration_damage;

  // Check for integer overflow
  if (kCurrentManaGain + mana_gain < 0 || kCurrentDamage + damage < 0) {
    PostIterationDamageAndMana(spell_name);
  }

  combat_log_breakdown.at(spell_name)->iteration_mana_gain += mana_gain;
  combat_log_breakdown.at(spell_name)->iteration_damage += damage;
}

void Player::PostIterationDamageAndMana(const std::string& spell_name) {
  PostCombatLogBreakdownVector(spell_name.c_str(), combat_log_breakdown.at(spell_name)->iteration_mana_gain,
                               combat_log_breakdown.at(spell_name)->iteration_damage);
  combat_log_breakdown.at(spell_name)->iteration_damage = 0;
  combat_log_breakdown.at(spell_name)->iteration_mana_gain = 0;
}

void Player::ThrowError(const std::string& error) {
  SendCombatLogEntries();
  ErrorCallback(error.c_str());
  throw std::runtime_error(error);
}

bool Player::ShouldWriteToCombatLog() { return iteration == 10 && settings.equipped_item_simulation; }

void Player::SendCombatLogEntries() {
  for (const auto& value : combat_log_entries) {
    CombatLogUpdate(value.c_str());
  }
}

void Player::CombatLog(const std::string& entry) {
  combat_log_entries.push_back("|" + DoubleToString(fight_time, 4) + "| " + entry);
}

void Player::SendPlayerInfoToCombatLog() {
  combat_log_entries.push_back("---------------- Player stats ----------------");
  combat_log_entries.push_back("Health: " + DoubleToString(round(stats.health)));
  combat_log_entries.push_back("Mana: " + DoubleToString(round(stats.max_mana)));
  combat_log_entries.push_back("Stamina: " + DoubleToString(round(stats.GetStamina())));
  combat_log_entries.push_back("Intellect: " + DoubleToString(round(stats.GetIntellect())));
  combat_log_entries.push_back("Spell Power: " + DoubleToString(round(GetSpellPower())));
  combat_log_entries.push_back("Shadow Power: " + std::to_string(stats.shadow_power));
  combat_log_entries.push_back("Fire Power: " + std::to_string(stats.fire_power));
  combat_log_entries.push_back(
      "Crit Chance: " + DoubleToString(round(GetCritChance(SpellType::kDestruction) * 100) / 100, 2) + "%");
  combat_log_entries.push_back(
      "Hit Chance: " + DoubleToString(std::min(16.0, round((stats.extra_hit_chance) * 100) / 100), 2) + "%");
  combat_log_entries.push_back(
      "Haste: " + DoubleToString(round((stats.haste_rating / kHasteRatingPerPercent) * 100) / 100, 2) + "%");
  combat_log_entries.push_back("Shadow Modifier: " + DoubleToString(stats.shadow_modifier * 100, 2) + "%");
  combat_log_entries.push_back("Fire Modifier: " + DoubleToString(stats.fire_modifier * 100, 2) + "%");
  combat_log_entries.push_back("MP5: " + std::to_string(stats.mp5));
  combat_log_entries.push_back("Spell Penetration: " + std::to_string(stats.spell_penetration));
  if (pet != NULL) {
    combat_log_entries.push_back("---------------- Pet stats ----------------");
    combat_log_entries.push_back("Stamina: " + DoubleToString(pet->GetStamina()));
    combat_log_entries.push_back("Intellect: " + DoubleToString(pet->GetIntellect()));
    combat_log_entries.push_back("Strength: " + DoubleToString(pet->GetStrength()));
    combat_log_entries.push_back("Agility: " + DoubleToString(pet->GetAgility()));
    combat_log_entries.push_back("Spirit: " + DoubleToString(pet->GetSpirit()));
    combat_log_entries.push_back("Attack Power: " + DoubleToString(round(pet->stats->attack_power)) +
                                 " (without attack power % modifiers)");
    combat_log_entries.push_back("Spell Power: " + DoubleToString(pet->stats->spell_power));
    combat_log_entries.push_back("Mana: " + DoubleToString(pet->stats->max_mana));
    combat_log_entries.push_back("MP5: " + std::to_string(pet->stats->mp5));
    if (pet->pet_type == PetType::kMelee) {
      combat_log_entries.push_back(
          "Physical Hit Chance: " + DoubleToString(round(pet->GetMeleeHitChance() * 100) / 100.0, 2) + "%");
      combat_log_entries.push_back(
          "Physical Crit Chance: " + DoubleToString(round(pet->GetMeleeCritChance() * 100) / 100.0, 2) + "% (" +
          DoubleToString(pet->crit_suppression, 2) + "% Crit Suppression Applied)");
      combat_log_entries.push_back(
          "Glancing Blow Chance: " + DoubleToString(round(pet->glancing_blow_chance * 100) / 100.0, 2) + "%");
      combat_log_entries.push_back("Attack Power Modifier: " +
                                   DoubleToString(round(pet->stats->attack_power_modifier * 10000) / 100.0, 2) + "%");
    }
    if (pet->pet == PetName::kImp || pet->pet == PetName::kSuccubus) {
      combat_log_entries.push_back(
          "Spell Hit Chance: " + DoubleToString(round(pet->GetSpellHitChance() * 100) / 100.0, 2) + "%");
      combat_log_entries.push_back(
          "Spell Crit Chance: " + DoubleToString(round(pet->GetSpellCritChance() * 100) / 100.0, 2) + "%");
    }
    combat_log_entries.push_back(
        "Damage Modifier: " + DoubleToString(round(pet->stats->damage_modifier * 10000) / 100, 2) + "%");
  }
  combat_log_entries.push_back("---------------- Enemy stats ----------------");
  combat_log_entries.push_back("Level: " + std::to_string(settings.enemy_level));
  combat_log_entries.push_back("Shadow Resistance: " + std::to_string(settings.enemy_shadow_resist));
  combat_log_entries.push_back("Fire Resistance: " + std::to_string(settings.enemy_fire_resist));
  if (pet != NULL && pet->pet != PetName::kImp) {
    combat_log_entries.push_back("Dodge Chance: " + DoubleToString(pet->enemy_dodge_chance) + "%");
    combat_log_entries.push_back("Armor: " + std::to_string(settings.enemy_armor));
    combat_log_entries.push_back(
        "Damage Reduction From Armor: " + DoubleToString(round((1 - pet->armor_multiplier) * 10000) / 100.0, 2) + "%");
  }
  combat_log_entries.push_back("---------------------------------------------");
}