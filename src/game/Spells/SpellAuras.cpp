/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Server/WorldSession.h"
#include "Server/Opcodes.h"
#include "Log.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"
#include "Spells/SpellMgr.h"
#include "Entities/Player.h"
#include "Entities/Unit.h"
#include "Spells/Spell.h"
#include "Entities/DynamicObject.h"
#include "Groups/Group.h"
#include "Entities/UpdateData.h"
#include "Globals/ObjectAccessor.h"
#include "Policies/Singleton.h"
#include "Entities/Totem.h"
#include "Entities/Creature.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "AI/BaseAI/CreatureAI.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Util.h"
#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Entities/Vehicle.h"
#include "Grids/CellImpl.h"
#include "Tools/Language.h"
#include "Maps/MapManager.h"
#include "Loot/LootMgr.h"
#include "Entities/TemporarySpawn.h"
#include "Maps/InstanceData.h"
#include "AI/ScriptDevAI/include/sc_grid_searchers.h"

#define NULL_AURA_SLOT 0xFF

/**
 * An array with all the different handlers for taking care of
 * the various aura types that are defined in AuraType.
 */
pAuraHandler AuraHandler[TOTAL_AURAS] =
{
    &Aura::HandleNULL,                                      //  0 SPELL_AURA_NONE
    &Aura::HandleBindSight,                                 //  1 SPELL_AURA_BIND_SIGHT
    &Aura::HandleModPossess,                                //  2 SPELL_AURA_MOD_POSSESS
    &Aura::HandlePeriodicDamage,                            //  3 SPELL_AURA_PERIODIC_DAMAGE
    &Aura::HandleAuraDummy,                                 //  4 SPELL_AURA_DUMMY
    &Aura::HandleModConfuse,                                //  5 SPELL_AURA_MOD_CONFUSE
    &Aura::HandleModCharm,                                  //  6 SPELL_AURA_MOD_CHARM
    &Aura::HandleModFear,                                   //  7 SPELL_AURA_MOD_FEAR
    &Aura::HandlePeriodicHeal,                              //  8 SPELL_AURA_PERIODIC_HEAL
    &Aura::HandleModAttackSpeed,                            //  9 SPELL_AURA_MOD_ATTACKSPEED
    &Aura::HandleModThreat,                                 // 10 SPELL_AURA_MOD_THREAT
    &Aura::HandleModTaunt,                                  // 11 SPELL_AURA_MOD_TAUNT
    &Aura::HandleAuraModStun,                               // 12 SPELL_AURA_MOD_STUN
    &Aura::HandleModDamageDone,                             // 13 SPELL_AURA_MOD_DAMAGE_DONE
    &Aura::HandleNoImmediateEffect,                         // 14 SPELL_AURA_MOD_DAMAGE_TAKEN   implemented in Unit::MeleeDamageBonusTaken and Unit::SpellBaseDamageBonusTaken
    &Aura::HandleNoImmediateEffect,                         // 15 SPELL_AURA_DAMAGE_SHIELD      implemented in Unit::DealMeleeDamage
    &Aura::HandleModStealth,                                // 16 SPELL_AURA_MOD_STEALTH
    &Aura::HandleNoImmediateEffect,                         // 17 SPELL_AURA_MOD_STEALTH_DETECT implemented in Unit::IsVisibleForOrDetect
    &Aura::HandleInvisibility,                              // 18 SPELL_AURA_MOD_INVISIBILITY
    &Aura::HandleInvisibilityDetect,                        // 19 SPELL_AURA_MOD_INVISIBILITY_DETECTION
    &Aura::HandleAuraModTotalHealthPercentRegen,            // 20 SPELL_AURA_OBS_MOD_HEALTH
    &Aura::HandleAuraModTotalManaPercentRegen,              // 21 SPELL_AURA_OBS_MOD_MANA
    &Aura::HandleAuraModResistance,                         // 22 SPELL_AURA_MOD_RESISTANCE
    &Aura::HandlePeriodicTriggerSpell,                      // 23 SPELL_AURA_PERIODIC_TRIGGER_SPELL
    &Aura::HandlePeriodicEnergize,                          // 24 SPELL_AURA_PERIODIC_ENERGIZE
    &Aura::HandleAuraModPacify,                             // 25 SPELL_AURA_MOD_PACIFY
    &Aura::HandleAuraModRoot,                               // 26 SPELL_AURA_MOD_ROOT
    &Aura::HandleAuraModSilence,                            // 27 SPELL_AURA_MOD_SILENCE
    &Aura::HandleNoImmediateEffect,                         // 28 SPELL_AURA_REFLECT_SPELLS        implement in Unit::SpellHitResult
    &Aura::HandleAuraModStat,                               // 29 SPELL_AURA_MOD_STAT
    &Aura::HandleAuraModSkill,                              // 30 SPELL_AURA_MOD_SKILL
    &Aura::HandleAuraModIncreaseSpeed,                      // 31 SPELL_AURA_MOD_INCREASE_SPEED
    &Aura::HandleAuraModIncreaseMountedSpeed,               // 32 SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED
    &Aura::HandleAuraModDecreaseSpeed,                      // 33 SPELL_AURA_MOD_DECREASE_SPEED
    &Aura::HandleAuraModIncreaseHealth,                     // 34 SPELL_AURA_MOD_INCREASE_HEALTH
    &Aura::HandleAuraModIncreaseEnergy,                     // 35 SPELL_AURA_MOD_INCREASE_ENERGY
    &Aura::HandleAuraModShapeshift,                         // 36 SPELL_AURA_MOD_SHAPESHIFT
    &Aura::HandleAuraModEffectImmunity,                     // 37 SPELL_AURA_EFFECT_IMMUNITY
    &Aura::HandleAuraModStateImmunity,                      // 38 SPELL_AURA_STATE_IMMUNITY
    &Aura::HandleAuraModSchoolImmunity,                     // 39 SPELL_AURA_SCHOOL_IMMUNITY
    &Aura::HandleAuraModDmgImmunity,                        // 40 SPELL_AURA_DAMAGE_IMMUNITY
    &Aura::HandleAuraModDispelImmunity,                     // 41 SPELL_AURA_DISPEL_IMMUNITY
    &Aura::HandleAuraProcTriggerSpell,                      // 42 SPELL_AURA_PROC_TRIGGER_SPELL  implemented in Unit::ProcDamageAndSpellFor and Unit::HandleProcTriggerSpell
    &Aura::HandleNoImmediateEffect,                         // 43 SPELL_AURA_PROC_TRIGGER_DAMAGE implemented in Unit::ProcDamageAndSpellFor
    &Aura::HandleAuraTrackCreatures,                        // 44 SPELL_AURA_TRACK_CREATURES
    &Aura::HandleAuraTrackResources,                        // 45 SPELL_AURA_TRACK_RESOURCES
    &Aura::HandleUnused,                                    // 46 SPELL_AURA_46 (used in test spells 54054 and 54058, and spell 48050) (3.0.8a-3.2.2a)
    &Aura::HandleAuraModParryPercent,                       // 47 SPELL_AURA_MOD_PARRY_PERCENT
    &Aura::HandleNoImmediateEffect,                         // 48 SPELL_AURA_PERIODIC_TRIGGER_BY_CLIENT (Client periodic trigger spell by self (3 spells in 3.3.5a)). Implemented in pet/player cast chains.
    &Aura::HandleAuraModDodgePercent,                       // 49 SPELL_AURA_MOD_DODGE_PERCENT
    &Aura::HandleNoImmediateEffect,                         // 50 SPELL_AURA_MOD_CRITICAL_HEALING_AMOUNT implemented in Unit::SpellCriticalHealingBonus
    &Aura::HandleAuraModBlockPercent,                       // 51 SPELL_AURA_MOD_BLOCK_PERCENT
    &Aura::HandleAuraModCritPercent,                        // 52 SPELL_AURA_MOD_CRIT_PERCENT
    &Aura::HandlePeriodicLeech,                             // 53 SPELL_AURA_PERIODIC_LEECH
    &Aura::HandleModHitChance,                              // 54 SPELL_AURA_MOD_HIT_CHANCE
    &Aura::HandleModSpellHitChance,                         // 55 SPELL_AURA_MOD_SPELL_HIT_CHANCE
    &Aura::HandleAuraTransform,                             // 56 SPELL_AURA_TRANSFORM
    &Aura::HandleModSpellCritChance,                        // 57 SPELL_AURA_MOD_SPELL_CRIT_CHANCE
    &Aura::HandleAuraModIncreaseSwimSpeed,                  // 58 SPELL_AURA_MOD_INCREASE_SWIM_SPEED
    &Aura::HandleNoImmediateEffect,                         // 59 SPELL_AURA_MOD_DAMAGE_DONE_CREATURE implemented in Unit::MeleeDamageBonusDone and Unit::SpellDamageBonusDone
    &Aura::HandleAuraModPacifyAndSilence,                   // 60 SPELL_AURA_MOD_PACIFY_SILENCE
    &Aura::HandleAuraModScale,                              // 61 SPELL_AURA_MOD_SCALE
    &Aura::HandlePeriodicHealthFunnel,                      // 62 SPELL_AURA_PERIODIC_HEALTH_FUNNEL
    &Aura::HandleUnused,                                    // 63 unused (3.0.8a-3.2.2a) old SPELL_AURA_PERIODIC_MANA_FUNNEL
    &Aura::HandlePeriodicManaLeech,                         // 64 SPELL_AURA_PERIODIC_MANA_LEECH
    &Aura::HandleModCastingSpeed,                           // 65 SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK
    &Aura::HandleFeignDeath,                                // 66 SPELL_AURA_FEIGN_DEATH
    &Aura::HandleAuraModDisarm,                             // 67 SPELL_AURA_MOD_DISARM
    &Aura::HandleAuraModStalked,                            // 68 SPELL_AURA_MOD_STALKED
    &Aura::HandleSchoolAbsorb,                              // 69 SPELL_AURA_SCHOOL_ABSORB implemented in Unit::CalculateAbsorbAndResist
    &Aura::HandleUnused,                                    // 70 SPELL_AURA_EXTRA_ATTACKS      Useless, used by only one spell 41560 that has only visual effect (3.2.2a)
    &Aura::HandleModSpellCritChanceShool,                   // 71 SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL
    &Aura::HandleModPowerCostPCT,                           // 72 SPELL_AURA_MOD_POWER_COST_SCHOOL_PCT
    &Aura::HandleModPowerCost,                              // 73 SPELL_AURA_MOD_POWER_COST_SCHOOL
    &Aura::HandleNoImmediateEffect,                         // 74 SPELL_AURA_REFLECT_SPELLS_SCHOOL  implemented in Unit::SpellHitResult
    &Aura::HandleNoImmediateEffect,                         // 75 SPELL_AURA_MOD_LANGUAGE           implemented in WorldSession::HandleMessagechatOpcode
    &Aura::HandleFarSight,                                  // 76 SPELL_AURA_FAR_SIGHT
    &Aura::HandleModMechanicImmunity,                       // 77 SPELL_AURA_MECHANIC_IMMUNITY
    &Aura::HandleAuraMounted,                               // 78 SPELL_AURA_MOUNTED
    &Aura::HandleModDamagePercentDone,                      // 79 SPELL_AURA_MOD_DAMAGE_PERCENT_DONE
    &Aura::HandleModPercentStat,                            // 80 SPELL_AURA_MOD_PERCENT_STAT
    &Aura::HandleNoImmediateEffect,                         // 81 SPELL_AURA_SPLIT_DAMAGE_PCT       implemented in Unit::CalculateAbsorbAndResist
    &Aura::HandleWaterBreathing,                            // 82 SPELL_AURA_WATER_BREATHING
    &Aura::HandleModBaseResistance,                         // 83 SPELL_AURA_MOD_BASE_RESISTANCE
    &Aura::HandleModRegen,                                  // 84 SPELL_AURA_MOD_REGEN
    &Aura::HandleModPowerRegen,                             // 85 SPELL_AURA_MOD_POWER_REGEN
    &Aura::HandleChannelDeathItem,                          // 86 SPELL_AURA_CHANNEL_DEATH_ITEM
    &Aura::HandleDamagePercentTaken,                        // 87 SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN implemented in Unit::MeleeDamageBonusTaken and Unit::SpellDamageBonusTaken
    &Aura::HandleNoImmediateEffect,                         // 88 SPELL_AURA_MOD_HEALTH_REGEN_PERCENT implemented in Player::RegenerateHealth
    &Aura::HandlePeriodicDamagePCT,                         // 89 SPELL_AURA_PERIODIC_DAMAGE_PERCENT
    &Aura::HandleUnused,                                    // 90 unused (3.0.8a-3.2.2a) old SPELL_AURA_MOD_RESIST_CHANCE
    &Aura::HandleNoImmediateEffect,                         // 91 SPELL_AURA_MOD_DETECT_RANGE implemented in Creature::GetAttackDistance
    &Aura::HandlePreventFleeing,                            // 92 SPELL_AURA_PREVENTS_FLEEING
    &Aura::HandleModUnattackable,                           // 93 SPELL_AURA_MOD_UNATTACKABLE
    &Aura::HandleNoImmediateEffect,                         // 94 SPELL_AURA_INTERRUPT_REGEN implemented in Player::RegenerateAll
    &Aura::HandleAuraGhost,                                 // 95 SPELL_AURA_GHOST
    &Aura::HandleNoImmediateEffect,                         // 96 SPELL_AURA_SPELL_MAGNET implemented in Unit::SelectMagnetTarget
    &Aura::HandleManaShield,                                // 97 SPELL_AURA_MANA_SHIELD implemented in Unit::CalculateAbsorbAndResist
    &Aura::HandleAuraModSkill,                              // 98 SPELL_AURA_MOD_SKILL_TALENT
    &Aura::HandleAuraModAttackPower,                        // 99 SPELL_AURA_MOD_ATTACK_POWER
    &Aura::HandleUnused,                                    //100 SPELL_AURA_AURAS_VISIBLE obsolete 3.x? all player can see all auras now, but still have 2 spells including GM-spell (1852,2855)
    &Aura::HandleModResistancePercent,                      //101 SPELL_AURA_MOD_RESISTANCE_PCT
    &Aura::HandleNoImmediateEffect,                         //102 SPELL_AURA_MOD_MELEE_ATTACK_POWER_VERSUS implemented in Unit::MeleeDamageBonusDone
    &Aura::HandleAuraModTotalThreat,                        //103 SPELL_AURA_MOD_TOTAL_THREAT
    &Aura::HandleAuraWaterWalk,                             //104 SPELL_AURA_WATER_WALK
    &Aura::HandleAuraFeatherFall,                           //105 SPELL_AURA_FEATHER_FALL
    &Aura::HandleAuraHover,                                 //106 SPELL_AURA_HOVER
    &Aura::HandleAddModifier,                               //107 SPELL_AURA_ADD_FLAT_MODIFIER
    &Aura::HandleAddModifier,                               //108 SPELL_AURA_ADD_PCT_MODIFIER
    &Aura::HandleNoImmediateEffect,                         //109 SPELL_AURA_ADD_TARGET_TRIGGER
    &Aura::HandleModPowerRegenPCT,                          //110 SPELL_AURA_MOD_POWER_REGEN_PERCENT
    &Aura::HandleNoImmediateEffect,                         //111 SPELL_AURA_ADD_CASTER_HIT_TRIGGER implemented in Unit::SelectMagnetTarget
    &Aura::HandleNoImmediateEffect,                         //112 SPELL_AURA_OVERRIDE_CLASS_SCRIPTS implemented in diff functions.
    &Aura::HandleNoImmediateEffect,                         //113 SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN implemented in Unit::MeleeDamageBonusTaken
    &Aura::HandleNoImmediateEffect,                         //114 SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN_PCT implemented in Unit::MeleeDamageBonusTaken
    &Aura::HandleNoImmediateEffect,                         //115 SPELL_AURA_MOD_HEALING                 implemented in Unit::SpellBaseHealingBonusTaken
    &Aura::HandleNoImmediateEffect,                         //116 SPELL_AURA_MOD_REGEN_DURING_COMBAT     imppemented in Player::RegenerateAll and Player::RegenerateHealth
    &Aura::HandleNoImmediateEffect,                         //117 SPELL_AURA_MOD_MECHANIC_RESISTANCE     implemented in Unit::MagicSpellHitResult
    &Aura::HandleNoImmediateEffect,                         //118 SPELL_AURA_MOD_HEALING_PCT             implemented in Unit::SpellHealingBonusTaken
    &Aura::HandleUnused,                                    //119 unused (3.0.8a-3.2.2a) old SPELL_AURA_SHARE_PET_TRACKING
    &Aura::HandleAuraUntrackable,                           //120 SPELL_AURA_UNTRACKABLE
    &Aura::HandleAuraEmpathy,                               //121 SPELL_AURA_EMPATHY
    &Aura::HandleModOffhandDamagePercent,                   //122 SPELL_AURA_MOD_OFFHAND_DAMAGE_PCT
    &Aura::HandleModTargetResistance,                       //123 SPELL_AURA_MOD_TARGET_RESISTANCE
    &Aura::HandleAuraModRangedAttackPower,                  //124 SPELL_AURA_MOD_RANGED_ATTACK_POWER
    &Aura::HandleNoImmediateEffect,                         //125 SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN implemented in Unit::MeleeDamageBonusTaken
    &Aura::HandleNoImmediateEffect,                         //126 SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN_PCT implemented in Unit::MeleeDamageBonusTaken
    &Aura::HandleNoImmediateEffect,                         //127 SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS implemented in Unit::MeleeDamageBonusDone
    &Aura::HandleModPossessPet,                             //128 SPELL_AURA_MOD_POSSESS_PET
    &Aura::HandleAuraModIncreaseSpeed,                      //129 SPELL_AURA_MOD_SPEED_ALWAYS
    &Aura::HandleAuraModIncreaseMountedSpeed,               //130 SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS
    &Aura::HandleNoImmediateEffect,                         //131 SPELL_AURA_MOD_RANGED_ATTACK_POWER_VERSUS implemented in Unit::MeleeDamageBonusDone
    &Aura::HandleAuraModIncreaseEnergyPercent,              //132 SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT
    &Aura::HandleAuraModIncreaseHealthPercent,              //133 SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT
    &Aura::HandleAuraModRegenInterrupt,                     //134 SPELL_AURA_MOD_MANA_REGEN_INTERRUPT
    &Aura::HandleModHealingDone,                            //135 SPELL_AURA_MOD_HEALING_DONE
    &Aura::HandleNoImmediateEffect,                         //136 SPELL_AURA_MOD_HEALING_DONE_PERCENT   implemented in Unit::SpellHealingBonusDone
    &Aura::HandleModTotalPercentStat,                       //137 SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE
    &Aura::HandleModMeleeSpeedPct,                          //138 SPELL_AURA_MOD_MELEE_HASTE
    &Aura::HandleForceReaction,                             //139 SPELL_AURA_FORCE_REACTION
    &Aura::HandleAuraModRangedHaste,                        //140 SPELL_AURA_MOD_RANGED_HASTE
    &Aura::HandleRangedAmmoHaste,                           //141 SPELL_AURA_MOD_RANGED_AMMO_HASTE
    &Aura::HandleAuraModBaseResistancePCT,                  //142 SPELL_AURA_MOD_BASE_RESISTANCE_PCT
    &Aura::HandleAuraModResistanceExclusive,                //143 SPELL_AURA_MOD_RESISTANCE_EXCLUSIVE
    &Aura::HandleAuraSafeFall,                              //144 SPELL_AURA_SAFE_FALL                  implemented in WorldSession::HandleMovementOpcodes
    &Aura::HandleAuraModPetTalentsPoints,                   //145 SPELL_AURA_MOD_PET_TALENT_POINTS
    &Aura::HandleNoImmediateEffect,                         //146 SPELL_AURA_ALLOW_TAME_PET_TYPE        implemented in Player::CanTameExoticPets
    &Aura::HandleModMechanicImmunityMask,                   //147 SPELL_AURA_MECHANIC_IMMUNITY_MASK     implemented in Unit::IsImmuneToSpell and Unit::IsImmuneToSpellEffect (check part)
    &Aura::HandleAuraRetainComboPoints,                     //148 SPELL_AURA_RETAIN_COMBO_POINTS
    &Aura::HandleNoImmediateEffect,                         //149 SPELL_AURA_REDUCE_PUSHBACK            implemented in Spell::Delayed and Spell::DelayedChannel
    &Aura::HandleShieldBlockValue,                          //150 SPELL_AURA_MOD_SHIELD_BLOCKVALUE_PCT
    &Aura::HandleAuraTrackStealthed,                        //151 SPELL_AURA_TRACK_STEALTHED
    &Aura::HandleNoImmediateEffect,                         //152 SPELL_AURA_MOD_DETECTED_RANGE         implemented in Creature::GetAttackDistance
    &Aura::HandleNoImmediateEffect,                         //153 SPELL_AURA_SPLIT_DAMAGE_FLAT          implemented in Unit::CalculateAbsorbAndResist
    &Aura::HandleNoImmediateEffect,                         //154 SPELL_AURA_MOD_STEALTH_LEVEL          implemented in Unit::IsVisibleForOrDetect
    &Aura::HandleNoImmediateEffect,                         //155 SPELL_AURA_MOD_WATER_BREATHING        implemented in Player::getMaxTimer
    &Aura::HandleNoImmediateEffect,                         //156 SPELL_AURA_MOD_REPUTATION_GAIN        implemented in Player::CalculateReputationGain
    &Aura::HandleUnused,                                    //157 SPELL_AURA_PET_DAMAGE_MULTI (single test like spell 20782, also single for 214 aura)
    &Aura::HandleShieldBlockValue,                          //158 SPELL_AURA_MOD_SHIELD_BLOCKVALUE
    &Aura::HandleNoImmediateEffect,                         //159 SPELL_AURA_NO_PVP_CREDIT              implemented in Player::RewardHonor
    &Aura::HandleNoImmediateEffect,                         //160 SPELL_AURA_MOD_AOE_AVOIDANCE          implemented in Unit::MagicSpellHitResult
    &Aura::HandleNoImmediateEffect,                         //161 SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT implemented in Player::RegenerateAll and Player::RegenerateHealth
    &Aura::HandleAuraPowerBurn,                             //162 SPELL_AURA_POWER_BURN_MANA
    &Aura::HandleNoImmediateEffect,                         //163 SPELL_AURA_MOD_CRIT_DAMAGE_BONUS      implemented in Unit::CalculateMeleeDamage and Unit::SpellCriticalDamageBonus
    &Aura::HandleUnused,                                    //164 unused (3.0.8a-3.2.2a), only one test spell 10654
    &Aura::HandleNoImmediateEffect,                         //165 SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS implemented in Unit::MeleeDamageBonusDone
    &Aura::HandleAuraModAttackPowerPercent,                 //166 SPELL_AURA_MOD_ATTACK_POWER_PCT
    &Aura::HandleAuraModRangedAttackPowerPercent,           //167 SPELL_AURA_MOD_RANGED_ATTACK_POWER_PCT
    &Aura::HandleNoImmediateEffect,                         //168 SPELL_AURA_MOD_DAMAGE_DONE_VERSUS            implemented in Unit::SpellDamageBonusDone, Unit::MeleeDamageBonusDone
    &Aura::HandleNoImmediateEffect,                         //169 SPELL_AURA_MOD_CRIT_PERCENT_VERSUS           implemented in Unit::DealDamageBySchool, Unit::DoAttackDamage, Unit::SpellCriticalBonus
    &Aura::HandleDetectAmore,                               //170 SPELL_AURA_DETECT_AMORE       different spells that ignore transformation effects
    &Aura::HandleAuraModIncreaseSpeed,                      //171 SPELL_AURA_MOD_SPEED_NOT_STACK
    &Aura::HandleAuraModIncreaseMountedSpeed,               //172 SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK
    &Aura::HandleUnused,                                    //173 unused (3.0.8a-3.2.2a) no spells, old SPELL_AURA_ALLOW_CHAMPION_SPELLS  only for Proclaim Champion spell
    &Aura::HandleModSpellDamagePercentFromStat,             //174 SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT  implemented in Unit::SpellBaseDamageBonusDone
    &Aura::HandleModSpellHealingPercentFromStat,            //175 SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT implemented in Unit::SpellBaseHealingBonusDone
    &Aura::HandleSpiritOfRedemption,                        //176 SPELL_AURA_SPIRIT_OF_REDEMPTION   only for Spirit of Redemption spell, die at aura end
    &Aura::HandleAoECharm,                                  //177 SPELL_AURA_AOE_CHARM
    &Aura::HandleNoImmediateEffect,                         //178 SPELL_AURA_MOD_DEBUFF_RESISTANCE          implemented in Unit::MagicSpellHitResult
    &Aura::HandleNoImmediateEffect,                         //179 SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE implemented in Unit::SpellCriticalBonus
    &Aura::HandleNoImmediateEffect,                         //180 SPELL_AURA_MOD_FLAT_SPELL_DAMAGE_VERSUS   implemented in Unit::SpellDamageBonusDone
    &Aura::HandleUnused,                                    //181 unused (3.0.8a-3.2.2a) old SPELL_AURA_MOD_FLAT_SPELL_CRIT_DAMAGE_VERSUS
    &Aura::HandleAuraModResistenceOfStatPercent,            //182 SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT
    &Aura::HandleNoImmediateEffect,                         //183 SPELL_AURA_MOD_CRITICAL_THREAT only used in 28746, implemented in ThreatCalcHelper::CalcThreat
    &Aura::HandleNoImmediateEffect,                         //184 SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE  implemented in Unit::CalculateEffectiveMissChance
    &Aura::HandleNoImmediateEffect,                         //185 SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE implemented in Unit::CalculateEffectiveMissChance
    &Aura::HandleNoImmediateEffect,                         //186 SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE  implemented in Unit::MagicSpellHitResult
    &Aura::HandleNoImmediateEffect,                         //187 SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE  implemented in Unit::CalculateEffectiveCritChance
    &Aura::HandleNoImmediateEffect,                         //188 SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE implemented in Unit::CalculateEffectiveCritChance
    &Aura::HandleModRating,                                 //189 SPELL_AURA_MOD_RATING
    &Aura::HandleNoImmediateEffect,                         //190 SPELL_AURA_MOD_FACTION_REPUTATION_GAIN     implemented in Player::CalculateReputationGain
    &Aura::HandleAuraModUseNormalSpeed,                     //191 SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED
    &Aura::HandleModMeleeRangedSpeedPct,                    //192 SPELL_AURA_MOD_MELEE_RANGED_HASTE
    &Aura::HandleModCombatSpeedPct,                         //193 SPELL_AURA_HASTE_ALL (in fact combat (any type attack) speed pct)
    &Aura::HandleNoImmediateEffect,                         //194 SPELL_AURA_MOD_IGNORE_ABSORB_SCHOOL       implement in Unit::CalcNotIgnoreAbsorbDamage
    &Aura::HandleNoImmediateEffect,                         //195 SPELL_AURA_MOD_IGNORE_ABSORB_FOR_SPELL    implement in Unit::CalcNotIgnoreAbsorbDamage
    &Aura::HandleNULL,                                      //196 SPELL_AURA_MOD_COOLDOWN (single spell 24818 in 3.2.2a)
    &Aura::HandleNoImmediateEffect,                         //197 SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE implemented in Unit::SpellCriticalBonus Unit::GetUnitCriticalChance
    &Aura::HandleUnused,                                    //198 unused (3.0.8a-3.2.2a) old SPELL_AURA_MOD_ALL_WEAPON_SKILLS
    &Aura::HandleNoImmediateEffect,                         //199 SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT  implemented in Unit::MagicSpellHitResult
    &Aura::HandleNoImmediateEffect,                         //200 SPELL_AURA_MOD_KILL_XP_PCT                 implemented in Player::GiveXP
    &Aura::HandleAuraAllowFlight,                           //201 SPELL_AURA_FLY                             this aura enable flight mode...
    &Aura::HandleNoImmediateEffect,                         //202 SPELL_AURA_IGNORE_COMBAT_RESULT            implemented in Unit::MeleeSpellHitResult
    &Aura::HandleNoImmediateEffect,                         //203 SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_DAMAGE  implemented in Unit::CalculateMeleeDamage and Unit::SpellCriticalDamageBonus
    &Aura::HandleNoImmediateEffect,                         //204 SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_DAMAGE implemented in Unit::CalculateMeleeDamage and Unit::SpellCriticalDamageBonus
    &Aura::HandleNoImmediateEffect,                         //205 SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_DAMAGE  implemented in Unit::SpellCriticalDamageBonus
    &Aura::HandleAuraModIncreaseFlightSpeed,                //206 SPELL_AURA_MOD_FLIGHT_SPEED
    &Aura::HandleAuraModIncreaseFlightSpeed,                //207 SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED
    &Aura::HandleAuraModIncreaseFlightSpeed,                //208 SPELL_AURA_MOD_FLIGHT_SPEED_STACKING
    &Aura::HandleAuraModIncreaseFlightSpeed,                //209 SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_STACKING
    &Aura::HandleAuraModIncreaseFlightSpeed,                //210 SPELL_AURA_MOD_FLIGHT_SPEED_NOT_STACKING
    &Aura::HandleAuraModIncreaseFlightSpeed,                //211 SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_NOT_STACKING
    &Aura::HandleAuraModRangedAttackPowerOfStatPercent,     //212 SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT
    &Aura::HandleNoImmediateEffect,                         //213 SPELL_AURA_MOD_RAGE_FROM_DAMAGE_DEALT implemented in Player::RewardRage
    &Aura::HandleUnused,                                    //214 Tamed Pet Passive (single test like spell 20782, also single for 157 aura)
    &Aura::HandleArenaPreparation,                          //215 SPELL_AURA_ARENA_PREPARATION
    &Aura::HandleModCastingSpeed,                           //216 SPELL_AURA_HASTE_SPELLS
    &Aura::HandleUnused,                                    //217 unused (3.0.8a-3.2.2a)
    &Aura::HandleAuraModRangedHaste,                        //218 SPELL_AURA_HASTE_RANGED
    &Aura::HandleModManaRegen,                              //219 SPELL_AURA_MOD_MANA_REGEN_FROM_STAT
    &Aura::HandleModRatingFromStat,                         //220 SPELL_AURA_MOD_RATING_FROM_STAT
    &Aura::HandleAuraDetaunt,                               //221 SPELL_AURA_DETAUNT
    &Aura::HandleUnused,                                    //222 unused (3.0.8a-3.2.2a) only for spell 44586 that not used in real spell cast
    &Aura::HandleNULL,                                      //223 dummy code (cast damage spell to attacker) and another dymmy (jump to another nearby raid member)
    &Aura::HandleUnused,                                    //224 unused (3.0.8a-3.2.2a)
    &Aura::HandlePrayerOfMending,                           //225 SPELL_AURA_PRAYER_OF_MENDING
    &Aura::HandleAuraPeriodicDummy,                         //226 SPELL_AURA_PERIODIC_DUMMY
    &Aura::HandlePeriodicTriggerSpellWithValue,             //227 SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE
    &Aura::HandleNoImmediateEffect,                         //228 SPELL_AURA_DETECT_STEALTH
    &Aura::HandleNoImmediateEffect,                         //229 SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE        implemented in Unit::SpellDamageBonusTaken
    &Aura::HandleAuraModIncreaseMaxHealth,                  //230 Commanding Shout
    &Aura::HandleNoImmediateEffect,                         //231 SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE
    &Aura::HandleNoImmediateEffect,                         //232 SPELL_AURA_MECHANIC_DURATION_MOD           implement in Unit::CalculateAuraDuration
    &Aura::HandleNULL,                                      //233 set model id to the one of the creature with id m_modifier.m_miscvalue
    &Aura::HandleNoImmediateEffect,                         //234 SPELL_AURA_MECHANIC_DURATION_MOD_NOT_STACK implement in Unit::CalculateAuraDuration
    &Aura::HandleAuraModDispelResist,                       //235 SPELL_AURA_MOD_DISPEL_RESIST               implement in Unit::MagicSpellHitResult
    &Aura::HandleAuraControlVehicle,                        //236 SPELL_AURA_CONTROL_VEHICLE
    &Aura::HandleModSpellDamagePercentFromAttackPower,      //237 SPELL_AURA_MOD_SPELL_DAMAGE_OF_ATTACK_POWER  implemented in Unit::SpellBaseDamageBonusDone
    &Aura::HandleModSpellHealingPercentFromAttackPower,     //238 SPELL_AURA_MOD_SPELL_HEALING_OF_ATTACK_POWER implemented in Unit::SpellBaseHealingBonusDone
    &Aura::HandleAuraModScale,                              //239 SPELL_AURA_MOD_SCALE_2 only in Noggenfogger Elixir (16595) before 2.3.0 aura 61
    &Aura::HandleAuraModExpertise,                          //240 SPELL_AURA_MOD_EXPERTISE
    &Aura::HandleForceMoveForward,                          //241 Forces the caster to move forward
    &Aura::HandleUnused,                                    //242 SPELL_AURA_MOD_SPELL_DAMAGE_FROM_HEALING (only 2 test spels in 3.2.2a)
    &Aura::HandleFactionOverride,                           //243 SPELL_AURA_FACTION_OVERRIDE
    &Aura::HandleComprehendLanguage,                        //244 SPELL_AURA_COMPREHEND_LANGUAGE
    &Aura::HandleNoImmediateEffect,                         //245 SPELL_AURA_MOD_DURATION_OF_MAGIC_EFFECTS     implemented in Unit::CalculateAuraDuration
    &Aura::HandleNoImmediateEffect,                         //246 SPELL_AURA_MOD_DURATION_OF_EFFECTS_BY_DISPEL implemented in Unit::CalculateAuraDuration
    &Aura::HandleAuraMirrorImage,                           //247 SPELL_AURA_MIRROR_IMAGE                      target to become a clone of the caster
    &Aura::HandleNoImmediateEffect,                         //248 SPELL_AURA_MOD_COMBAT_RESULT_CHANCE         implemented in Unit::CalculateEffectiveDodgeChance, Unit::CalculateEffectiveParryChance, Unit::CalculateEffectiveBlockChance
    &Aura::HandleAuraConvertRune,                           //249 SPELL_AURA_CONVERT_RUNE
    &Aura::HandleAuraModIncreaseHealth,                     //250 SPELL_AURA_MOD_INCREASE_HEALTH_2
    &Aura::HandleNULL,                                      //251 SPELL_AURA_MOD_ENEMY_DODGE
    &Aura::HandleModCombatSpeedPct,                         //252 SPELL_AURA_SLOW_ALL
    &Aura::HandleNoImmediateEffect,                         //253 SPELL_AURA_MOD_BLOCK_CRIT_CHANCE             implemented in Unit::CalculateMeleeDamage
    &Aura::HandleAuraModDisarm,                             //254 SPELL_AURA_MOD_DISARM_OFFHAND     also disarm shield
    &Aura::HandleNoImmediateEffect,                         //255 SPELL_AURA_MOD_MECHANIC_DAMAGE_TAKEN_PERCENT    implemented in Unit::SpellDamageBonusTaken
    &Aura::HandleNoReagentUseAura,                          //256 SPELL_AURA_NO_REAGENT_USE Use SpellClassMask for spell select
    &Aura::HandleNULL,                                      //257 SPELL_AURA_MOD_TARGET_RESIST_BY_SPELL_CLASS Use SpellClassMask for spell select
    &Aura::HandleNULL,                                      //258 SPELL_AURA_MOD_SPELL_VISUAL
    &Aura::HandleNoImmediateEffect,                         //259 SPELL_AURA_MOD_PERIODIC_HEAL                    implemented in Unit::SpellHealingBonus
    &Aura::HandleNoImmediateEffect,                         //260 SPELL_AURA_SCREEN_EFFECT (miscvalue = id in ScreenEffect.dbc) not required any code
    &Aura::HandlePhase,                                     //261 SPELL_AURA_PHASE undetectable invisibility?     implemented in Unit::IsVisibleForOrDetect
    &Aura::HandleNoImmediateEffect,                         //262 SPELL_AURA_IGNORE_UNIT_STATE                    implemented in Unit::isIgnoreUnitState & Spell::CheckCast
    &Aura::HandleNoImmediateEffect,                         //263 SPELL_AURA_ALLOW_ONLY_ABILITY                   implemented in Spell::CheckCasterAuras, lool enum IgnoreUnitState for known misc values
    &Aura::HandleUnused,                                    //264 unused (3.0.8a-3.2.2a)
    &Aura::HandleUnused,                                    //265 unused (3.0.8a-3.2.2a)
    &Aura::HandleUnused,                                    //266 unused (3.0.8a-3.2.2a)
    &Aura::HandleNoImmediateEffect,                         //267 SPELL_AURA_MOD_IMMUNE_AURA_APPLY_SCHOOL         implemented in Unit::IsImmuneToSpellEffect
    &Aura::HandleAuraModAttackPowerOfStatPercent,           //268 SPELL_AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT
    &Aura::HandleNoImmediateEffect,                         //269 SPELL_AURA_MOD_IGNORE_DAMAGE_REDUCTION_SCHOOL   implemented in Unit::CalcNotIgnoreDamageReduction
    &Aura::HandleUnused,                                    //270 SPELL_AURA_MOD_IGNORE_TARGET_RESIST (unused in 3.2.2a)
    &Aura::HandleNoImmediateEffect,                         //271 SPELL_AURA_MOD_DAMAGE_FROM_CASTER    implemented in Unit::SpellDamageBonusTaken
    &Aura::HandleNoImmediateEffect,                         //272 SPELL_AURA_MAELSTROM_WEAPON (unclear use for aura, it used in (3.2.2a...3.3.0) in single spell 53817 that spellmode stacked and charged spell expected to be drop as stack
    &Aura::HandleNoImmediateEffect,                         //273 SPELL_AURA_X_RAY (client side implementation)
    &Aura::HandleNULL,                                      //274 proc free shot?
    &Aura::HandleNoImmediateEffect,                         //275 SPELL_AURA_MOD_IGNORE_SHAPESHIFT Use SpellClassMask for spell select
    &Aura::HandleNULL,                                      //276 mod damage % mechanic?
    &Aura::HandleNoImmediateEffect,                         //277 SPELL_AURA_MOD_MAX_AFFECTED_TARGETS Use SpellClassMask for spell select
    &Aura::HandleAuraModDisarm,                             //278 SPELL_AURA_MOD_DISARM_RANGED disarm ranged weapon
    &Aura::HandleMirrorName,                                //279 SPELL_AURA_MIRROR_NAME                target receives the casters name
    &Aura::HandleModTargetArmorPct,                         //280 SPELL_AURA_MOD_TARGET_ARMOR_PCT
    &Aura::HandleNoImmediateEffect,                         //281 SPELL_AURA_MOD_HONOR_GAIN             implemented in Player::RewardHonor
    &Aura::HandleAuraIncreaseBaseHealthPercent,             //282 SPELL_AURA_INCREASE_BASE_HEALTH_PERCENT
    &Aura::HandleNoImmediateEffect,                         //283 SPELL_AURA_MOD_HEALING_RECEIVED       implemented in Unit::SpellHealingBonusTaken
    &Aura::HandleTriggerLinkedAura,                         //284 SPELL_AURA_TRIGGER_LINKED_AURA
    &Aura::HandleAuraModAttackPowerOfArmor,                 //285 SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR  implemented in Player::UpdateAttackPowerAndDamage
    &Aura::HandleNoImmediateEffect,                         //286 SPELL_AURA_ABILITY_PERIODIC_CRIT      implemented in Aura::IsCritFromAbilityAura called from Aura::PeriodicTick
    &Aura::HandleNoImmediateEffect,                         //287 SPELL_AURA_DEFLECT_SPELLS             implemented in Unit::MagicSpellHitResult and Unit::MeleeSpellHitResult
    &Aura::HandleNoImmediateEffect,                         //288 SPELL_AURA_MOD_PARRY_FROM_BEHIND_PERCENT percent from normal parry/deflect applied to from behind attack case (single spell used 67801, also look 4.1.0 spell 97574)
    &Aura::HandleUnused,                                    //289 unused (3.2.2a)
    &Aura::HandleAuraModAllCritChance,                      //290 SPELL_AURA_MOD_ALL_CRIT_CHANCE
    &Aura::HandleNoImmediateEffect,                         //291 SPELL_AURA_MOD_QUEST_XP_PCT           implemented in Player::GiveXP
    &Aura::HandleAuraOpenStable,                            //292 call stabled pet
    &Aura::HandleAuraAddMechanicAbilities,                  //293 SPELL_AURA_ADD_MECHANIC_ABILITIES  replaces target's action bars with a predefined spellset
    &Aura::HandleAuraStopNaturalManaRegen,                  //294 SPELL_AURA_STOP_NATURAL_MANA_REGEN implemented in Player:Regenerate
    &Aura::HandleUnused,                                    //295 unused (3.2.2a)
    &Aura::HandleAuraSetVehicleId,                          //296 6 spells
    &Aura::HandleNULL,                                      //297 1 spell (counter spell school?)
    &Aura::HandleUnused,                                    //298 unused (3.2.2a)
    &Aura::HandleUnused,                                    //299 unused (3.2.2a)
    &Aura::HandleNoImmediateEffect,                         //300 SPELL_AURA_SHARE_DAMAGE_PCT 9 spells
    &Aura::HandleNULL,                                      //301 SPELL_AURA_HEAL_ABSORB 5 spells
    &Aura::HandleUnused,                                    //302 unused (3.2.2a)
    &Aura::HandleNoImmediateEffect,                         //303 SPELL_AURA_DAMAGE_DONE_VERSUS_AURA_STATE_PCT - 17 spells implemented in Unit::*DamageBonus
    &Aura::HandleAuraFakeInebriation,                       //304 SPELL_AURA_FAKE_INEBRIATE
    &Aura::HandleAuraModIncreaseSpeed,                      //305 SPELL_AURA_MOD_MINIMUM_SPEED
    &Aura::HandleNULL,                                      //306 1 spell
    &Aura::HandleNULL,                                      //307 absorb healing?
    &Aura::HandleNULL,                                      //308 new aura for hunter traps
    &Aura::HandleNULL,                                      //309 absorb healing?
    &Aura::HandleNoImmediateEffect,                         //310 SPELL_AURA_MOD_PET_AOE_DAMAGE_AVOIDANCE implemented in Unit::SpellDamageBonusTaken
    &Aura::HandleNULL,                                      //311 0 spells in 3.3
    &Aura::HandleNULL,                                      //312 0 spells in 3.3
    &Aura::HandleNULL,                                      //313 0 spells in 3.3
    &Aura::HandlePreventResurrection,                       //314 SPELL_AURA_PREVENT_RESURRECTION
    &Aura::HandleNULL,                                      //315 underwater walking
    &Aura::HandleNULL                                       //316 makes haste affect HOT/DOT ticks
};

static AuraType const frozenAuraTypes[] = { SPELL_AURA_MOD_ROOT, SPELL_AURA_MOD_STUN, SPELL_AURA_NONE };

Aura::Aura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster, Item* castItem) :
    m_periodicTimer(0), m_periodicTick(0), m_removeMode(AURA_REMOVE_BY_DEFAULT),
    m_effIndex(eff), m_positive(false), m_isPeriodic(false), m_isAreaAura(false),
    m_isPersistent(false), m_magnetUsed(false), m_spellAuraHolder(holder)
{
    MANGOS_ASSERT(target);
    MANGOS_ASSERT(spellproto && spellproto == sSpellTemplate.LookupEntry<SpellEntry>(spellproto->Id) && "`info` must be pointer to sSpellTemplate element");

    m_currentBasePoints = currentBasePoints ? *currentBasePoints : spellproto->CalculateSimpleValue(eff);

    m_positive = IsPositiveAuraEffect(spellproto, m_effIndex, caster, target);
    m_applyTime = time(nullptr);

    int32 damage;
    if (!caster)
        damage = m_currentBasePoints;
    else
    {
        damage = caster->CalculateSpellDamage(target, spellproto, m_effIndex, &m_currentBasePoints);

        if (!damage && castItem && castItem->GetItemSuffixFactor())
        {
            ItemRandomSuffixEntry const* item_rand_suffix = sItemRandomSuffixStore.LookupEntry(abs(castItem->GetItemRandomPropertyId()));
            if (item_rand_suffix)
            {
                for (int k = 0; k < 3; ++k)
                {
                    SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(item_rand_suffix->enchant_id[k]);
                    if (pEnchant)
                    {
                        for (unsigned int t : pEnchant->spellid)
                        {
                            if (t != spellproto->Id)
                                continue;

                            damage = uint32((item_rand_suffix->prefix[k] * castItem->GetItemSuffixFactor()) / 10000);
                            break;
                        }
                    }

                    if (damage)
                        break;
                }
            }
        }

        // scripting location for custom aura damage
        switch (spellproto->Id)
        {
            case 34501: // Expose Weakness
                damage = (caster->GetStat(STAT_AGILITY) * damage) / 100;
                break;
        }
    }

    damage *= holder->GetStackAmount();

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Aura: construct Spellid : %u, Aura : %u Target : %d Damage : %d", spellproto->Id, spellproto->EffectApplyAuraName[eff], spellproto->EffectImplicitTargetA[eff], damage);

    SetModifier(AuraType(spellproto->EffectApplyAuraName[eff]), damage, spellproto->EffectAmplitude[eff], spellproto->EffectMiscValue[eff]);

    Player* modOwner = caster ? caster->GetSpellModOwner() : nullptr;

    // Apply periodic time mod
    if (modOwner && m_modifier.periodictime)
        modOwner->ApplySpellMod(spellproto->Id, SPELLMOD_ACTIVATION_TIME, m_modifier.periodictime);

    if (caster && spellproto->HasAttribute(SPELL_ATTR_EX5_HASTE_AFFECT_DURATION))
        m_modifier.periodictime = int32(m_modifier.periodictime * caster->GetFloatValue(UNIT_MOD_CAST_SPEED));

    // Start periodic on next tick or at aura apply
    if (!spellproto->HasAttribute(SPELL_ATTR_EX5_START_PERIODIC_AT_APPLY))
        m_periodicTimer = m_modifier.periodictime;
}

Aura::~Aura()
{
}

AreaAura::AreaAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target,
                   Unit* caster, Item* castItem, uint32 originalRankSpellId)
    : Aura(spellproto, eff, currentBasePoints, holder, target, caster, castItem), m_originalRankSpellId(originalRankSpellId)
{
    m_isAreaAura = true;

    // caster==nullptr in constructor args if target==caster in fact
    Unit* caster_ptr = caster ? caster : target;

    m_radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(spellproto->EffectRadiusIndex[m_effIndex]));
    if (Player* modOwner = caster_ptr->GetSpellModOwner())
        modOwner->ApplySpellMod(spellproto->Id, SPELLMOD_RADIUS, m_radius);

    switch (spellproto->Effect[eff])
    {
        case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
            m_areaAuraType = AREA_AURA_PARTY;
            break;
        case SPELL_EFFECT_APPLY_AREA_AURA_RAID:
            m_areaAuraType = AREA_AURA_RAID;
            // Light's Beacon not applied to caster itself (TODO: more generic check for another similar spell if any?)
            if (target == caster_ptr && spellproto->Id == 53651)
                m_modifier.m_auraname = SPELL_AURA_NONE;
            break;
        case SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
            m_areaAuraType = AREA_AURA_FRIEND;
            break;
        case SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
            m_areaAuraType = AREA_AURA_ENEMY;
            if (target == caster_ptr)
                m_modifier.m_auraname = SPELL_AURA_NONE;    // Do not do any effect on self
            break;
        case SPELL_EFFECT_APPLY_AREA_AURA_PET:
            m_areaAuraType = AREA_AURA_PET;
            break;
        case SPELL_EFFECT_APPLY_AREA_AURA_OWNER:
            m_areaAuraType = AREA_AURA_OWNER;
            if (target == caster_ptr)
                m_modifier.m_auraname = SPELL_AURA_NONE;
            break;
        default:
            sLog.outError("Wrong spell effect in AreaAura constructor");
            MANGOS_ASSERT(false);
            break;
    }

    // totems are immune to any kind of area auras
    if (target->GetTypeId() == TYPEID_UNIT && ((Creature*)target)->IsTotem())
        m_modifier.m_auraname = SPELL_AURA_NONE;
}

AreaAura::~AreaAura()
{
}

PersistentAreaAura::PersistentAreaAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target,
                                       Unit* caster, Item* castItem) : Aura(spellproto, eff, currentBasePoints, holder, target, caster, castItem)
{
    m_isPersistent = true;
}

PersistentAreaAura::~PersistentAreaAura()
{
}

SingleEnemyTargetAura::SingleEnemyTargetAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target,
        Unit* caster, Item* castItem) : Aura(spellproto, eff, currentBasePoints, holder, target, caster, castItem)
{
    if (caster)
        m_castersTargetGuid = caster->GetSelectionGuid();
}

SingleEnemyTargetAura::~SingleEnemyTargetAura()
{
}

Unit* SingleEnemyTargetAura::GetTriggerTarget() const
{
    return ObjectAccessor::GetUnit(*(m_spellAuraHolder->GetTarget()), m_castersTargetGuid);
}

Aura* CreateAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster, Item* castItem)
{
    if (IsAreaAuraEffect(spellproto->Effect[eff]))
        return new AreaAura(spellproto, eff, currentBasePoints, holder, target, caster, castItem);

    uint32 triggeredSpellId = spellproto->EffectTriggerSpell[eff];

    if (SpellEntry const* triggeredSpellInfo = sSpellTemplate.LookupEntry<SpellEntry>(triggeredSpellId))
        for (unsigned int i : triggeredSpellInfo->EffectImplicitTargetA)
            if (i == TARGET_UNIT_CHANNEL_TARGET)
                return new SingleEnemyTargetAura(spellproto, eff, currentBasePoints, holder, target, caster, castItem);

    return new Aura(spellproto, eff, currentBasePoints, holder, target, caster, castItem);
}

SpellAuraHolder* CreateSpellAuraHolder(SpellEntry const* spellproto, Unit* target, WorldObject* caster, Item* castItem /*= nullptr*/, SpellEntry const* triggeredBy /*= nullptr*/)
{
    return new SpellAuraHolder(spellproto, target, caster, castItem, triggeredBy);
}

void Aura::SetModifier(AuraType t, int32 a, uint32 pt, int32 miscValue)
{
    m_modifier.m_auraname = t;
    m_modifier.m_amount = a;
    m_modifier.m_baseAmount = a;
    m_modifier.m_miscvalue = miscValue;
    m_modifier.periodictime = pt;
}

void Aura::Update(uint32 diff)
{
    if (m_isPeriodic)
    {
        m_periodicTimer -= diff;
        if (m_periodicTimer <= 0) // tick also at m_periodicTimer==0 to prevent lost last tick in case max m_duration == (max m_periodicTimer)*N
        {
            // update before applying (aura can be removed in TriggerSpell or PeriodicTick calls)
            m_periodicTimer += m_modifier.periodictime;
            ++m_periodicTick;                               // for some infinity auras in some cases can overflow and reset
            PeriodicTick();
        }
    }
}

void AreaAura::Update(uint32 diff)
{
    // update for the caster of the aura
    if (GetCasterGuid() == GetTarget()->GetObjectGuid())
    {
        Unit* caster = GetTarget();

        if (!caster->hasUnitState(UNIT_STAT_ISOLATED))
        {
            Unit* owner = caster->GetMaster();
            if (!owner)
                owner = caster;
            UnitList targets;

            switch (m_areaAuraType)
            {
                case AREA_AURA_PARTY:
                {
                    Group* pGroup = nullptr;

                    // Handle aura party for players
                    if (owner->GetTypeId() == TYPEID_PLAYER)
                    {
                        pGroup = ((Player*)owner)->GetGroup();

                        if (pGroup)
                        {
                            uint8 subgroup = ((Player*)owner)->GetSubGroup();
                            for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
                            {
                                Player* Target = itr->getSource();
                                if (Target && Target->isAlive() && Target->GetSubGroup() == subgroup && caster->CanAssist(Target))
                                {
                                    if (caster->IsWithinDistInMap(Target, m_radius))
                                        targets.push_back(Target);
                                    Pet* pet = Target->GetPet();
                                    if (pet && pet->isAlive() && caster->IsWithinDistInMap(pet, m_radius))
                                        targets.push_back(pet);
                                }
                            }
                            break;
                        }
                    }
                    else    // handle aura party for creatures
                    {
                        // Get all creatures in spell radius
                        std::list<Creature*> nearbyTargets;
                        MaNGOS::AnyUnitInObjectRangeCheck u_check(owner, m_radius);
                        MaNGOS::CreatureListSearcher<MaNGOS::AnyUnitInObjectRangeCheck> searcher(nearbyTargets, u_check);
                        Cell::VisitGridObjects(owner, searcher, m_radius);

                        for (auto target : nearbyTargets)
                        {
                            // Due to the lack of support for NPC groups or formations, are considered of the same party NPCs with same faction than caster
                            if (target != owner && target->isAlive() && target->getFaction() == ((Creature*)owner)->getFaction())
                                targets.push_back(target);
                        }
                    }

                    // add owner
                    if (owner != caster && caster->IsWithinDistInMap(owner, m_radius))
                        targets.push_back(owner);
                    // add caster's pet
                    Unit* pet = caster->GetPet();
                    if (pet && caster->IsWithinDistInMap(pet, m_radius))
                        targets.push_back(pet);

                    break;
                }
                case AREA_AURA_RAID:
                {
                    Group* pGroup = nullptr;

                    if (owner->GetTypeId() == TYPEID_PLAYER)
                        pGroup = ((Player*)owner)->GetGroup();

                    if (pGroup)
                    {
                        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
                        {
                            Player* Target = itr->getSource();
                            if (Target && Target->isAlive() && caster->CanAssist(Target))
                            {
                                if (caster->IsWithinDistInMap(Target, m_radius))
                                    targets.push_back(Target);
                                Pet* pet = Target->GetPet();
                                if (pet && pet->isAlive() && caster->IsWithinDistInMap(pet, m_radius))
                                    targets.push_back(pet);
                            }
                        }
                    }
                    else
                    {
                        // add owner
                        if (owner != caster && caster->IsWithinDistInMap(owner, m_radius))
                            targets.push_back(owner);
                        // add caster's pet
                        Unit* pet = caster->GetPet();
                        if (pet && caster->IsWithinDistInMap(pet, m_radius))
                            targets.push_back(pet);
                    }
                    break;
                }
                case AREA_AURA_FRIEND:
                {
                    MaNGOS::AnyFriendlyUnitInObjectRangeCheck u_check(caster, nullptr, m_radius);
                    MaNGOS::UnitListSearcher<MaNGOS::AnyFriendlyUnitInObjectRangeCheck> searcher(targets, u_check);
                    Cell::VisitAllObjects(caster, searcher, m_radius);
                    break;
                }
                case AREA_AURA_ENEMY:
                {
                    MaNGOS::AnyAoETargetUnitInObjectRangeCheck u_check(caster, nullptr, m_radius); // No GetCharmer in searcher
                    MaNGOS::UnitListSearcher<MaNGOS::AnyAoETargetUnitInObjectRangeCheck> searcher(targets, u_check);
                    Cell::VisitAllObjects(caster, searcher, m_radius);
                    break;
                }
                case AREA_AURA_OWNER:
                case AREA_AURA_PET:
                {
                    if (owner != caster && caster->IsWithinDistInMap(owner, m_radius))
                        targets.push_back(owner);
                    break;
                }
            }

            for (auto& target : targets)
            {
                // flag for selection is need apply aura to current iteration target
                bool apply = true;

                SpellEntry const* actualSpellInfo;
                if (GetCasterGuid() == target->GetObjectGuid()) // if caster is same as target then no need to change rank of the spell
                    actualSpellInfo = GetSpellProto();
                else
                    actualSpellInfo = sSpellMgr.SelectAuraRankForLevel(GetSpellProto(), target->getLevel()); // use spell id according level of the target
                if (!actualSpellInfo)
                    continue;

                Unit::SpellAuraHolderBounds spair = target->GetSpellAuraHolderBounds(actualSpellInfo->Id);
                // we need ignore present caster self applied are auras sometime
                // in cases if this only auras applied for spell effect
                for (Unit::SpellAuraHolderMap::const_iterator i = spair.first; i != spair.second; ++i)
                {
                    if (i->second->IsDeleted())
                        continue;

                    Aura* aur = i->second->GetAuraByEffectIndex(m_effIndex);

                    if (!aur)
                        continue;

                    switch (m_areaAuraType)
                    {
                        case AREA_AURA_ENEMY:
                            // non caster self-casted auras (non stacked)
                            if (aur->GetModifier()->m_auraname != SPELL_AURA_NONE)
                                apply = false;
                            break;
                        case AREA_AURA_RAID:
                            // non caster self-casted auras (stacked from diff. casters)
                            if (aur->GetModifier()->m_auraname != SPELL_AURA_NONE || i->second->GetCasterGuid() == GetCasterGuid())
                                apply = false;
                            break;
                        default:
                            // in generic case not allow stacking area auras
                            apply = false;
                            break;
                    }

                    if (!apply)
                        break;
                }

                if (!apply)
                    continue;

                // Skip some targets (TODO: Might require better checks, also unclear how the actual caster must/can be handled)
                if (actualSpellInfo->HasAttribute(SPELL_ATTR_EX3_TARGET_ONLY_PLAYER) && target->GetTypeId() != TYPEID_PLAYER)
                    continue;

                int32 actualBasePoints = m_currentBasePoints;
                // recalculate basepoints for lower rank (all AreaAura spell not use custom basepoints?)
                if (actualSpellInfo != GetSpellProto())
                    actualBasePoints = actualSpellInfo->CalculateSimpleValue(m_effIndex);

                SpellAuraHolder* holder = target->GetSpellAuraHolder(actualSpellInfo->Id, GetCasterGuid());

                bool addedToExisting = true;
                if (!holder)
                {
                    holder = CreateSpellAuraHolder(actualSpellInfo, target, caster);
                    addedToExisting = false;
                }

                holder->SetAuraDuration(GetAuraDuration());

                AreaAura* aur = new AreaAura(actualSpellInfo, m_effIndex, &actualBasePoints, holder, target, caster, nullptr, GetSpellProto()->Id);
                holder->AddAura(aur, m_effIndex);

                if (addedToExisting)
                {
                    target->AddAuraToModList(aur);
                    aur->ApplyModifier(true, true);
                }
                else
                {
                    if (target->AddSpellAuraHolder(holder))
                        holder->SetState(SPELLAURAHOLDER_STATE_READY);
                    else
                        delete holder;
                }
            }
        }
        Aura::Update(diff);
    }
    else                                                    // aura at non-caster
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        uint32 originalRankSpellId = m_originalRankSpellId ? m_originalRankSpellId : GetId(); // caster may have different spell id if target has lower level

        Aura::Update(diff);

        // remove aura if out-of-range from caster (after teleport for example)
        // or caster is isolated or caster no longer has the aura
        // or caster is (no longer) friendly
        bool needFriendly = (m_areaAuraType != AREA_AURA_ENEMY);
        if (!caster ||
                caster->hasUnitState(UNIT_STAT_ISOLATED)               ||
                !caster->HasAura(originalRankSpellId, GetEffIndex())   ||
                !caster->IsWithinDistInMap(target, m_radius)           ||
                caster->CanAssist(target) != needFriendly
           )
        {
            target->RemoveSingleAuraFromSpellAuraHolder(GetId(), GetEffIndex(), GetCasterGuid());
        }
        else if (m_areaAuraType == AREA_AURA_PARTY)         // check if in same sub group
        {
            // Do not check group if target == owner or target == pet
            // or if caster is a not player (as NPCs do not support group so aura is only removed by moving out of range)
            if (caster->GetMasterGuid() != target->GetObjectGuid()  &&
                caster->GetObjectGuid() != target->GetMasterGuid()  &&
                caster->GetTypeId() == TYPEID_PLAYER)
            {
                Player* check = caster->GetBeneficiaryPlayer();

                Group* pGroup = check ? check->GetGroup() : nullptr;
                if (pGroup)
                {
                    Player* checkTarget = target->GetBeneficiaryPlayer();
                    if (!checkTarget || !pGroup->SameSubGroup(check, checkTarget))
                        target->RemoveSingleAuraFromSpellAuraHolder(GetId(), GetEffIndex(), GetCasterGuid());
                }
                else
                    target->RemoveSingleAuraFromSpellAuraHolder(GetId(), GetEffIndex(), GetCasterGuid());
            }
        }
        else if (m_areaAuraType == AREA_AURA_RAID)          // Check if on same raid group
        {
            // not check group if target == owner or target == pet
            if (caster->GetMasterGuid() != target->GetObjectGuid() && caster->GetObjectGuid() != target->GetMasterGuid())
            {
                Player* check = caster->GetBeneficiaryPlayer();

                Group* pGroup = check ? check->GetGroup() : nullptr;
                if (pGroup)
                {
                    Player* checkTarget = target->GetBeneficiaryPlayer();
                    if (!checkTarget || !checkTarget->GetGroup() || checkTarget->GetGroup()->GetId() != pGroup->GetId())
                        target->RemoveSingleAuraFromSpellAuraHolder(GetId(), GetEffIndex(), GetCasterGuid());
                }
                else
                    target->RemoveSingleAuraFromSpellAuraHolder(GetId(), GetEffIndex(), GetCasterGuid());
            }
        }
        else if (m_areaAuraType == AREA_AURA_PET || m_areaAuraType == AREA_AURA_OWNER)
        {
            if (target->GetObjectGuid() != caster->GetMasterGuid())
                target->RemoveSingleAuraFromSpellAuraHolder(GetId(), GetEffIndex(), GetCasterGuid());
        }
    }
}

void PersistentAreaAura::Update(uint32 diff)
{
    bool remove = true;
    AuraRemoveMode removeMode = AURA_REMOVE_BY_EXPIRE;

    // remove the aura if its caster or the dynamic object causing it was removed
    // or if the target moves too far from the dynamic object
    if (Unit* caster = GetCaster())
    {
        if (DynamicObject* dynObj = caster->GetDynObject(GetId()))
        {
            if (GetTarget()->GetDistance(dynObj, true, DIST_CALC_COMBAT_REACH) > dynObj->GetRadius())
            {
                removeMode = AURA_REMOVE_BY_DEFAULT;
                dynObj->RemoveAffected(GetTarget()); // let later reapply if target return to range
            }
            else
                remove = false;
        }
    }

    if (remove)
    {
        GetTarget()->RemoveSingleAuraFromSpellAuraHolder(GetHolder(), GetEffIndex(), removeMode);
        return;
    }

    Aura::Update(diff);
}

void Aura::ApplyModifier(bool apply, bool Real)
{
    AuraType aura = m_modifier.m_auraname;

    if (aura < TOTAL_AURAS)
        (*this.*AuraHandler [aura])(apply, Real);
}

bool Aura::isAffectedOnSpell(SpellEntry const* spell) const
{
    return spell->IsFitToFamily(SpellFamily(GetSpellProto()->SpellFamilyName), GetAuraSpellClassMask());
}

bool Aura::CanProcFrom(SpellEntry const* spell, uint32 /*procFlag*/, uint32 EventProcEx, uint32 procEx, bool active, bool useClassMask) const
{
    // Check EffectClassMask
    ClassFamilyMask const& mask  = GetAuraSpellClassMask();

    // allow proc for modifier auras with charges
    if (IsCastEndProcModifierAura(GetSpellProto(), GetEffIndex(), spell))
    {
        if (GetHolder()->GetAuraCharges() > 0)
        {
            if (procEx != PROC_EX_CAST_END && EventProcEx == PROC_EX_NONE)
                return false;
        }
    }
    else if (EventProcEx == PROC_EX_NONE && procEx == PROC_EX_CAST_END)
        return false;

    // if no class mask defined, or spell_proc_event has SpellFamilyName=0 - allow proc
    if (!useClassMask || !mask)
    {
        if (!(EventProcEx & PROC_EX_EX_TRIGGER_ALWAYS))
        {
            // Check for extra req (if none) and hit/crit
            if (EventProcEx == PROC_EX_NONE)
            {
                // No extra req, so can trigger only for active (damage/healing present) and hit/crit
                return ((procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) && active) || procEx == PROC_EX_CAST_END;
            }
                // Passive spells hits here only if resist/reflect/immune/evade
            // Passive spells can`t trigger if need hit (exclude cases when procExtra include non-active flags)
            if ((EventProcEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT) & procEx) && !active)
                return false;
        }
        return true;
    }
    // SpellFamilyName check is performed in SpellMgr::IsSpellProcEventCanTriggeredBy and it is done once for whole holder
    // note: SpellFamilyName is not checked if no spell_proc_event is defined
    return mask.IsFitToFamilyMask(spell->SpellFamilyFlags);
}

void Aura::ReapplyAffectedPassiveAuras(Unit* target, bool owner_mode)
{
    // we need store cast item guids for self casted spells
    // expected that not exist permanent auras from stackable auras from different items
    std::map<uint32, ObjectGuid> affectedSelf;

    std::set<uint32> affectedAuraCaster;

    for (Unit::SpellAuraHolderMap::const_iterator itr = target->GetSpellAuraHolderMap().begin(); itr != target->GetSpellAuraHolderMap().end(); ++itr)
    {
        // permanent passive or permanent area aura
        // passive spells can be affected only by own or owner spell mods)
        if ((itr->second->IsPermanent() && ((owner_mode && itr->second->IsPassive()) || itr->second->IsAreaAura())) &&
                // non deleted and not same aura (any with same spell id)
                !itr->second->IsDeleted() && itr->second->GetId() != GetId() &&
                // and affected by aura
                isAffectedOnSpell(itr->second->GetSpellProto()))
        {
            // only applied by self or aura caster
            if (itr->second->GetCasterGuid() == target->GetObjectGuid())
                affectedSelf[itr->second->GetId()] = itr->second->GetCastItemGuid();
            else if (itr->second->GetCasterGuid() == GetCasterGuid())
                affectedAuraCaster.insert(itr->second->GetId());
        }
    }

    if (!affectedSelf.empty())
    {
        Player* pTarget = target->GetTypeId() == TYPEID_PLAYER ? (Player*)target : nullptr;

        for (std::map<uint32, ObjectGuid>::const_iterator map_itr = affectedSelf.begin(); map_itr != affectedSelf.end(); ++map_itr)
        {
            Item* item = pTarget && map_itr->second ? pTarget->GetItemByGuid(map_itr->second) : nullptr;
            target->RemoveAurasDueToSpell(map_itr->first);
            target->CastSpell(target, map_itr->first, TRIGGERED_OLD_TRIGGERED, item);
        }
    }

    if (!affectedAuraCaster.empty())
    {
        Unit* caster = GetCaster();
        for (uint32 set_itr : affectedAuraCaster)
        {
            target->RemoveAurasDueToSpell(set_itr);
            if (caster)
                caster->CastSpell(GetTarget(), set_itr, TRIGGERED_OLD_TRIGGERED);
        }
    }
}

struct ReapplyAffectedPassiveAurasHelper
{
    explicit ReapplyAffectedPassiveAurasHelper(Aura* _aura) : aura(_aura) {}
    void operator()(Unit* unit) const { aura->ReapplyAffectedPassiveAuras(unit, true); }
    Aura* aura;
};

void Aura::ReapplyAffectedPassiveAuras()
{
    // not reapply spell mods with charges (use original value because processed and at remove)
    if (GetSpellProto()->procCharges)
        return;

    // not reapply some spell mods ops (mostly speedup case)
    switch (m_modifier.m_miscvalue)
    {
        case SPELLMOD_DURATION:
        case SPELLMOD_CHARGES:
        case SPELLMOD_NOT_LOSE_CASTING_TIME:
        case SPELLMOD_CASTING_TIME:
        case SPELLMOD_COOLDOWN:
        case SPELLMOD_COST:
        case SPELLMOD_ACTIVATION_TIME:
        case SPELLMOD_GLOBAL_COOLDOWN:
            return;
    }

    // reapply talents to own passive persistent auras
    ReapplyAffectedPassiveAuras(GetTarget(), true);

    // re-apply talents/passives/area auras applied to pet/totems (it affected by player spellmods)
    GetTarget()->CallForAllControlledUnits(ReapplyAffectedPassiveAurasHelper(this), CONTROLLED_PET | CONTROLLED_TOTEMS);

    // re-apply talents/passives/area auras applied to group members (it affected by player spellmods)
    if (Group* group = ((Player*)GetTarget())->GetGroup())
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            if (Player* member = itr->getSource())
                if (member != GetTarget() && member->IsInMap(GetTarget()))
                    ReapplyAffectedPassiveAuras(member, false);
}

/*********************************************************/
/***               BASIC AURA FUNCTION                 ***/
/*********************************************************/
void Aura::HandleAddModifier(bool apply, bool Real)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER || !Real)
        return;

    if (m_modifier.m_miscvalue >= MAX_SPELLMOD)
        return;

    if (apply)
    {
        SpellEntry const* spellProto = GetSpellProto();

        // Add custom charges for some mod aura
        switch (spellProto->Id)
        {
            case 17941:                                     // Shadow Trance
            case 22008:                                     // Netherwind Focus
            case 31834:                                     // Light's Grace
            case 34754:                                     // Clearcasting
            case 34936:                                     // Backlash
            case 44401:                                     // Missile Barrage
            case 48108:                                     // Hot Streak
            case 51124:                                     // Killing Machine
            case 54741:                                     // Firestarter
            case 57761:                                     // Fireball!
            case 64823:                                     // Elune's Wrath (Balance druid t8 set
                GetHolder()->SetAuraCharges(1);
                break;
        }

        // Everlasting Affliction, overwrite wrong data, if will need more better restore support of spell_affect table
        if (spellProto->SpellFamilyName == SPELLFAMILY_WARLOCK && spellProto->SpellIconID == 3169)
        {
            // Corruption and Unstable Affliction
            // TODO: drop when override will be possible
            SpellEntry* entry = const_cast<SpellEntry*>(spellProto);
            entry->EffectSpellClassMask[GetEffIndex()].Flags = uint64(0x0000010000000002);
        }
        // Improved Flametongue Weapon, overwrite wrong data, maybe time re-add table
        else if (spellProto->Id == 37212)
        {
            // Flametongue Weapon (Passive)
            // TODO: drop when override will be possible
            SpellEntry* entry = const_cast<SpellEntry*>(spellProto);
            entry->EffectSpellClassMask[GetEffIndex()].Flags = uint64(0x0000000000200000);
        }
    }

    ((Player*)GetTarget())->AddSpellMod(this, apply);

    ReapplyAffectedPassiveAuras();
}

void Aura::TriggerSpell()
{
    ObjectGuid casterGUID = GetCasterGuid();
    Unit* triggerTarget = GetTriggerTarget();

    if (!casterGUID || !triggerTarget)
        return;

    // generic casting code with custom spells and target/caster customs
    uint32 trigger_spell_id = GetSpellProto()->EffectTriggerSpell[m_effIndex];

    SpellEntry const* triggeredSpellInfo = sSpellTemplate.LookupEntry<SpellEntry>(trigger_spell_id);
    SpellEntry const* auraSpellInfo = GetSpellProto();
    uint32 auraId = auraSpellInfo->Id;
    Unit* target = GetTarget();
    Unit* triggerCaster = triggerTarget;
    WorldObject* triggerTargetObject = nullptr;

    // specific code for cases with no trigger spell provided in field
    if (triggeredSpellInfo == nullptr)
    {
        switch (auraSpellInfo->SpellFamilyName)
        {
            case SPELLFAMILY_GENERIC:
            {
                switch (auraId)
                {
                    case 812:                               // Periodic Mana Burn
                    {
                        trigger_spell_id = 25779;           // Mana Burn

                        if (GetTarget()->GetTypeId() != TYPEID_UNIT)
                            return;

                        triggerTarget = ((Creature*)GetTarget())->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 0, trigger_spell_id, SELECT_FLAG_POWER_MANA);
                        if (!triggerTarget)
                            return;

                        break;
                    }
//                    // Polymorphic Ray
//                    case 6965: break;
                    case 9712:                              // Thaumaturgy Channel
                        if (Unit* caster = GetCaster())
                            caster->CastSpell(caster, 21029, TRIGGERED_OLD_TRIGGERED);
                        return;
//                    // Egan's Blaster
//                    case 17368: break;
//                    // Haunted
//                    case 18347: break;
//                    // Ranshalla Waiting
//                    case 18953: break;
                    case 19695:                             // Inferno
                    {
                        int32 damageForTick[8] = { 500, 500, 1000, 1000, 2000, 2000, 3000, 5000 };
                        triggerTarget->CastCustomSpell(triggerTarget, 19698, &damageForTick[GetAuraTicks() - 1], nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr);
                        return;
                    }
//                    // Frostwolf Muzzle DND
//                    case 21794: break;
//                    // Alterac Ram Collar DND
//                    case 21866: break;
//                    // Celebras Waiting
//                    case 21916: break;
                    case 23170:                             // Brood Affliction: Bronze
                    {
                        // Only 10% chance of triggering spell, return for the remaining 90%
                        if (urand(0, 9) >= 1)
                            return;
                        target->CastSpell(target, 23171, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    }
                    case 23493:                             // Restoration
                    {
                        uint32 heal = triggerTarget->GetMaxHealth() / 10;
                        uint32 absorb = 0;
                        triggerTarget->CalculateHealAbsorb(heal, &absorb);
                        triggerTarget->DealHeal(triggerTarget, heal - absorb, auraSpellInfo, false, absorb);

                        if (int32 mana = triggerTarget->GetMaxPower(POWER_MANA))
                        {
                            mana /= 10;
                            triggerTarget->EnergizeBySpell(triggerTarget, 23493, mana, POWER_MANA);
                        }
                        return;
                    }
//                    // Stoneclaw Totem Passive TEST
//                    case 23792: break;
//                    // Axe Flurry
//                    case 24018: break;
//                    // Restoration
//                    case 24379: break;
//                    // Happy Pet
//                    case 24716: break;
                    case 24743:                             // Cannon Prep
                    case 24832:                             // Cannon Prep
                    case 42825:                             // Cannon Prep
                        trigger_spell_id = auraId == 42825 ? 42868 : 24731;
                        break;
                    case 24780:                             // Dream Fog
                    {
                        // Note: In 1.12 triggered spell 24781 still exists, need to script dummy effect for this spell then
                        // Select an unfriendly enemy in 100y range and attack it
                        if (target->GetTypeId() != TYPEID_UNIT)
                            return;

                        ThreatList const& tList = target->getThreatManager().getThreatList();
                        for (auto itr : tList)
                        {
                            Unit* pUnit = target->GetMap()->GetUnit(itr->getUnitGuid());

                            if (pUnit && target->getThreatManager().getThreat(pUnit))
                                target->getThreatManager().modifyThreatPercent(pUnit, -100);
                        }

                        if (Unit* pEnemy = target->SelectRandomUnfriendlyTarget(target->getVictim(), 100.0f))
                            ((Creature*)target)->AI()->AttackStart(pEnemy);

                        return;
                    }
//                    // Cannon Prep
//                    case 24832: break;
                    case 24834:                             // Shadow Bolt Whirl
                    {
                        uint32 spellForTick[8] = { 24820, 24821, 24822, 24823, 24835, 24836, 24837, 24838 };
                        uint32 tick = (GetAuraTicks() + 7/*-1*/) % 8;

                        // casted in left/right (but triggered spell have wide forward cone)
                        float forward = target->GetOrientation();
                        if (tick <= 3)
                            target->SetOrientation(forward + 0.75f * M_PI_F - tick * M_PI_F / 8);       // Left
                        else
                            target->SetOrientation(forward - 0.75f * M_PI_F + (8 - tick) * M_PI_F / 8); // Right

                        triggerTarget->CastSpell(triggerTarget, spellForTick[tick], TRIGGERED_OLD_TRIGGERED, nullptr, this, casterGUID);
                        target->SetOrientation(forward);
                        return;
                    }
//                    // Stink Trap
//                    case 24918: break;
//                    // Agro Drones
//                    case 25152: break;
                    case 25371:                             // Consume
                    {
                        int32 bpDamage = triggerTarget->GetMaxHealth() * 10 / 100;
                        triggerTarget->CastCustomSpell(triggerTarget, 25373, &bpDamage, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this, casterGUID);
                        return;
                    }
//                    // Pain Spike
//                    case 25572: break;
                    case 26009:                             // Rotate 360
                    case 26136:                             // Rotate -360
                    {
                        float newAngle = target->GetOrientation();

                        if (auraId == 26009)
                            newAngle += M_PI_F / 40;
                        else
                            newAngle -= M_PI_F / 40;

                        newAngle = MapManager::NormalizeOrientation(newAngle);

                        target->SetFacingTo(newAngle);

                        target->CastSpell(target, 26029, TRIGGERED_OLD_TRIGGERED);
                        return;
                    }
//                    // Consume
//                    case 26196: break;
//                    // Berserk
//                    case 26615: break;
//                    // Defile
//                    case 27177: break;
//                    // Teleport: IF/UC
//                    case 27601: break;
//                    // Five Fat Finger Exploding Heart Technique
//                    case 27673: break;
//                    // Nitrous Boost
//                    case 27746: break;
//                    // Steam Tank Passive
//                    case 27747: break;
                    case 27808:                             // Frost Blast
                    {
                        int32 bpDamage = triggerTarget->GetMaxHealth() * 26 / 100;
                        triggerTarget->CastCustomSpell(triggerTarget, 29879, &bpDamage, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this, casterGUID);
                        return;
                    }
                    // Detonate Mana
                    case 27819:
                    {
                        // 33% Mana Burn on normal mode, 50% on heroic mode
                        int32 bpDamage = (int32)triggerTarget->GetPower(POWER_MANA) / (triggerTarget->GetMap()->GetDifficulty() ? 2 : 3);
                        triggerTarget->ModifyPower(POWER_MANA, -bpDamage);
                        triggerTarget->CastCustomSpell(triggerTarget, 27820, &bpDamage, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this, triggerTarget->GetObjectGuid());
                        return;
                    }
//                    // Controller Timer
//                    case 28095: break;
                    // Stalagg Chain and Feugen Chain
                    case 28096:
                    case 28111:
                    {
                        // X-Chain is casted by Tesla to X, so: caster == Tesla, target = X
                        Unit* pCaster = GetCaster();
                        if (pCaster && pCaster->GetTypeId() == TYPEID_UNIT && !pCaster->IsWithinDistInMap(target, 60.0f))
                        {
                            pCaster->InterruptNonMeleeSpells(true);
                            ((Creature*)pCaster)->SetInCombatWithZone();
                            // Stalagg Tesla Passive or Feugen Tesla Passive
                            pCaster->CastSpell(pCaster, auraId == 28096 ? 28097 : 28109, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, target->GetObjectGuid());
                        }
                        return;
                    }
                    // Stalagg Tesla Passive and Feugen Tesla Passive
                    case 28097:
                    case 28109:
                    {
                        // X-Tesla-Passive is casted by Tesla on Tesla with original caster X, so: caster = X, target = Tesla
                        Unit* pCaster = GetCaster();
                        if (pCaster && pCaster->GetTypeId() == TYPEID_UNIT)
                        {
                            if (pCaster->getVictim() && !pCaster->IsWithinDistInMap(target, 60.0f))
                            {
                                if (Unit* pTarget = ((Creature*)pCaster)->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                                    target->CastSpell(pTarget, 28099, TRIGGERED_NONE);// Shock
                            }
                            else
                            {
                                // "Evade"
                                target->RemoveAurasDueToSpell(auraId);
                                target->CombatStop(true);
                                // Recast chain (Stalagg Chain or Feugen Chain
                                target->CastSpell(pCaster, auraId == 28097 ? 28096 : 28111, TRIGGERED_NONE);
                            }
                        }
                        return;
                    }
//                    // Mark of Didier
//                    case 28114: break;
//                    // Communique Timer, camp
//                    case 28346: break;
//                    // Icebolt
//                    case 28522: break;
//                    // Silithyst
//                    case 29519: break;
                    case 29528:                             // Inoculate Nestlewood Owlkin
                        // prevent error reports in case ignored player target
                        if (triggerTarget->GetTypeId() != TYPEID_UNIT)
                            return;
                        break;
//                    // Return Fire
//                    case 29788: break;
//                    // Return Fire
//                    case 29793: break;
//                    // Return Fire
//                    case 29794: break;
//                    // Guardian of Icecrown Passive
//                    case 29897: break;
                    case 29917:                             // Feed Captured Animal
                        trigger_spell_id = 29916;
                        break;
//                    // Mind Exhaustion Passive
//                    case 30025: break;
//                    // Nether Beam - Serenity
//                    case 30401: break;
                    case 30427:                             // Extract Gas
                    {
                        Unit* caster = GetCaster();
                        if (!caster)
                            return;
                        // move loot to player inventory and despawn target
                        if (caster->GetTypeId() == TYPEID_PLAYER &&
                                triggerTarget->GetTypeId() == TYPEID_UNIT &&
                                ((Creature*)triggerTarget)->GetCreatureInfo()->CreatureType == CREATURE_TYPE_GAS_CLOUD)
                        {
                            Player* player = (Player*)caster;
                            Creature* creature = (Creature*)triggerTarget;
                            // missing lootid has been reported on startup - just return
                            if (!creature->GetCreatureInfo()->SkinningLootId)
                                return;

                            Loot loot(player, creature->GetCreatureInfo()->SkinningLootId, LOOT_SKINNING);
                            loot.AutoStore(player);

                            creature->ForcedDespawn();
                        }
                        return;
                    }
                    case 30576:                             // Quake
                        trigger_spell_id = 30571;
                        break;
//                    // Burning Maul
//                    case 30598: break;
//                    // Regeneration
//                    case 30799:
//                    case 30800:
//                    case 30801:
//                        break;
//                    // Despawn Self - Smoke cloud
//                    case 31269: break;
//                    // Time Rift Periodic
//                    case 31320: break;
//                    // Corrupt Medivh
//                    case 31326: break;
                    case 31347:                             // Doom
                    {
                        target->CastSpell(target, 31350, TRIGGERED_OLD_TRIGGERED);
                        target->DealDamage(target, target->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
                        return;
                    }
                    case 31373:                             // Spellcloth
                    {
                        // Summon Elemental after create item
                        triggerTarget->SummonCreature(17870, 0.0f, 0.0f, 0.0f, triggerTarget->GetOrientation(), TEMPSPAWN_DEAD_DESPAWN, 0);
                        return;
                    }
                    case 31611:                             // Bloodmyst Tesla
                        // no custom effect required; return to avoid spamming with errors
                        return;
                    case 31944:                             // Doomfire
                    {
                        int32 damage = m_modifier.m_amount * ((GetAuraDuration() + m_modifier.periodictime) / GetAuraMaxDuration());
                        triggerTarget->CastCustomSpell(triggerTarget, 31969, &damage, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this, casterGUID);
                        return;
                    }
//                    // Teleport Test
//                    case 32236: break;
                    case 32686:                             // Earthquake
                        if (urand(0, 1)) // 50% chance to trigger
                            triggerTarget->CastSpell(nullptr, 13360, TRIGGERED_OLD_TRIGGERED);
                        break;
//                    // Possess
//                    case 33401: break;
//                    // Draw Shadows
//                    case 33563: break;
//                    // Murmur's Touch
//                    case 33711: break;
                    case 34229:                             // Flame Quills
                    {
                        // cast 24 spells 34269-34289, 34314-34316
                        for (uint32 spell_id = 34269; spell_id != 34290; ++spell_id)
                            triggerTarget->CastSpell(triggerTarget, spell_id, TRIGGERED_OLD_TRIGGERED, nullptr, this, casterGUID);
                        for (uint32 spell_id = 34314; spell_id != 34317; ++spell_id)
                            triggerTarget->CastSpell(triggerTarget, spell_id, TRIGGERED_OLD_TRIGGERED, nullptr, this, casterGUID);
                        return;
                    }
                    case 35268:                             // Inferno (normal and heroic)
                    case 39346:
                    {
                        int32 damage = auraSpellInfo->EffectBasePoints[0];
                        triggerTarget->CastCustomSpell(triggerTarget, 35283, &damage, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this, casterGUID);
                        return;
                    }
                    // Gravity Lapse
                    case 34480:
                    {
                        float x, y, z;
                        target->GetPosition(x, y, z);
                        float floorZ = target->GetMap()->GetHeight(target->GetPhaseMask(), x, y, z);
                        if (std::abs(z - floorZ) < 4.f) // knock up player if he is too close to the ground
                            target->CastSpell(nullptr, 35938, TRIGGERED_OLD_TRIGGERED);

                        return;
                    }
//                    // Tornado
//                    case 34683: break;
//                    // Frostbite Rotate
//                    case 34748: break;
                    case 34821:                               // Arcane Flurry (Melee Component)
                    {
                        trigger_spell_id = 34824;       // (Range Component)

                        if (GetTarget()->GetTypeId() != TYPEID_UNIT)
                            return;

                        triggerTarget = ((Creature*)GetTarget())->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, trigger_spell_id, SELECT_FLAG_PLAYER);
                        if (!triggerTarget)
                            return;

                        break;
                    }
//                    // Interrupt Shutdown
//                    case 35016: break;
//                    // Interrupt Shutdown
//                    case 35176: break;
//                    // Salaadin's Tesla
                    case 35515:
                        return;
//                    // Ethereal Channel (Red)
//                    case 35518: break;
//                    // Nether Vapor
//                    case 35879: break;
//                    // Dark Portal Storm
//                    case 36018: break;
//                    // Burning Maul
//                    case 36056: break;
//                    // Living Grove Defender Lifespan
//                    case 36061: break;
//                    // Professor Dabiri Talks
//                    case 36064: break;
//                    // Kael Gaining Power
                    case 36091:
                    {
                        switch (GetAuraTicks())
                        {
                            case 1:
                                target->CastSpell(target, 36364, TRIGGERED_OLD_TRIGGERED);
                                target->PlayDirectSound(27);
                                target->PlayDirectSound(1136);
                                break;
                            case 2:
                                target->RemoveAurasDueToSpell(36364);
                                target->CastSpell(target, 36370, TRIGGERED_OLD_TRIGGERED);
                                target->PlayDirectSound(27);
                                target->PlayDirectSound(1136);
                                break;
                            case 3:
                                target->RemoveAurasDueToSpell(36370);
                                target->CastSpell(target, 36371, TRIGGERED_OLD_TRIGGERED);
                                target->PlayDirectSound(27);
                                target->PlayDirectSound(1136);
                                break;
                            case 4:
                                if (target->GetTypeId() == TYPEID_UNIT && target->AI())
                                    target->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, target, static_cast<Creature*>(target));
                                break;
                        }
                        break;
                    }
//                    // They Must Burn Bomb Aura
//                    case 36344: break;
//                    // They Must Burn Bomb Aura (self)
//                    case 36350: break;
//                    // Stolen Ravenous Ravager Egg
//                    case 36401: break;
//                    // Activated Cannon
//                    case 36410: break;
//                    // Stolen Ravenous Ravager Egg
//                    case 36418: break;
//                    // Enchanted Weapons
//                    case 36510: break;
//                    // Cursed Scarab Periodic
//                    case 36556: break;
//                    // Cursed Scarab Despawn Periodic
//                    case 36561: break;
//                    // Vision Guide
//                    case 36573: break;
//                    // Cannon Charging (platform)
//                    case 36785: break;
//                    // Cannon Charging (self)
//                    case 36860: break;
                    case 37027:                             // Remote Toy
                        if (urand(0, 4) == 0)               // 20% chance to apply trigger spell
                            trigger_spell_id = 37029;
                        else
                            return;
                        break;
//                    // Mark of Death
//                    case 37125: break;
                    case 37268:                               // Arcane Flurry (Melee Component)
                    {
                        trigger_spell_id = 37271;       // (Range Component, parentspell 37269)

                        if (GetTarget()->GetTypeId() != TYPEID_UNIT)
                            return;

                        triggerTarget = ((Creature*)GetTarget())->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, trigger_spell_id, SELECT_FLAG_PLAYER);
                        if (!triggerTarget)
                            return;

                        break;
                    }
                    case 37429:                             // Spout (left)
                    case 37430:                             // Spout (right)
                    {
                        float newAngle = target->GetOrientation();

                        if (auraId == 37429)
                            newAngle += 2 * M_PI_F / 72;
                        else
                            newAngle -= 2 * M_PI_F / 72;

                        newAngle = MapManager::NormalizeOrientation(newAngle);

                        target->SetFacingTo(newAngle);
                        target->SetOrientation(newAngle);

                        target->CastSpell(target, 37433, TRIGGERED_OLD_TRIGGERED);
                        return;
                    }
//                    // Karazhan - Chess NPC AI, Snapshot timer
//                    case 37440: break;
//                    // Karazhan - Chess NPC AI, action timer
//                    case 37504: break;
//                    // Banish
//                    case 37546: break;
//                    // Shriveling Gaze
//                    case 37589: break;
//                    // Fake Aggro Radius (2 yd)
//                    case 37815: break;
//                    // Corrupt Medivh
//                    case 37853: break;
                    case 38495:                             // Eye of Grillok
                    {
                        target->CastSpell(target, 38530, TRIGGERED_OLD_TRIGGERED);
                        return;
                    }
                    case 38554:                             // Absorb Eye of Grillok (Zezzak's Shard)
                    {
                        if (target->GetTypeId() != TYPEID_UNIT)
                            return;

                        if (Unit* caster = GetCaster())
                            caster->CastSpell(caster, 38495, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        else
                            return;

                        Creature* creatureTarget = (Creature*)target;

                        creatureTarget->ForcedDespawn();
                        return;
                    }
//                    // Magic Sucker Device timer
//                    case 38672: break;
//                    // Tomb Guarding Charging
//                    case 38751: break;
                    // Murmur's Touch
                    case 33711:
                        switch (GetAuraTicks())
                        {
                            case 7:
                            case 10:
                            case 12:
                            case 13:
                            case 14:
                                target->CastSpell(target, 33760, TRIGGERED_OLD_TRIGGERED);
                                break;
                        }
                        return;
                    // Murmur's Touch
                    case 38794:
                        switch (GetAuraTicks())
                        {
                            case 3:
                            case 6:
                            case 7:
                                target->CastSpell(target, 33760, TRIGGERED_OLD_TRIGGERED);
                                break;
                        }
                        return;
                    case 39105:                             // Activate Nether-wraith Beacon (31742 Nether-wraith Beacon item)
                    {
                        float fX, fY, fZ;
                        triggerTarget->GetClosePoint(fX, fY, fZ, triggerTarget->GetObjectBoundingRadius(), 20.0f);
                        Creature* wraith = triggerTarget->SummonCreature(22408, fX, fY, fZ, triggerTarget->GetOrientation(), TEMPSPAWN_DEAD_DESPAWN, 0);
                        if (Unit* caster = GetCaster())
                            wraith->AI()->AttackStart(caster);
                        return;
                    }
//                    // Drain World Tree Visual
//                    case 39140: break;
//                    // Quest - Dustin's Undead Dragon Visual aura
//                    case 39259: break;
//                    // Hellfire - The Exorcism, Jules releases darkness, aura
//                    case 39306: break;
//                    // Enchanted Weapons
//                    case 39489: break;
//                    // Shadow Bolt Whirl
//                    case 39630: break;
//                    // Shadow Bolt Whirl
//                    case 39634: break;
//                    // Shadow Inferno
//                    case 39645: break;
                    case 39857:                             // Tear of Azzinoth Summon Channel - it's not really supposed to do anything,and this only prevents the console spam
                        trigger_spell_id = 39856;
                        break;
//                    // Soulgrinder Ritual Visual (Smashed)
//                    case 39974: break;
//                    // Simon Game Pre-game timer
//                    case 40041: break;
//                    // Knockdown Fel Cannon: The Aggro Check Aura
//                    case 40113: break;
//                    // Spirit Lance
//                    case 40157: break;
                    case 40398:                             // Demon Transform 2
                        switch (GetAuraTicks())
                        {
                            case 1:
                                if (target->HasAura(40506))
                                    target->RemoveAurasDueToSpell(40506);
                                else
                                    trigger_spell_id = 40506;
                                break;
                            case 2:
                                trigger_spell_id = 40510;
                                break;
                        }
                        break;
                    case 40511:                             // Demon Transform 1
                        trigger_spell_id = 40398;
                        break;
                    case 40657:                             // Ancient Flames
                    {
                        // 40720 is called Terokk Shield
                        if (target->GetEntry() == 21838)
                            target->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, target, (Creature*)target);
                        return;
                    }
//                    // Ethereal Ring Cannon: Cannon Aura
//                    case 40734: break;
//                    // Cage Trap
//                    case 40760: break;
//                    // Random Periodic
//                    case 40867: break;
//                    // Prismatic Shield
//                    case 40879: break;
//                    // Aura of Desire
//                    case 41350: break;
//                    // Dementia
//                    case 41404: break;
//                    // Chaos Form
//                    case 41629: break;
//                    // Alert Drums
//                    case 42177: break;
                    case 42581:                             // Spout (left)
                    case 42582:                             // Spout (right)
                    {
                        float newAngle = target->GetOrientation();

                        if (auraId == 42581)
                            newAngle += 2 * M_PI_F / 100;
                        else
                            newAngle -= 2 * M_PI_F / 100;

                        newAngle = MapManager::NormalizeOrientation(newAngle);

                        target->SetFacingTo(newAngle);

                        target->CastSpell(target, auraSpellInfo->CalculateSimpleValue(m_effIndex), TRIGGERED_OLD_TRIGGERED);
                        return;
                    }
//                    // Return to the Spirit Realm
//                    case 44035: break;
//                    // Curse of Boundless Agony
//                    case 45050: break;
//                    // Earthquake
//                    case 46240: break;
                    case 46736:                             // Personalized Weather
                        switch (urand(0, 1))
                        {
                            case 0:
                                return;
                            case 1:
                                trigger_spell_id = 46737;
                                break;
                        }
                        break;
//                    // Stay Submerged
//                    case 46981: break;
//                    // Dragonblight Ram
//                    case 47015: break;
//                    // Party G.R.E.N.A.D.E.
//                    case 51510: break;
//                    // Horseman Abilities
//                    case 52347: break;
//                    // GPS (Greater drake Positioning System)
//                    case 53389: break;
//                    // WotLK Prologue Frozen Shade Summon Aura
//                    case 53459: break;
//                    // WotLK Prologue Frozen Shade Speech
//                    case 53460: break;
//                    // WotLK Prologue Dual-plagued Brain Summon Aura
//                    case 54295: break;
//                    // WotLK Prologue Dual-plagued Brain Speech
//                    case 54299: break;
//                    // Rotate 360 (Fast)
//                    case 55861: break;
//                    // Shadow Sickle
//                    case 56702: break;
//                    // Portal Periodic
//                    case 58008: break;
//                    // Destroy Door Seal
//                    case 58040: break;
//                    // Draw Magic
//                    case 58185: break;
                    case 58886:                             // Food
                    {
                        if (GetAuraTicks() != 1)
                            return;

                        uint32 randomBuff[5] = {57288, 57139, 57111, 57286, 57291};

                        trigger_spell_id = urand(0, 1) ? 58891 : randomBuff[urand(0, 4)];

                        break;
                    }
//                    // Shadow Sickle
//                    case 59103: break;
//                    // Time Bomb
//                    case 59376: break;
//                    // Whirlwind Visual
//                    case 59551: break;
//                    // Hearstrike
//                    case 59783: break;
//                    // Z Check
//                    case 61678: break;
//                    // isDead Check
//                    case 61976: break;
//                    // Start the Engine
//                    case 62432: break;
//                    // Enchanted Broom
//                    case 62571: break;
//                    // Mulgore Hatchling
//                    case 62586: break;
                    case 62679:                             // Durotar Scorpion
                        trigger_spell_id = auraSpellInfo->CalculateSimpleValue(m_effIndex);
                        break;
//                    // Fighting Fish
//                    case 62833: break;
//                    // Shield Level 1
//                    case 63130: break;
//                    // Shield Level 2
//                    case 63131: break;
//                    // Shield Level 3
//                    case 63132: break;
//                    // Food
                    case 64345:                             // Remove Player from Phase
                        target->RemoveSpellsCausingAura(SPELL_AURA_PHASE);
                        return;
//                    case 64445: break;
//                    // Food
//                    case 65418: break;
//                    // Food
//                    case 65419: break;
//                    // Food
//                    case 65420: break;
//                    // Food
//                    case 65421: break;
//                    // Food
//                    case 65422: break;
//                    // Rolling Throw
//                    case 67546: break;
                    case 69012:                             // Explosive Barrage
                    {
                        // Summon an Exploding Orb for each player in combat with the caster
                        ThreatList const& threatList = target->getThreatManager().getThreatList();
                        for (auto itr : threatList)
                        {
                            if (Unit* expectedTarget = target->GetMap()->GetUnit(itr->getUnitGuid()))
                            {
                                if (expectedTarget->GetTypeId() == TYPEID_PLAYER)
                                    target->CastSpell(expectedTarget, 69015, TRIGGERED_OLD_TRIGGERED);
                            }
                        }
                        return;
                    }
//                    // Gunship Cannon Fire
//                    case 70017: break;
//                    // Ice Tomb
//                    case 70157: break;
//                    // Mana Barrier                       // HANDLED IN SD2!
//                    case 70842: break;
//                    // Summon Timer: Suppresser
//                    case 70912: break;
//                    // Aura of Darkness
//                    case 71110: break;
//                    // Aura of Darkness
//                    case 71111: break;
                    case 71441:                             // Unstable Ooze Explosion Suicide Trigger
                        target->DealDamage(target, target->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
                        return;
//                    // Ball of Flames Visual
//                    case 71706: break;
//                    // Summon Broken Frostmourne
//                    case 74081: break;
                    default:
                        break;
                }
                break;
            }
            case SPELLFAMILY_MAGE:
            {
                /*switch (auraId)
                {
                    default:
                        break;
                }*/
                break;
            }
            case SPELLFAMILY_WARRIOR:
            {
                switch(auraId)
                {
                    case 23410:                             // Wild Magic (Mage class call in Nefarian encounter)
                    {
                        trigger_spell_id = 23603;
                        break;
                    }
//                    // Corrupted Totems
//                    case 23425: break;
                    default:
                        break;
                }
                break;
            }
//            case SPELLFAMILY_PRIEST:
//            {
//                switch(auraId)
//                {
//                    // Blue Beam
//                    case 32930: break;
//                    // Fury of the Dreghood Elders
//                    case 35460: break;
//                    default:
//                        break;
//                }
//                break;
//            }
            case SPELLFAMILY_HUNTER:
            {
                switch (auraId)
                {
                    case 53302:                             // Sniper training
                    case 53303:
                    case 53304:
                        if (triggerTarget->GetTypeId() != TYPEID_PLAYER)
                            return;

                        // Reset reapply counter at move
                        if (triggerTarget->IsMoving())
                        {
                            m_modifier.m_amount = 6;
                            return;
                        }

                        // We are standing at the moment
                        if (m_modifier.m_amount > 0)
                        {
                            --m_modifier.m_amount;
                            return;
                        }

                        // select rank of buff
                        switch (auraId)
                        {
                            case 53302: trigger_spell_id = 64418; break;
                            case 53303: trigger_spell_id = 64419; break;
                            case 53304: trigger_spell_id = 64420; break;
                        }

                        // If aura is active - no need to continue
                        if (triggerTarget->HasAura(trigger_spell_id))
                            return;

                        break;
                    default:
                        break;
                }
                break;
            }
            case SPELLFAMILY_DRUID:
            {
                switch (auraId)
                {
                    case 768:                               // Cat Form
                        // trigger_spell_id not set and unknown effect triggered in this case, ignoring for while
                        return;
                    case 22842:                             // Frenzied Regeneration
                    case 22895:
                    case 22896:
                    case 26999:
                    {
                        int32 LifePerRage = GetModifier()->m_amount;

                        int32 lRage = target->GetPower(POWER_RAGE);
                        if (lRage > 100)                    // rage stored as rage*10
                            lRage = 100;
                        target->ModifyPower(POWER_RAGE, -lRage);
                        int32 FRTriggerBasePoints = int32(lRage * LifePerRage / 10);
                        target->CastCustomSpell(target, 22845, &FRTriggerBasePoints, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    }
                    default:
                        break;
                }
                break;
            }

//            case SPELLFAMILY_HUNTER:
//            {
//                switch(auraId)
//                {
//                    // Frost Trap Aura
//                    case 13810:
//                        return;
//                    // Rizzle's Frost Trap
//                    case 39900:
//                        return;
//                    // Tame spells
//                    case 19597:         // Tame Ice Claw Bear
//                    case 19676:         // Tame Snow Leopard
//                    case 19677:         // Tame Large Crag Boar
//                    case 19678:         // Tame Adult Plainstrider
//                    case 19679:         // Tame Prairie Stalker
//                    case 19680:         // Tame Swoop
//                    case 19681:         // Tame Dire Mottled Boar
//                    case 19682:         // Tame Surf Crawler
//                    case 19683:         // Tame Armored Scorpid
//                    case 19684:         // Tame Webwood Lurker
//                    case 19685:         // Tame Nightsaber Stalker
//                    case 19686:         // Tame Strigid Screecher
//                    case 30100:         // Tame Crazed Dragonhawk
//                    case 30103:         // Tame Elder Springpaw
//                    case 30104:         // Tame Mistbat
//                    case 30647:         // Tame Barbed Crawler
//                    case 30648:         // Tame Greater Timberstrider
//                    case 30652:         // Tame Nightstalker
//                        return;
//                    default:
//                        break;
//                }
//                break;
//            }
            case SPELLFAMILY_SHAMAN:
            {
                switch (auraId)
                {
                    case 28820:                             // Lightning Shield (The Earthshatterer set trigger after cast Lighting Shield)
                    {
                        // Need remove self if Lightning Shield not active
                        Unit::SpellAuraHolderMap const& auras = triggerTarget->GetSpellAuraHolderMap();
                        for (const auto& aura : auras)
                        {
                            SpellEntry const* spell = aura.second->GetSpellProto();
                            if (spell->SpellFamilyName == SPELLFAMILY_SHAMAN &&
                                    (spell->SpellFamilyFlags & uint64(0x0000000000000400)))
                                return;
                        }
                        triggerTarget->RemoveAurasDueToSpell(28820);
                        return;
                    }
                    case 38443:                             // Totemic Mastery (Skyshatter Regalia (Shaman Tier 6) - bonus)
                    {
                        if (triggerTarget->IsAllTotemSlotsUsed())
                            triggerTarget->CastSpell(triggerTarget, 38437, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        else
                            triggerTarget->RemoveAurasDueToSpell(38437);
                        return;
                    }
                    default:
                        break;
                }
                break;
            }
            default:
                break;
        }

        // Reget trigger spell proto
        triggeredSpellInfo = sSpellTemplate.LookupEntry<SpellEntry>(trigger_spell_id);
    }
    else                                                    // initial triggeredSpellInfo != nullptr
    {
        // for channeled spell cast applied from aura owner to channel target (persistent aura affects already applied to true target)
        // come periodic casts applied to targets, so need select proper caster (ex. 15790)
        // interesting 2 cases: periodic aura at caster of channeled spell
        if (target->GetObjectGuid() == casterGUID)
            triggerCaster = target;

        switch (triggeredSpellInfo->EffectImplicitTargetA[0])
        {
            case TARGET_LOCATION_UNIT_RANDOM_SIDE: // fireball barrage
            case TARGET_UNIT_ENEMY:
            case TARGET_UNIT:
                triggerCaster = GetCaster();
                triggerTarget = triggerCaster->GetTarget(); // This will default to channel target for channels
                break;
            case TARGET_UNIT_CASTER:
                triggerCaster = target;
                triggerTarget = target;
                break;
            case TARGET_LOCATION_DYNOBJ_POSITION:
                triggerTargetObject = target->GetDynObject(GetId());
            case TARGET_LOCATION_CASTER_DEST:
            case TARGET_LOCATION_CASTER_SRC: // TODO: this needs to be done whenever target isnt important, doing it per case for safety
            default:
                triggerTarget = nullptr;
                break;
        }

        // Spell exist but require custom code
        switch (auraId)
        {
            case 9347:                                      // Mortal Strike
            {
                if (target->GetTypeId() != TYPEID_UNIT)
                    return;
                // expected selection current fight target
                triggerTarget = ((Creature*)target)->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 0, triggeredSpellInfo);
                if (!triggerTarget)
                    return;

                break;
            }
            case 1010:                                      // Curse of Idiocy
            {
                // TODO: spell casted by result in correct way mostly
                // BUT:
                // 1) target show casting at each triggered cast: target don't must show casting animation for any triggered spell
                //      but must show affect apply like item casting
                // 2) maybe aura must be replace by new with accumulative stat mods instead stacking

                // prevent cast by triggered auras
                if (casterGUID == triggerTarget->GetObjectGuid())
                    return;

                // stop triggering after each affected stats lost > 90
                int32 intelectLoss = 0;
                int32 spiritLoss = 0;

                Unit::AuraList const& mModStat = triggerTarget->GetAurasByType(SPELL_AURA_MOD_STAT);
                for (auto i : mModStat)
                {
                    if (i->GetId() == 1010)
                    {
                        switch (i->GetModifier()->m_miscvalue)
                        {
                            case STAT_INTELLECT: intelectLoss += i->GetModifier()->m_amount; break;
                            case STAT_SPIRIT:    spiritLoss   += i->GetModifier()->m_amount; break;
                            default: break;
                        }
                    }
                }

                if (intelectLoss <= -90 && spiritLoss <= -90)
                    return;

                break;
            }
            case 16191:                                     // Mana Tide
            {
                triggerCaster->CastCustomSpell(nullptr, trigger_spell_id, &m_modifier.m_amount, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
            case 19695:                                     // Inferno
            {
                int32 damageForTick[8] = { 500, 500, 1000, 1000, 2000, 2000, 3000, 5000 };
                triggerCaster->CastCustomSpell(nullptr, 19698, &damageForTick[GetAuraTicks() - 1], nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr);
                return;
            }
            case 29768:                                     // Overload
            {
                int32 damage = m_modifier.m_amount * (pow(2.0f, GetAuraTicks()));
                if (damage > 3200)
                    damage = 3200;
                triggerCaster->CastCustomSpell(triggerTarget, triggeredSpellInfo, &damage, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this, casterGUID);
                return;
            }
            case 32930:                                     // Blue beam
                return; // Never seems to go off in sniffs - hides errors
            case 37716:                                     // Demon Link
                triggerTarget = static_cast<TemporarySpawn*>(target)->GetSpawner();
                break;
            case 37850:                                     // Watery Grave
            case 38023:
            case 38024:
            case 38025:
            {
                casterGUID = target->GetObjectGuid();
                break;
            }
            case 38736:                                     // Rod of Purification - for quest 10839 (Veil Skith: Darkstone of Terokk)
            {
                if (Unit* caster = GetCaster())
                    caster->CastSpell(triggerTarget, trigger_spell_id, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
            case 28059:                                     // Positive Charge
            case 28084:                                     // Negative Charge
            case 39088:                                     // Positive Charge
            case 39091:                                     // Negative Charge
            {
                uint32 buffAuraId;
                float range;
                switch (auraId)
                {
                    case 28059:
                        buffAuraId = 29659;
                        range = 13.f;
                        break;
                    case 28084:
                        buffAuraId = 29660;
                        range = 13.f;
                        break;
                    case 39088:
                        buffAuraId = 39089;
                        range = 10.f;
                        break;
                    default:
                    case 39091:
                        buffAuraId = 39092;
                        range = 10.f;
                        break;
                }
                uint32 curCount = 0;
                PlayerList playerList;
                GetPlayerListWithEntryInWorld(playerList, target, range); // official range
                for (Player* player : playerList)
                    if (target != player && player->HasAura(auraId))
                        curCount++;

                target->RemoveAurasDueToSpell(buffAuraId);
                if (curCount)
                    for (uint32 i = 0; i < curCount; i++)
                        target->CastSpell(target, buffAuraId, TRIGGERED_OLD_TRIGGERED);

                break;
            }
            case 36657:                                     // Death Count
            case 38818:                                     // Death Count
            {
                Unit* caster = GetCaster(); // should only go off if caster is still alive
                if (!caster || !caster->isAlive())
                    return;
                break;
            }
            case 43149:                                     // Claw Rage
            {
                // Need to provide explicit target for trigger spell target combination
                target->CastSpell(target->getVictim(), trigger_spell_id, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
            case 33419:                                     // Arcane Missiles - TODO: Review other spells with TARGET_UNIT_ENEMY
            case 40106:                                     // Merge
            case 42483:                                     // Ooze Channel
            {
                triggerCaster = GetCaster();
                break;
            }
            case 44883:                                     // Encapsulate
            case 56505:                                     // Surge of Power
            {
                // Self cast spell, hence overwrite caster (only channeled spell where the triggered spell deals dmg to SELF)
                triggerCaster = triggerTarget;
                break;
            }
            case 53563:                                     // Beacon of Light
                // original caster must be target (beacon)
                target->CastSpell(target, trigger_spell_id, TRIGGERED_OLD_TRIGGERED, nullptr, this, target->GetObjectGuid());
                return;
            case 56654:                                     // Rapid Recuperation (triggered energize have baspioints == 0)
            case 58882:
            {
                int32 mana = target->GetMaxPower(POWER_MANA) * m_modifier.m_amount / 100;
                triggerTarget->CastCustomSpell(triggerTarget, trigger_spell_id, &mana, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
        }
    }

    // All ok cast by default case
    if (triggeredSpellInfo)
    {
        Spell* spell = new Spell(triggerCaster, triggeredSpellInfo, TRIGGERED_OLD_TRIGGERED, casterGUID, GetSpellProto());
        SpellCastTargets targets;
        if (triggeredSpellInfo->Targets & TARGET_FLAG_DEST_LOCATION)
        {
            if (triggerTargetObject)
                targets.setDestination(triggerTargetObject->GetPositionX(), triggerTargetObject->GetPositionY(), triggerTargetObject->GetPositionZ());
            else if (triggerTarget)
                targets.setDestination(triggerTarget->GetPositionX(), triggerTarget->GetPositionY(), triggerTarget->GetPositionZ());
            else
                targets.setDestination(triggerCaster->GetPositionX(), triggerCaster->GetPositionY(), triggerCaster->GetPositionZ());
        }
        if (triggeredSpellInfo->Targets & TARGET_FLAG_SOURCE_LOCATION)
        {
            if (triggerTargetObject)
                targets.setSource(triggerTargetObject->GetPositionX(), triggerTargetObject->GetPositionY(), triggerTargetObject->GetPositionZ());
            else if (triggerTarget)
                targets.setSource(triggerTarget->GetPositionX(), triggerTarget->GetPositionY(), triggerTarget->GetPositionZ());
            else
                targets.setSource(triggerCaster->GetPositionX(), triggerCaster->GetPositionY(), triggerCaster->GetPositionZ());
        }
        if (triggerTarget)
            targets.setUnitTarget(triggerTarget);
        spell->SpellStart(&targets, this);
    }
    else
    {
        if (Unit* caster = GetCaster())
        {
            if (triggerTarget->GetTypeId() != TYPEID_UNIT || !sScriptDevAIMgr.OnEffectDummy(caster, GetId(), GetEffIndex(), (Creature*)triggerTarget, ObjectGuid()))
                sLog.outError("Aura::TriggerSpell: Spell %u have 0 in EffectTriggered[%d], not handled custom case?", GetId(), GetEffIndex());
        }
    }
}

void Aura::TriggerSpellWithValue()
{
    ObjectGuid casterGuid = GetCasterGuid();
    Unit* target = GetTriggerTarget();

    if (!casterGuid || !target)
        return;

    // generic casting code with custom spells and target/caster customs
    uint32 trigger_spell_id = GetSpellProto()->EffectTriggerSpell[m_effIndex];
    int32  basepoints0 = GetModifier()->m_amount;

    target->CastCustomSpell(target, trigger_spell_id, &basepoints0, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this, casterGuid);
}

/*********************************************************/
/***                  AURA EFFECTS                     ***/
/*********************************************************/

void Aura::HandleAuraDummy(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    // AT APPLY
    if (apply)
    {
        switch (GetSpellProto()->SpellFamilyName)
        {
            case SPELLFAMILY_GENERIC:
            {
                switch (GetId())
                {
                    case 1515:                              // Tame beast
                        // FIX_ME: this is 2.0.12 threat effect replaced in 2.1.x by dummy aura, must be checked for correctness
                        if (target->CanHaveThreatList())
                            if (Unit* caster = GetCaster())
                                target->AddThreat(caster, 10.0f, false, GetSpellSchoolMask(GetSpellProto()), GetSpellProto());
                        return;
                    case 7057:                              // Haunting Spirits
                        // expected to tick with 30 sec period (tick part see in Aura::PeriodicTick)
                        m_isPeriodic = true;
                        m_modifier.periodictime = 30 * IN_MILLISECONDS;
                        m_periodicTimer = m_modifier.periodictime;
                        return;
                    case 10255:                             // Stoned
                    {
                        if (Unit* caster = GetCaster())
                        {
                            if (caster->GetTypeId() != TYPEID_UNIT)
                                return;

                            caster->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                            caster->addUnitState(UNIT_STAT_ROOT);
                        }
                        return;
                    }
                    case 13139:                             // net-o-matic
                        // root to self part of (root_target->charge->root_self sequence
                        if (Unit* caster = GetCaster())
                            caster->CastSpell(caster, 13138, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 21094:                             // Separation Anxiety (Majordomo Executus)
                    case 23487:                             // Separation Anxiety (Garr)
                    {
                        // expected to tick with 5 sec period (tick part see in Aura::PeriodicTick)
                        m_isPeriodic = true;
                        m_modifier.periodictime = 5 * IN_MILLISECONDS;
                        m_periodicTimer = m_modifier.periodictime;
                        return;
                    }
                    case 23183:                             // Mark of Frost
                    {
                        if (target->HasAura(23182))
                            target->CastSpell(target, 23186, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCaster()->GetObjectGuid());
                        return;
                    }
                    case 25042:                             // Mark of Nature
                    {
                        if (target->HasAura(25040))
                            target->CastSpell(target, 25043, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCaster()->GetObjectGuid());
                        return;
                    }
                    case 37127:                             // Mark of Death
                    {
                        if (target->HasAura(37128))
                            target->CastSpell(target, 37131, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCaster()->GetObjectGuid());
                        return;
                    }
                    case 28832:                             // Mark of Korth'azz
                    case 28833:                             // Mark of Blaumeux
                    case 28834:                             // Mark of Rivendare
                    case 28835:                             // Mark of Zeliek
                    {
                        int32 damage;
                        switch (GetStackAmount())
                        {
                            case 1:
                                return;
                            case 2: damage =   500; break;
                            case 3: damage =  1500; break;
                            case 4: damage =  4000; break;
                            case 5: damage = 12500; break;
                            default:
                                damage = 14000 + 1000 * GetStackAmount();
                                break;
                        }

                        if (Unit* caster = GetCaster())
                            caster->CastCustomSpell(target, 28836, &damage, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    }
                    case 30410:                             // Shadow Grasp - upon trigger
                    {
                        target->CastSpell(nullptr, 30166, TRIGGERED_OLD_TRIGGERED); // Triggered in sniff
                        break;
                    }
                    case 30166:                             // Shadow Grasp - upon magtheridon
                    {
                        if (target->GetAuraCount(30166) == 5)
                        {
                            target->CastSpell(target, 30168, TRIGGERED_OLD_TRIGGERED); // cast Shadow cage if stacks are 5
                            target->InterruptSpell(CURRENT_CHANNELED_SPELL); // if he is casting blast nova interrupt channel, only magth channel spell
                        }
                        break;
                    }
                    case 31606:                             // Stormcrow Amulet
                    {
                        CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(17970);

                        // we must assume db or script set display id to native at ending flight (if not, target is stuck with this model)
                        if (cInfo)
                            target->SetDisplayId(Creature::ChooseDisplayId(cInfo));

                        return;
                    }
                    case 31736:                                     // Ironvine Seeds
                    {
                        Unit* pCaster = GetCaster();

                        Creature* SteamPumpOverseer = target->SummonCreature(18340, pCaster->GetPositionX()-20, pCaster->GetPositionY()+20, pCaster->GetPositionZ(), target->GetOrientation(), TEMPSPAWN_TIMED_OOC_DESPAWN, 10000);

                        if (SteamPumpOverseer && pCaster)
                            SteamPumpOverseer->GetMotionMaster()->MovePoint(0, pCaster->GetPositionX(), pCaster->GetPositionY(), pCaster->GetPositionZ());

                        return;
                    }
                    case 32045:                             // Soul Charge
                    case 32051:
                    case 32052:
                    {
                        // max duration is 2 minutes, but expected to be random duration
                        // real time randomness is unclear, using max 30 seconds here
                        // see further down for expire of this aura
                        GetHolder()->SetAuraDuration(urand(1, 30)*IN_MILLISECONDS);
                        return;
                    }
                    case 32441:                             // Brittle Bones
                    {
                        m_isPeriodic = true;
                        m_modifier.periodictime = 10 * IN_MILLISECONDS; // randomly applies Rattled 32437
                        m_periodicTimer = 0;
                        return;
                    }
                    case 33326:                             // Stolen Soul Dispel
                    {
                        target->RemoveAurasDueToSpell(32346);
                        return;
                    }
                    case 36550:                             // Floating Drowned
                    {
                        // Possibly need some of the below to fix Vengeful Harbinger flying

                        //if (Unit* caster = GetCaster())
                        //{
                        //    caster->SetByteValue(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_FLY_ANIM);
                        //    caster->RemoveByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_ALWAYS_STAND);
                        //    caster->SetHover(true);
                        //    caster->SetLevitate(true);
                        //    caster->SetCanFly(true);
                        //}
                        return;
                    }
                    case 36089:
                    case 36090:                             // Netherbeam - Kaelthas
                    {
                        float speed = target->GetBaseRunSpeed(); // fetch current base speed
                        target->ApplyModPositiveFloatValue(OBJECT_FIELD_SCALE_X, float(m_modifier.m_amount) / 100, apply);
                        target->UpdateModelData(); // resets speed
                        target->SetBaseRunSpeed(speed + (1.f / 7.f));
                        target->UpdateSpeed(MOVE_RUN, true); // sends speed packet
                        return;
                    }
                    case 36587:                             // Vision Guide
                    {
                        target->CastSpell(target, 36573, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    }
                    // Gender spells
                    case 38224:                             // Illidari Agent Illusion
                    case 37096:                             // Blood Elf Illusion
                    case 46354:                             // Blood Elf Illusion
                    {
                        uint8 gender = target->getGender();
                        uint32 spellId;
                        switch (GetId())
                        {
                            case 38224: spellId = (gender == GENDER_MALE ? 38225 : 38227); break;
                            case 37096: spellId = (gender == GENDER_MALE ? 37093 : 37095); break;
                            case 46354: spellId = (gender == GENDER_MALE ? 46355 : 46356); break;
                            default: return;
                        }
                        target->CastSpell(target, spellId, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    }
                    case 37750:                             // Clear Consuming Madness
                        if (target->HasAura(37749))
                            target->RemoveAurasDueToSpell(37749);
                        return;
                    case 39850:                             // Rocket Blast
                        if (roll_chance_i(20))              // backfire stun
                            target->CastSpell(target, 51581, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 40856:                                     // Wrangling Rope
                    {
                        if (target->GetTypeId() != TYPEID_UNIT)
                            return;

                        if (Unit* caster = GetCaster())
                            target->CastSpell(caster, 40917, TRIGGERED_NONE); // Wrangle Aether Rays: Character Force Cast

                        static_cast<Creature*>(target)->ForcedDespawn();

                        return;
                    }
                    case 40926:                                     // Wrangle Aether Rays: Wrangling Rope Channel
                    {
                        if (target->GetTypeId() != TYPEID_PLAYER)
                            return;

                        if (Unit* caster = GetCaster())
                            caster->GetMotionMaster()->MoveFollow(target, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE, true);

                        return;
                    }
                    case 42416:                             // Apexis Mob Faction Check Aura
                    {
                        if (target->GetTypeId() != TYPEID_UNIT)
                            return;

                        if (target->GetPositionX() > 3000.f)
                            ((Creature*)target)->UpdateEntry(22243);
                        else
                            ((Creature*)target)->UpdateEntry(23386);
                        return;
                    }
                    case 43873:                             // Headless Horseman Laugh
                        target->PlayDistanceSound(11965);
                        return;
                    case 45963:                             // Call Alliance Deserter
                    {
                        // Escorting Alliance Deserter
                        if (target->GetMiniPet())
                            target->CastSpell(target, 45957, TRIGGERED_OLD_TRIGGERED);

                        return;
                    }
                    case 46637:                             // Break Ice
                        target->CastSpell(target, 46638, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 46699:                             // Requires No Ammo
                        if (target->GetTypeId() == TYPEID_PLAYER)
                            // not use ammo and not allow use
                            ((Player*)target)->RemoveAmmo();
                        return;
                    case 47190:                             // Toalu'u's Spiritual Incense
                        target->CastSpell(target, 47189, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        // allow script to process further (text)
                        break;
                    case 47563:                             // Freezing Cloud
                        target->CastSpell(target, 47574, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 47593:                             // Freezing Cloud
                        target->CastSpell(target, 47594, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 48025:                             // Headless Horseman's Mount
                        Spell::SelectMountByAreaAndSkill(target, GetSpellProto(), 51621, 48024, 51617, 48023, 0);
                        return;
                    case 48143:                             // Forgotten Aura
                        // See Death's Door
                        target->CastSpell(target, 48814, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 51405:                             // Digging for Treasure
                        target->HandleEmote(EMOTE_STATE_WORK);
                        // Pet will be following owner, this makes him stop
                        target->addUnitState(UNIT_STAT_STUNNED);
                        return;
                    case 54729:                             // Winged Steed of the Ebon Blade
                        Spell::SelectMountByAreaAndSkill(target, GetSpellProto(), 0, 0, 54726, 54727, 0);
                        return;
                    case 58600:                             // Restricted Flight Area (Dalaran)
                    case 58730:                             // Restricted Flight Area (Wintergrasp)
                    {
                        if (!target || target->GetTypeId() != TYPEID_PLAYER)
                            return;
                        const char* text = sObjectMgr.GetMangosString(LANG_NO_FLY_ZONE, ((Player*)target)->GetSession()->GetSessionDbLocaleIndex());
                        target->MonsterWhisper(text, target, true);
                        return;
                    }
                    case 61187:                             // Twilight Shift (single target)
                    case 61190:                             // Twilight Shift (many targets)
                        target->RemoveAurasDueToSpell(57620);
                        target->CastSpell(target, 61885, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 62061:                             // Festive Holiday Mount
                        if (target->HasAuraType(SPELL_AURA_MOUNTED))
                            // Reindeer Transformation
                            target->CastSpell(target, 25860, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 62109:                             // Tails Up: Aura
                        target->setFaction(1990);           // Ambient (hostile)
                        target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PLAYER);
                        return;
                    case 63122:                             // Clear Insane
                        target->RemoveAurasDueToSpell(GetSpellProto()->CalculateSimpleValue(m_effIndex));
                        return;
                    case 63624:                             // Learn a Second Talent Specialization
                        // Teach Learn Talent Specialization Switches, required for client triggered casts, allow after 30 sec delay
                        if (target->GetTypeId() == TYPEID_PLAYER)
                            ((Player*)target)->learnSpell(63680, false);
                        return;
                    case 63651:                             // Revert to One Talent Specialization
                        // Teach Learn Talent Specialization Switches, remove
                        if (target->GetTypeId() == TYPEID_PLAYER)
                            ((Player*)target)->removeSpell(63680);
                        return;
                    case 64132:                             // Constrictor Tentacle
                        if (target->GetTypeId() == TYPEID_PLAYER)
                            target->CastSpell(target, 64133, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 65684:                             // Dark Essence
                        target->RemoveAurasDueToSpell(65686);
                        return;
                    case 65686:                             // Light Essence
                        target->RemoveAurasDueToSpell(65684);
                        return;
                    case 68912:                             // Wailing Souls
                        if (Unit* caster = GetCaster())
                        {
                            caster->SetTarget(target);

                            // TODO - this is confusing, it seems the boss should channel this aura, and start casting the next spell
                            caster->CastSpell(caster, 68899, TRIGGERED_NONE);
                        }
                        return;
                    case 70623:                             // Jaina's Call
                        if (target->GetTypeId() == TYPEID_PLAYER)
                            target->CastSpell(target, 70525, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 70638:                             // Call of Sylvanas
                        if (target->GetTypeId() == TYPEID_PLAYER)
                            target->CastSpell(target, 70639, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 71342:                             // Big Love Rocket
                        Spell::SelectMountByAreaAndSkill(target, GetSpellProto(), 71344, 71345, 71346, 71347, 0);
                        return;
                    case 71563:                             // Deadly Precision
                        target->CastSpell(target, 71564, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    case 72286:                             // Invincible
                        Spell::SelectMountByAreaAndSkill(target, GetSpellProto(), 72281, 72282, 72283, 72284, 0);
                        return;
                    case 74856:                             // Blazing Hippogryph
                        Spell::SelectMountByAreaAndSkill(target, GetSpellProto(), 0, 0, 74854, 74855, 0);
                        return;
                    case 75614:                             // Celestial Steed
                        Spell::SelectMountByAreaAndSkill(target, GetSpellProto(), 75619, 75620, 75617, 75618, 76153);
                        return;
                    case 75973:                             // X-53 Touring Rocket
                        Spell::SelectMountByAreaAndSkill(target, GetSpellProto(), 0, 0, 75957, 75972, 76154);
                        return;
                }
                break;
            }
            case SPELLFAMILY_WARRIOR:
            {
                switch (GetId())
                {
                    case 23427:								// Summon Infernals (Warlock class call in Nefarian encounter)
                    {
                        if (Unit* target = GetTarget())
                            target->CastSpell(target, 23426, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                    }
                    case 41099:                             // Battle Stance
                    {
                        if (target->GetTypeId() != TYPEID_UNIT)
                            return;

                        // Stance Cooldown
                        target->CastSpell(target, 41102, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // Battle Aura
                        target->CastSpell(target, 41106, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // equipment
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32614);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
                        return;
                    }
                    case 41100:                             // Berserker Stance
                    {
                        if (target->GetTypeId() != TYPEID_UNIT)
                            return;

                        // Stance Cooldown
                        target->CastSpell(target, 41102, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // Berserker Aura
                        target->CastSpell(target, 41107, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // equipment
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32614);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
                        return;
                    }
                    case 41101:                             // Defensive Stance
                    {
                        if (target->GetTypeId() != TYPEID_UNIT)
                            return;

                        // Stance Cooldown
                        target->CastSpell(target, 41102, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // Defensive Aura
                        target->CastSpell(target, 41105, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // equipment
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32604);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 31467);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
                        return;
                    }
                    case 53790:                             // Defensive Stance
                    {
                        if (target->GetTypeId() != TYPEID_UNIT)
                            return;

                        // Stance Cooldown
                        target->CastSpell(target, 59526, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // Defensive Aura
                        target->CastSpell(target, 41105, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // equipment
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43625);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 39384);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
                        return;
                    }
                    case 53791:                             // Berserker Stance
                    {
                        if (target->GetTypeId() != TYPEID_UNIT)
                            return;

                        // Stance Cooldown
                        target->CastSpell(target, 59526, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // Berserker Aura
                        target->CastSpell(target, 41107, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // equipment
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43625);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 43625);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
                        return;
                    }
                    case 53792:                             // Battle Stance
                    {
                        if (target->GetTypeId() != TYPEID_UNIT)
                            return;

                        // Stance Cooldown
                        target->CastSpell(target, 59526, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // Battle Aura
                        target->CastSpell(target, 41106, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                        // equipment
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 43623);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
                        ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
                        return;
                    }
                }

                // Overpower
                if (GetSpellProto()->SpellFamilyFlags & uint64(0x0000000000000004))
                {
                    // Must be casting target
                    if (!target->IsNonMeleeSpellCasted(false))
                        return;

                    Unit* caster = GetCaster();
                    if (!caster)
                        return;

                    Unit::AuraList const& modifierAuras = caster->GetAurasByType(SPELL_AURA_ADD_FLAT_MODIFIER);
                    for (auto modifierAura : modifierAuras)
                    {
                        // Unrelenting Assault
                        if (modifierAura->GetSpellProto()->SpellFamilyName == SPELLFAMILY_WARRIOR && modifierAura->GetSpellProto()->SpellIconID == 2775)
                        {
                            switch (modifierAura->GetSpellProto()->Id)
                            {
                                case 46859:                 // Unrelenting Assault, rank 1
                                    target->CastSpell(target, 64849, TRIGGERED_OLD_TRIGGERED, nullptr, modifierAura);
                                    break;
                                case 46860:                 // Unrelenting Assault, rank 2
                                    target->CastSpell(target, 64850, TRIGGERED_OLD_TRIGGERED, nullptr, modifierAura);
                                    break;
                                default:
                                    break;
                            }
                            break;
                        }
                    }
                    return;
                }
                break;
            }
            case SPELLFAMILY_MAGE:
                break;
            case SPELLFAMILY_HUNTER:
            {
                switch (GetId())
                {
                    case 34026:                             // Kill Command
                        target->CastSpell(target, 34027, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                }
                break;
            }
            //case SPELLFAMILY_PALADIN:
            //{
            //    break;
            //}
            case SPELLFAMILY_SHAMAN:
            {
                switch (GetId())
                {
                    case 55198:                             // Tidal Force
                        target->CastSpell(target, 55166, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        return;
                }

                // Earth Shield
                if ((GetSpellProto()->SpellFamilyFlags & uint64(0x40000000000)))
                {
                    // prevent double apply bonuses
                    if (target->GetTypeId() != TYPEID_PLAYER || !((Player*)target)->GetSession()->PlayerLoading())
                    {
                        if (Unit* caster = GetCaster())
                        {
                            m_modifier.m_amount = caster->SpellHealingBonusDone(target, GetSpellProto(), m_modifier.m_amount, SPELL_DIRECT_DAMAGE);
                            m_modifier.m_amount = target->SpellHealingBonusTaken(caster, GetSpellProto(), m_modifier.m_amount, SPELL_DIRECT_DAMAGE);
                        }
                    }
                    return;
                }
                break;
            }
            case SPELLFAMILY_PRIEST:
            {
                switch (GetId())
                {
                    case 30238:             // Lordaeron's Blessing
                    {
                        target->CastSpell(target, 31906, TRIGGERED_OLD_TRIGGERED);
                        return;
                    }
                }
                break;
            }
        }
    }
    // AT REMOVE
    else
    {
        if (IsQuestTameSpell(GetId()) && target->isAlive())
        {
            Unit* caster = GetCaster();
            if (!caster || !caster->isAlive())
                return;

            uint32 finalSpellId = 0;
            switch (GetId())
            {
                case 19548: finalSpellId = 19597; break;
                case 19674: finalSpellId = 19677; break;
                case 19687: finalSpellId = 19676; break;
                case 19688: finalSpellId = 19678; break;
                case 19689: finalSpellId = 19679; break;
                case 19692: finalSpellId = 19680; break;
                case 19693: finalSpellId = 19684; break;
                case 19694: finalSpellId = 19681; break;
                case 19696: finalSpellId = 19682; break;
                case 19697: finalSpellId = 19683; break;
                case 19699: finalSpellId = 19685; break;
                case 19700: finalSpellId = 19686; break;
                case 30646: finalSpellId = 30647; break;
                case 30653: finalSpellId = 30648; break;
                case 30654: finalSpellId = 30652; break;
                case 30099: finalSpellId = 30100; break;
                case 30102: finalSpellId = 30103; break;
                case 30105: finalSpellId = 30104; break;
            }

            if (finalSpellId)
                caster->CastSpell(target, finalSpellId, TRIGGERED_OLD_TRIGGERED, nullptr, this);

            return;
        }

        switch (GetId())
        {
            case 10255:                                     // Stoned
            {
                if (Unit* caster = GetCaster())
                {
                    if (caster->GetTypeId() != TYPEID_UNIT)
                        return;

                    // see dummy effect of spell 10254 for removal of flags etc
                    caster->CastSpell(caster, 10254, TRIGGERED_OLD_TRIGGERED);
                }
                return;
            }
            case 12479:                                     // Hex of Jammal'an
                target->CastSpell(target, 12480, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            case 12774:                                     // (DND) Belnistrasz Idol Shutdown Visual
            {
                if (m_removeMode == AURA_REMOVE_BY_DEATH)
                    return;

                // Idom Rool Camera Shake <- wtf, don't drink while making spellnames?
                if (Unit* caster = GetCaster())
                    caster->CastSpell(caster, 12816, TRIGGERED_OLD_TRIGGERED);

                return;
            }
            case 17189:                                     // Frostwhisper's Lifeblood
                // Ras Frostwhisper gets back to full health when turned to his human form
                if (Unit* caster = GetCaster())
                    caster->ModifyHealth(caster->GetMaxHealth() - caster->GetHealth());
                return;
            case 25185:                                     // Itch
            {
                GetCaster()->CastSpell(target, 25187, TRIGGERED_OLD_TRIGGERED);
                return;
            }
            case 26077:                                     // Itch
            {
                GetCaster()->CastSpell(target, 26078, TRIGGERED_OLD_TRIGGERED);
                return;
            }
            case 27243:                                     // Seed of Corruption
            {
                if (m_removeMode == AURA_REMOVE_BY_DEATH)
                    target->CastSpell(target, 27285, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                return;
            }
            case 28169:                                     // Mutating Injection
            {
                // Mutagen Explosion
                target->CastSpell(target, 28206, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                // Poison Cloud
                target->CastSpell(target, 28240, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
            case 28059:                                     // Positive Charge
                target->RemoveAurasDueToSpell(29659);
                return;
            case 28084:                                     // Negative Charge
                target->RemoveAurasDueToSpell(29660);
                return;
            case 30410:                                     // Shadow Grasp - upon trigger
            {
                target->InterruptSpell(CURRENT_CHANNELED_SPELL);
                break;
            }
            case 30166:                                     // Shadow Grasp - upon magtheridon
            {
                if (target->HasAura(30168))
                    target->RemoveAurasDueToSpell(30168); // remove Shadow cage if stacks are 5
            }
            case 30238:                                     // Lordaeron's Bleesing
            {
                target->RemoveAurasDueToSpell(31906);
                return;
            }
            case 32045:                                     // Soul Charge
            {
                if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                    target->CastSpell(target, 32054, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                return;
            }
            case 32051:                                     // Soul Charge
            {
                if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                    target->CastSpell(target, 32057, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                return;
            }
            case 32052:                                     // Soul Charge
            {
                if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                    target->CastSpell(target, 32053, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                return;
            }
            case 35016:										// Interrupt shutdown
            case 35176:										// Interrupt shutdown (ara)
            {
                if (m_removeMode == AURA_REMOVE_BY_DEFAULT)
                {
                    Unit* caster = GetCaster();
                    if (caster && GetAuraDuration() <= 100) // only fail if finished cast (seems to finish with .1 seconds left)
                        if (Creature* summoner = caster->GetMap()->GetCreature(caster->GetSpawnerGuid()))
                            caster->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, caster, summoner);
                }
                return;
            }
            case 35079:                                     // Misdirection, triggered buff
            case 59628:                                     // Tricks of the Trade, triggered buff
            {
                if (Unit* pCaster = GetCaster())
                    pCaster->getHostileRefManager().ResetThreatRedirection();
                return;
            }
            case 36730:                                     // Flame Strike
            {
                target->CastSpell(target, 36731, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
            case 39088:                                     // Positive Charge
                target->RemoveAurasDueToSpell(39089);
                return;
            case 39091:                                     // Negative Charge
                target->RemoveAurasDueToSpell(39092);
                return;
            case 40830:                                     // Banish the Demons: Banishment Beam Periodic Aura Effect
            {
                if (m_removeMode == AURA_REMOVE_BY_DEATH)
                    target->CastSpell(nullptr, 40828, TRIGGERED_OLD_TRIGGERED);
                return;
            }
            case 41099:                                     // Battle Stance
            {
                // Battle Aura
                target->RemoveAurasDueToSpell(41106);
                return;
            }
            case 41100:                                     // Berserker Stance
            {
                // Berserker Aura
                target->RemoveAurasDueToSpell(41107);
                return;
            }
            case 41101:                                     // Defensive Stance
            {
                // Defensive Aura
                target->RemoveAurasDueToSpell(41105);
                return;
            }
            case 42385:                                     // Alcaz Survey Aura
            {
                target->CastSpell(target, 42316, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
            case 42517:                                     // Beam to Zelfrax
            {
                // expecting target to be a dummy creature
                Creature* pSummon = target->SummonCreature(23864, 0.0f, 0.0f, 0.0f, target->GetOrientation(), TEMPSPAWN_DEAD_DESPAWN, 0);

                Unit* pCaster = GetCaster();

                if (pSummon && pCaster)
                    pSummon->GetMotionMaster()->MovePoint(0, pCaster->GetPositionX(), pCaster->GetPositionY(), pCaster->GetPositionZ());

                return;
            }
            case 43681:                                     // Inactive
            {
                if (m_removeMode == AURA_REMOVE_BY_EXPIRE && target->GetTypeId() == TYPEID_PLAYER)
                    ((Player*)target)->ToggleAFK();
                return;
            }
            case 43969:                                     // Feathered Charm
            {
                // Steelfeather Quest Credit, Are there any requirements for this, like area?
                if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                    target->CastSpell(target, 43984, TRIGGERED_OLD_TRIGGERED);

                return;
            }
            case 44191:                                     // Flame Strike
            {
                if (target->GetMap()->IsDungeon())
                {
                    uint32 spellId = target->GetMap()->IsRegularDifficulty() ? 44190 : 46163;

                    target->CastSpell(target, spellId, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                }
                return;
            }
            case 45934:                                     // Dark Fiend
            {
                // Kill target if dispelled
                if (m_removeMode == AURA_REMOVE_BY_DISPEL)
                    target->DealDamage(target, target->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
                return;
            }
            case 45963:                                     // Call Alliance Deserter
            {
                // Escorting Alliance Deserter
                target->RemoveAurasDueToSpell(45957);
                return;
            }
            case 46308:                                     // Burning Winds
            {
                // casted only at creatures at spawn
                target->CastSpell(target, 47287, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
            case 46637:                                     // Break Ice
            {
                target->CastSpell(target, 47030, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
            case 46736:                                     // Personalized Weather
            case 46738:
            case 46739:
            case 46740:
            {
                uint32 spellId = 0;
                switch (urand(0, 5))
                {
                    case 0: spellId = 46736; break;
                    case 1: spellId = 46738; break;
                    case 2: spellId = 46739; break;
                    case 3: spellId = 46740; break;
                    case 4: return;
                }
                target->CastSpell(target, spellId, TRIGGERED_OLD_TRIGGERED);
                break;
            }
            case 48385:                                     // Create Spirit Fount Beam
            {
                target->CastSpell(target, target->GetMap()->IsRegularDifficulty() ? 48380 : 59320, TRIGGERED_OLD_TRIGGERED);
                return;
            }
            case 50141:                                     // Blood Oath
            {
                // Blood Oath
                if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                    target->CastSpell(target, 50001, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                return;
            }
            case 51405:                                     // Digging for Treasure
            {
                const uint32 spell_list[7] =
                {
                    51441,                                  // hare
                    51397,                                  // crystal
                    51398,                                  // armor
                    51400,                                  // gem
                    51401,                                  // platter
                    51402,                                  // treasure
                    51443                                   // bug
                };

                target->CastSpell(target, spell_list[urand(0, 6)], TRIGGERED_OLD_TRIGGERED);

                target->HandleEmote(EMOTE_STATE_NONE);
                target->clearUnitState(UNIT_STAT_STUNNED);
                return;
            }
            case 51870:                                     // Collect Hair Sample
            {
                if (Unit* pCaster = GetCaster())
                {
                    if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                        pCaster->CastSpell(target, 51872, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                }

                return;
            }
            case 52098:                                     // Charge Up
            {
                if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                    target->CastSpell(target, 52092, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                return;
            }
            case 53039:                                     // Deploy Parachute
            {
                // Crusader Parachute
                target->RemoveAurasDueToSpell(53031);
                return;
            }
            case 53790:                                     // Defensive Stance
            {
                // Defensive Aura
                target->RemoveAurasDueToSpell(41105);
                return;
            }
            case 53791:                                     // Berserker Stance
            {
                // Berserker Aura
                target->RemoveAurasDueToSpell(41107);
                return;
            }
            case 53792:                                     // Battle Stance
            {
                // Battle Aura
                target->RemoveAurasDueToSpell(41106);
                return;
            }
            case 56511:                                     // Towers of Certain Doom: Tower Bunny Smoke Flare Effect
            {
                // Towers of Certain Doom: Skorn Cannonfire
                if (m_removeMode == AURA_REMOVE_BY_DEFAULT)
                    target->CastSpell(target, 43069, TRIGGERED_OLD_TRIGGERED);

                return;
            }
            case 58600:                                     // Restricted Flight Area (Dalaran)
            case 58730:                                     // Restricted Flight Area (Wintergrasp)
            {
                AreaTableEntry const* area = GetAreaEntryByAreaID(target->GetAreaId());

                // Dalaran restricted flight zone (recheck before apply unmount)
                if (area && target->GetTypeId() == TYPEID_PLAYER && ((GetId() == 58600 && area->flags & AREA_FLAG_CANNOT_FLY) || (GetId() == 58730 && area->flags & AREA_FLAG_OUTDOOR_PVP)) &&
                        ((Player*)target)->IsFreeFlying() && !((Player*)target)->isGameMaster())
                {
                    target->CastSpell(target, 58601, TRIGGERED_OLD_TRIGGERED); // Remove Flight Auras (also triggered Parachute (45472))
                }
                return;
            }
            case 61900:                                     // Electrical Charge
            {
                if (m_removeMode == AURA_REMOVE_BY_DEATH)
                    target->CastSpell(target, GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_0), TRIGGERED_OLD_TRIGGERED);

                return;
            }
            case 68839:                                     // Corrupt Soul
            {
                // Knockdown Stun
                target->CastSpell(target, 68848, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                // Draw Corrupted Soul
                target->CastSpell(target, 68846, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
            case 70308:                                     // Mutated Transformation
            {
                if (target->GetMap()->IsDungeon())
                {
                    uint32 spellId;

                    Difficulty diff = target->GetMap()->GetDifficulty();
                    if (diff == RAID_DIFFICULTY_10MAN_NORMAL || diff == RAID_DIFFICULTY_10MAN_HEROIC)
                        spellId = 70311;
                    else
                        spellId = 71503;

                    target->CastSpell(target, spellId, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                }
                return;
            }
        }

        // Living Bomb
        if (GetSpellProto()->SpellFamilyName == SPELLFAMILY_MAGE && (GetSpellProto()->SpellFamilyFlags & uint64(0x2000000000000)))
        {
            if (m_removeMode == AURA_REMOVE_BY_EXPIRE || m_removeMode == AURA_REMOVE_BY_DISPEL)
                target->CastSpell(target, m_modifier.m_amount, TRIGGERED_OLD_TRIGGERED, nullptr, this);

            return;
        }
    }

    // AT APPLY & REMOVE

    switch (GetSpellProto()->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (GetId())
            {
                case 6606:                                  // Self Visual - Sleep Until Cancelled (DND)
                {
                    if (apply)
                    {
                        target->SetStandState(UNIT_STAND_STATE_SLEEP);
                        target->addUnitState(UNIT_STAT_ROOT);
                    }
                    else
                    {
                        target->clearUnitState(UNIT_STAT_ROOT);
                        target->SetStandState(UNIT_STAND_STATE_STAND);
                    }

                    return;
                }
                case 11196:                                 // Recently Bandaged
                    target->ApplySpellImmune(this, IMMUNITY_MECHANIC, GetMiscValue(), apply);
                    return;
                case 16093:                                  // Self Visual - Sleep Until Cancelled (DND)
                {
                    if (apply)
                    {
                        target->SetStandState(UNIT_STAND_STATE_SLEEP);
                        target->addUnitState(UNIT_STAT_ROOT);
                    }
                    else
                    {
                        target->clearUnitState(UNIT_STAT_ROOT);
                        target->SetStandState(UNIT_STAND_STATE_STAND);
                    }

                    return;
                }
                case 24658:                                 // Unstable Power
                {
                    if (apply)
                    {
                        Unit* caster = GetCaster();
                        if (!caster)
                            return;

                        caster->CastSpell(target, 24659, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                    }
                    else
                        target->RemoveAurasDueToSpell(24659);
                    return;
                }
                case 24661:                                 // Restless Strength
                {
                    if (apply)
                    {
                        Unit* caster = GetCaster();
                        if (!caster)
                            return;

                        caster->CastSpell(target, 24662, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                    }
                    else
                        target->RemoveAurasDueToSpell(24662);
                    return;
                }
                case 29266:                                 // Permanent Feign Death
                case 31261:                                 // Permanent Feign Death (Root)
                case 37493:                                 // Feign Death
                case 52593:                                 // Bloated Abomination Feign Death
                case 55795:                                 // Falling Dragon Feign Death
                case 57626:                                 // Feign Death
                case 57685:                                 // Permanent Feign Death
                case 58768:                                 // Permanent Feign Death (Freeze Jumpend)
                case 58806:                                 // Permanent Feign Death (Drowned Anim)
                case 58951:                                 // Permanent Feign Death
                case 64461:                                 // Permanent Feign Death (No Anim) (Root)
                case 65985:                                 // Permanent Feign Death (Root Silence Pacify)
                case 70592:                                 // Permanent Feign Death
                case 70628:                                 // Permanent Feign Death
                case 70630:                                 // Frozen Aftermath - Feign Death
                case 71598:                                 // Feign Death
                {
                    // Unclear what the difference really is between them.
                    // Some has effect1 that makes the difference, however not all.
                    // Some appear to be used depending on creature location, in water, at solid ground, in air/suspended, etc
                    // For now, just handle all the same way
                    //if (target->GetTypeId() == TYPEID_UNIT)
                    target->SetFeignDeath(apply, GetCasterGuid(), GetId());

                    return;
                }
                case 32096:                                 // Thrallmar's Favor
                case 32098:                                 // Honor Hold's Favor
                    if (target->GetTypeId() == TYPEID_PLAYER)
                    {
                        if (apply) // cast/remove Buffbot Buff Effect
                            target->CastSpell(target, 32172, TRIGGERED_NONE);
                        else
                            target->RemoveAurasDueToSpell(32172);
                    }
                    return;
                case 32567:                                 // Green Banish State
                {
                    target->SetHover(apply);
                    return;
                }
                case 35519:                                 // White Beam
                {
                    target->SetLevitate(apply);
                    target->SetHover(apply);
                    return;
                }
                case 35356:                                 // Spawn Feign Death
                case 35357:                                 // Spawn Feign Death
                case 42557:                                 // Feign Death
                case 51329:                                 // Feign Death
                {
                    // UNIT_DYNFLAG_DEAD does not appear with these spells.
                    // All of the spells appear to be present at spawn and not used to feign in combat or similar.
                    if (target->GetTypeId() == TYPEID_UNIT)
                        target->SetFeignDeath(apply, GetCasterGuid(), GetId(), false);

                    return;
                }
                case 37025: // Coilfang Water
                {
                    if (apply)
                    {
                        if (InstanceData* pInst = target->GetInstanceData())
                        {
                            Player* playerTarget = (Player*)target;
                            if (pInst->CheckConditionCriteriaMeet(playerTarget, INSTANCE_CONDITION_ID_LURKER, nullptr, CONDITION_FROM_HARDCODED))
                            {
                                if (pInst->CheckConditionCriteriaMeet(playerTarget, INSTANCE_CONDITION_ID_SCALDING_WATER, nullptr, CONDITION_FROM_HARDCODED))
                                    playerTarget->CastSpell(playerTarget, 37284, TRIGGERED_OLD_TRIGGERED);
                                else
                                {
                                    m_isPeriodic = true;
                                    m_modifier.periodictime = 2 * IN_MILLISECONDS; // Summons Coilfang Frenzy
                                    m_periodicTimer = 0;
                                }
                            }
                        }
                        return;
                    }
                    else
                        target->RemoveAurasDueToSpell(37284);
                }
                case 37676:                             // Insidious Whisper
                {
                    if (target->GetTypeId() != TYPEID_PLAYER)
                        return;

                    if (apply)
                    {
                        target->CastSpell(target, 37735, TRIGGERED_OLD_TRIGGERED); // Summon Inner Demon

                        InstanceData* data = target->GetInstanceData();
                        if (data)
                        {
                            m_modifier.m_amount = target->GetInstanceData()->GetData(6);
                            target->GetInstanceData()->SetData(6, m_modifier.m_amount + 1);
                            m_modifier.m_amount += 1018;
                        }
                        else
                            m_modifier.m_amount = 1018;
                    }

                    ReputationRank faction_rank = ReputationRank(1); // value taken from sniff

                    Player* player = (Player*)target;

                    player->GetReputationMgr().ApplyForceReaction(m_modifier.m_amount, faction_rank, apply);
                    player->GetReputationMgr().SendForceReactions();

                    // stop fighting if at apply forced rank friendly or at remove real rank friendly
                    if ((apply && faction_rank >= REP_FRIENDLY) || (!apply && player->GetReputationRank(m_modifier.m_amount) >= REP_FRIENDLY))
                        player->StopAttackFaction(m_modifier.m_amount);

                    if (!apply)
                    {
                        if (m_removeMode == AURA_REMOVE_BY_EXPIRE) // MC player if inner demon was not killed
                        {
                            if (Unit* pCaster = GetCaster())
                            {
                                pCaster->CastSpell(target, 37749, TRIGGERED_OLD_TRIGGERED); // Consuming Madness
                                pCaster->getThreatManager().modifyThreatPercent(target, -100);
                            }
                        }
                    }
                    return;
                }
                case 37922:                                 // Clear Insidious Whisper
                {
                    // no clue why its a dummy aura
                    if (apply)
                    {
                        if (target->HasAura(37716) && target->GetTypeId() == TYPEID_UNIT)
                            static_cast<Creature*>(target)->ForcedDespawn();
                        else
                            target->RemoveAurasDueToSpell(37676);
                    }
                    return;
                }
                case 40133:                                 // Summon Fire Elemental
                {
                    Unit* caster = GetCaster();
                    if (!caster)
                        return;

                    Unit* owner = caster->GetOwner();
                    if (owner && owner->GetTypeId() == TYPEID_PLAYER)
                    {
                        if (apply)
                            owner->CastSpell(owner, 8985, TRIGGERED_OLD_TRIGGERED);
                        else
                            ((Player*)owner)->RemovePet(PET_SAVE_REAGENTS);
                    }
                    return;
                }
                case 40132:                                 // Summon Earth Elemental
                {
                    Unit* caster = GetCaster();
                    if (!caster)
                        return;

                    Unit* owner = caster->GetOwner();
                    if (owner && owner->GetTypeId() == TYPEID_PLAYER)
                    {
                        if (apply)
                            owner->CastSpell(owner, 19704, TRIGGERED_OLD_TRIGGERED);
                        else
                            ((Player*)owner)->RemovePet(PET_SAVE_REAGENTS);
                    }
                    return;
                }
                case 40214:                                 // Dragonmaw Illusion
                {
                    if (apply)
                    {
                        target->CastSpell(target, 40216, TRIGGERED_OLD_TRIGGERED);
                        target->CastSpell(target, 42016, TRIGGERED_OLD_TRIGGERED);
                    }
                    else
                    {
                        target->RemoveAurasDueToSpell(40216);
                        target->RemoveAurasDueToSpell(42016);
                    }
                    return;
                }
                case 42515:                                 // Jarl Beam
                {
                    // aura animate dead (fainted) state for the duration, but we need to animate the death itself (correct way below?)
                    if (Unit* pCaster = GetCaster())
                        pCaster->ApplyModFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH, apply);

                    // Beam to Zelfrax at remove
                    if (!apply)
                        target->CastSpell(target, 42517, TRIGGERED_OLD_TRIGGERED);
                    return;
                }
                case 42583:                                 // Claw Rage
                case 68987:                                 // Pursuit
                {
                    Unit* caster = GetCaster();
                    if (!caster || target->GetTypeId() != TYPEID_PLAYER)
                        return;

                    if (apply)
                        caster->FixateTarget(target);
                    else
                        caster->FixateTarget(nullptr);

                    return;
                }
                case 43874:                                 // Scourge Mur'gul Camp: Force Shield Arcane Purple x3
                    target->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PLAYER, apply);
                    if (apply)
                        target->addUnitState(UNIT_STAT_ROOT);
                    return;
                case 47178:                                 // Plague Effect Self
                    target->SetFeared(apply, GetCasterGuid(), GetId());
                    return;
                case 50053:                                 // Centrifuge Shield
                    target->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PLAYER, apply);
                    target->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC, apply);
                    return;
                case 50241:                                 // Evasive Charges
                    target->ModifyAuraState(AURA_STATE_UNKNOWN22, apply);
                    return;
                case 56422:                                 // Nerubian Submerge
                case 70733:                                 // Stoneform
                    // not known if there are other things todo, only flag are confirmed valid
                    target->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE, apply);
                    return;
                case 58204:                                 // LK Intro VO (1)
                    if (target->GetTypeId() == TYPEID_PLAYER)
                    {
                        // Play part 1
                        if (apply)
                            target->PlayDirectSound(14970, PlayPacketParameters(PLAY_TARGET, (Player*)target));
                        // continue in 58205
                        else
                            target->CastSpell(target, 58205, TRIGGERED_OLD_TRIGGERED);
                    }
                    return;
                case 58205:                                 // LK Intro VO (2)
                    if (target->GetTypeId() == TYPEID_PLAYER)
                    {
                        // Play part 2
                        if (apply)
                            target->PlayDirectSound(14971, PlayPacketParameters(PLAY_TARGET, (Player*)target));
                        // Play part 3
                        else
                            target->PlayDirectSound(14972, PlayPacketParameters(PLAY_TARGET, (Player*)target));
                    }
                    return;
                case 27978:
                case 40131:
                    if (apply)
                        target->m_AuraFlags |= UNIT_AURAFLAG_ALIVE_INVISIBLE;
                    else
                        target->m_AuraFlags &= ~UNIT_AURAFLAG_ALIVE_INVISIBLE;
                    return;
                case 66936:                                     // Submerge
                case 66948:                                     // Submerge
                    if (apply)
                        target->CastSpell(target, 66969, TRIGGERED_OLD_TRIGGERED);
                    else
                        target->RemoveAurasDueToSpell(66969);
                    return;
            }
            break;
        }
        case SPELLFAMILY_MAGE:
            break;
        case SPELLFAMILY_WARLOCK:
        {
            // Haunt
            if (GetSpellProto()->SpellIconID == 3172 && (GetSpellProto()->SpellFamilyFlags & uint64(0x0004000000000000)))
            {
                // NOTE: for avoid use additional field damage stored in dummy value (replace unused 100%
                if (apply)
                    m_modifier.m_amount = 0;                // use value as damage counter instead redundant 100% percent
                else
                {
                    int32 bp0 = m_modifier.m_amount;

                    if (Unit* caster = GetCaster())
                        target->CastCustomSpell(caster, 48210, &bp0, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                }
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            switch (GetId())
            {
                case 52610:                                 // Savage Roar
                {
                    if (apply)
                    {
                        if (target->GetShapeshiftForm() != FORM_CAT)
                            return;

                        target->CastSpell(target, 62071, TRIGGERED_OLD_TRIGGERED);
                    }
                    else
                        target->RemoveAurasDueToSpell(62071);
                    return;
                }
                case 61336:                                 // Survival Instincts
                {
                    if (apply)
                    {
                        if (!target->IsInFeralForm())
                            return;

                        int32 bp0 = int32(target->GetMaxHealth() * m_modifier.m_amount / 100);
                        target->CastCustomSpell(target, 50322, &bp0, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
                    }
                    else
                        target->RemoveAurasDueToSpell(50322);
                    return;
                }
            }
            // Lifebloom
            if (GetSpellProto()->SpellFamilyFlags & uint64(0x1000000000))
            {
                if (apply)
                {
                    if (Unit* caster = GetCaster())
                    {
                        // prevent double apply bonuses
                        if (target->GetTypeId() != TYPEID_PLAYER || !((Player*)target)->GetSession()->PlayerLoading())
                        {
                            m_modifier.m_amount = caster->SpellHealingBonusDone(target, GetSpellProto(), m_modifier.m_amount, SPELL_DIRECT_DAMAGE);
                            m_modifier.m_amount = target->SpellHealingBonusTaken(caster, GetSpellProto(), m_modifier.m_amount, SPELL_DIRECT_DAMAGE);
                        }
                    }
                }
                else
                {
                    // Final heal on duration end
                    if (m_removeMode != AURA_REMOVE_BY_EXPIRE && m_removeMode != AURA_REMOVE_BY_DISPEL)
                        return;

                    // final heal
                    if (target->IsInWorld())
                    {
                        int32 amount = m_modifier.m_amount;
                        target->CastCustomSpell(nullptr, 33778, &amount, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this, GetCasterGuid());

                        if (Unit* caster = GetCaster())
                        {
                            int32 returnmana = (GetSpellProto()->ManaCostPercentage * caster->GetCreateMana() / 100) * GetStackAmount() / 2;
                            caster->CastCustomSpell(caster, 64372, &returnmana, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this, GetCasterGuid());
                        }
                    }
                }
                return;
            }

            // Predatory Strikes
            if (target->GetTypeId() == TYPEID_PLAYER && GetSpellProto()->SpellIconID == 1563)
            {
                ((Player*)target)->UpdateAttackPowerAndDamage();
                return;
            }

            // Improved Moonkin Form
            if (GetSpellProto()->SpellIconID == 2855)
            {
                uint32 spell_id;
                switch (GetId())
                {
                    case 48384: spell_id = 50170; break;    // Rank 1
                    case 48395: spell_id = 50171; break;    // Rank 2
                    case 48396: spell_id = 50172; break;    // Rank 3
                    default:
                        sLog.outError("HandleAuraDummy: Not handled rank of IMF (Spell: %u)", GetId());
                        return;
                }

                if (apply)
                {
                    if (target->GetShapeshiftForm() != FORM_MOONKIN)
                        return;

                    target->CastSpell(target, spell_id, TRIGGERED_OLD_TRIGGERED);
                }
                else
                    target->RemoveAurasDueToSpell(spell_id);
                return;
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
            switch (GetId())
            {
                case 57934:                                 // Tricks of the Trade, main spell
                {
                    if (apply)
                        GetHolder()->SetAuraCharges(1);     // not have proper charges set in spell data
                    else
                    {
                        // used for direct in code aura removes and spell proc event charges expire
                        if (m_removeMode != AURA_REMOVE_BY_DEFAULT)
                            target->getHostileRefManager().ResetThreatRedirection();
                    }
                    return;
                }
            }
            break;
        case SPELLFAMILY_HUNTER:
        {
            switch (GetId())
            {
                case 34477:                                 // Misdirection, main spell
                {
                    if (apply)
                        GetHolder()->SetAuraCharges(1);     // not have proper charges set in spell data
                    else
                    {
                        // used for direct in code aura removes and spell proc event charges expire
                        if (m_removeMode != AURA_REMOVE_BY_DEFAULT)
                        {
                            if (Unit* misdirectTarget = target->getHostileRefManager().GetThreatRedirectionTarget())
                                misdirectTarget->RemoveAurasDueToSpell(35079);
                            target->getHostileRefManager().ResetThreatRedirection();
                        }
                    }
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
            switch (GetId())
            {
                case 20911:                                 // Blessing of Sanctuary
                case 25899:                                 // Greater Blessing of Sanctuary
                {
                    if (apply)
                        target->CastSpell(target, 67480, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    else
                        target->RemoveAurasDueToSpell(67480);
                    return;
                }
            }
            break;
        case SPELLFAMILY_SHAMAN:
        {
            switch (GetId())
            {
                case 6495:                                  // Sentry Totem
                {
                    if (target->GetTypeId() != TYPEID_PLAYER)
                        return;

                    Totem* totem = target->GetTotem(TOTEM_SLOT_AIR);

                    if (totem && apply)
                        ((Player*)target)->GetCamera().SetView(totem);
                    else
                        ((Player*)target)->GetCamera().ResetView();

                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_DEATHKNIGHT:
            switch (GetId())
            {
                // Raise ally
                case 46619:
                {
                    // at this point the ghoul is already spawned
                    Unit* caster = GetCaster();
                    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                        return;

                    Player* player = static_cast<Player*>(caster);

                    if (apply)
                    {
                        player->SetGhouled(true);
                    }
                    else
                    {
                        player->SetGhouled(false);

                        // this will reset death timer in the client
                        WorldPacket data(SMSG_FORCED_DEATH_UPDATE);
                        player->GetSession()->SendPacket(data);
                        player->ResetDeathTimer();
                    }
                }
            }
            break;
        case SPELLFAMILY_PRIEST:
        {
            switch (GetId())
            {
                case 36414: // Focused Bursts
                {
                    if (apply)
                        target->clearUnitState(UNIT_STAT_MELEE_ATTACKING);
                    else
                        target->addUnitState(UNIT_STAT_MELEE_ATTACKING);
                    return;
                }
            }
            break;
        }
    }

    // pet auras
    if (PetAura const* petSpell = sSpellMgr.GetPetAura(GetId(), m_effIndex))
    {
        if (apply)
            target->AddPetAura(petSpell);
        else
            target->RemovePetAura(petSpell);
        return;
    }

    if (target->IsBoarded() && target->GetTransportInfo()->IsOnVehicle())
    {
        if (IsSpellHaveAura(GetSpellProto(), SPELL_AURA_CONTROL_VEHICLE))
        {
            // TODO maybe move GetVehicleInfo() to WorldObject class
            auto vehicle = static_cast<Unit*>(target->GetTransportInfo()->GetTransport());
            auto vehicleInfo = vehicle->GetVehicleInfo();

            if (!apply)
            {
                //sLog.outString("Unboarding %s %s from %s %s", target->GetName(), target->GetGuidStr().c_str(), vehicle->GetName(), vehicle->GetGuidStr().c_str());
                vehicleInfo->UnBoard(target, false);
            }
        }
    }

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAuraMapBounds(GetId());
        if (saBounds.first != saBounds.second)
        {
            uint32 zone, area;
            target->GetZoneAndAreaId(zone, area);

            for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
                itr->second->ApplyOrRemoveSpellIfCan((Player*)target, zone, area, false);
        }
    }

    // script has to "handle with care", only use where data are not ok to use in the above code.
    if (target->GetTypeId() == TYPEID_UNIT)
        sScriptDevAIMgr.OnAuraDummy(this, apply);
}

void Aura::HandleAuraMounted(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (apply)
    {
        CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
        if (!ci)
        {
            sLog.outErrorDb("AuraMounted: `creature_template`='%u' not found in database (only need it modelid)", m_modifier.m_miscvalue);
            return;
        }

        uint32 display_id = Creature::ChooseDisplayId(ci);
        CreatureModelInfo const* minfo = sObjectMgr.GetCreatureModelRandomGender(display_id);
        if (minfo)
            display_id = minfo->modelid;

        target->Mount(display_id, GetId());

        if (ci->VehicleTemplateId)
        {
            target->SetVehicleId(ci->VehicleTemplateId, ci->Entry);

            if (target->GetTypeId() == TYPEID_PLAYER)
                target->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PLAYER_VEHICLE);
        }
    }
    else
    {
        target->Unmount(true);

        CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
        if (ci && target->IsVehicle() && ci->VehicleTemplateId == target->GetVehicleInfo()->GetVehicleEntry()->m_ID)
        {
            if (target->GetTypeId() == TYPEID_PLAYER)
                target->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PLAYER_VEHICLE);

            target->SetVehicleId(0, 0);
        }
    }
}

void Aura::HandleAuraWaterWalk(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    GetTarget()->SetWaterWalk(apply);
}

void Aura::HandleAuraFeatherFall(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    GetTarget()->SetFeatherFall(apply);
}

void Aura::HandleAuraHover(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    GetTarget()->SetHover(apply);
}

void Aura::HandleWaterBreathing(bool /*apply*/, bool /*Real*/)
{
    // update timers in client
    if (GetTarget()->GetTypeId() == TYPEID_PLAYER)
        ((Player*)GetTarget())->UpdateMirrorTimers();
}

void Aura::HandleAuraModShapeshift(bool apply, bool Real)
{
    if (!Real)
        return;

    ShapeshiftForm form = ShapeshiftForm(m_modifier.m_miscvalue);

    SpellShapeshiftFormEntry const* ssEntry = sSpellShapeshiftFormStore.LookupEntry(form);
    if (!ssEntry)
    {
        sLog.outError("Unknown shapeshift form %u in spell %u", form, GetId());
        return;
    }

    Unit* target = GetTarget();

    // remove SPELL_AURA_EMPATHY
    target->RemoveSpellsCausingAura(SPELL_AURA_EMPATHY);

    if (ssEntry->modelID_A)
    {
        // i will asume that creatures will always take the defined model from the dbc
        // since no field in creature_templates describes wether an alliance or
        // horde modelid should be used at shapeshifting
        if (target->GetTypeId() != TYPEID_PLAYER)
            m_modifier.m_amount = ssEntry->modelID_A;
        else
        {
            // players are a bit different since the dbc has seldomly an horde modelid
            if (Player::TeamForRace(target->getRace()) == HORDE)
            {
                if (ssEntry->modelID_H)
                    m_modifier.m_amount = ssEntry->modelID_H;           // 3.2.3 only the moonkin form has this information
                else                                        // get model for race
                    m_modifier.m_amount = sObjectMgr.GetModelForRace(ssEntry->modelID_A, target->getRaceMask());
            }

            // nothing found in above, so use default
            if (!m_modifier.m_amount)
                m_modifier.m_amount = ssEntry->modelID_A;
        }
    }

    // remove polymorph before changing display id to keep new display id
    switch (form)
    {
        case FORM_CAT:
        case FORM_TREE:
        case FORM_TRAVEL:
        case FORM_AQUA:
        case FORM_BEAR:
        case FORM_DIREBEAR:
        case FORM_FLIGHT_EPIC:
        case FORM_FLIGHT:
        case FORM_MOONKIN:
        {
            // remove movement affects
            target->RemoveSpellsCausingAura(SPELL_AURA_MOD_ROOT, GetHolder(), true);
            Unit::AuraList const& slowingAuras = target->GetAurasByType(SPELL_AURA_MOD_DECREASE_SPEED);
            for (Unit::AuraList::const_iterator iter = slowingAuras.begin(); iter != slowingAuras.end();)
            {
                SpellEntry const* aurSpellInfo = (*iter)->GetSpellProto();

                uint32 aurMechMask = GetAllSpellMechanicMask(aurSpellInfo);

                // If spell that caused this aura has Croud Control or Daze effect
                if ((aurMechMask & MECHANIC_NOT_REMOVED_BY_SHAPESHIFT) ||
                        // some Daze spells have these parameters instead of MECHANIC_DAZE (skip snare spells)
                        (aurSpellInfo->SpellIconID == 15 && aurSpellInfo->Dispel == 0 &&
                         (aurMechMask & (1 << (MECHANIC_SNARE - 1))) == 0))
                {
                    ++iter;
                    continue;
                }

                // All OK, remove aura now
                target->RemoveAurasDueToSpellByCancel(aurSpellInfo->Id);
                iter = slowingAuras.begin();
            }

            target->RemoveAurasDueToSpell(16591); // Patch 2.0.1 - Shapeshifting removes Noggenfogger elixir
            //no break here
            break;
        }
        default:
            break;
    }

    if (apply)
    {
        Powers PowerType = POWER_MANA;

        // remove other shapeshift before applying a new one
        target->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT, GetHolder());

        if (m_modifier.m_amount > 0)
            target->SetDisplayId(m_modifier.m_amount);

        // now only powertype must be set
        switch (form)
        {
            case FORM_CAT:
                PowerType = POWER_ENERGY;
                break;
            case FORM_BEAR:
            case FORM_DIREBEAR:
            case FORM_BATTLESTANCE:
            case FORM_BERSERKERSTANCE:
            case FORM_DEFENSIVESTANCE:
                PowerType = POWER_RAGE;
                break;
            default:
                break;
        }

        if (PowerType != POWER_MANA)
        {
            // reset power to default values only at power change
            if (target->GetPowerType() != PowerType)
                target->SetPowerType(PowerType);

            switch (form)
            {
                case FORM_CAT: // need to cast Track Humanoids if no other tracking is on
                    if (target->HasSpell(5225) && !target->HasAura(2383) && !target->HasAura(2580))
                        target->CastSpell(nullptr, 5225, TRIGGERED_OLD_TRIGGERED);
                    // no break
                case FORM_BEAR:
                case FORM_DIREBEAR:
                {
                    // get furor proc chance
                    int32 furorChance = 0;
                    Unit::AuraList const& mDummy = target->GetAurasByType(SPELL_AURA_DUMMY);
                    for (auto i : mDummy)
                    {
                        if (i->GetSpellProto()->SpellIconID == 238)
                        {
                            furorChance = i->GetModifier()->m_amount;
                            break;
                        }
                    }

                    if (m_modifier.m_miscvalue == FORM_CAT)
                    {
                        // Furor chance is now amount allowed to save energy for cat form
                        // without talent it reset to 0
                        if ((int32)target->GetPower(POWER_ENERGY) > furorChance)
                        {
                            target->SetPower(POWER_ENERGY, 0);
                            target->CastCustomSpell(target, 17099, &furorChance, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        }
                    }
                    else if (furorChance)                   // only if talent known
                    {
                        target->SetPower(POWER_RAGE, 0);
                        if (irand(1, 100) <= furorChance)
                            target->CastSpell(target, 17057, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    }
                    break;
                }
                case FORM_BATTLESTANCE:
                case FORM_DEFENSIVESTANCE:
                case FORM_BERSERKERSTANCE:
                {
                    ShapeshiftForm previousForm = target->GetShapeshiftForm();
                    uint32 ragePercent = 0;
                    if (previousForm == FORM_DEFENSIVESTANCE)
                    {
                        Unit::AuraList const& auraClassScripts = target->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                        for (Unit::AuraList::const_iterator itr = auraClassScripts.begin(); itr != auraClassScripts.end();)
                        {
                            if ((*itr)->GetModifier()->m_miscvalue == 831)
                            {
                                ragePercent = (*itr)->GetModifier()->m_amount;
                            }
                            else
                                ++itr;
                        }
                    }
                    uint32 Rage_val = 0;
                    // Stance mastery + Tactical mastery (both passive, and last have aura only in defense stance, but need apply at any stance switch)
                    if (target->GetTypeId() == TYPEID_PLAYER)
                    {
                        PlayerSpellMap const& sp_list = ((Player*)target)->GetSpellMap();
                        for (const auto& itr : sp_list)
                        {
                            if (itr.second.state == PLAYERSPELL_REMOVED)
                                continue;

                            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(itr.first);
                            if (spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_WARRIOR && spellInfo->SpellIconID == 139)
                                Rage_val += target->CalculateSpellDamage(target, spellInfo, EFFECT_INDEX_0) * 10;
                        }
                    }

                    if (ragePercent) // not zero
                    {
                        if (ragePercent != 100) // optimization
                            target->SetPower(POWER_RAGE, (target->GetPower(POWER_RAGE) * ragePercent) / 100);
                    }
                    else if (target->GetPower(POWER_RAGE) > Rage_val)
                        target->SetPower(POWER_RAGE, Rage_val);
                    break;
                }
                default:
                    break;
            }
        }

        target->SetShapeshiftForm(form);

        // a form can give the player a new castbar with some spells.. this is a clientside process..
        // serverside just needs to register the new spells so that player isn't kicked as cheater
        if (target->GetTypeId() == TYPEID_PLAYER)
            for (unsigned int i : ssEntry->spellId)
                if (i)
                    ((Player*)target)->addSpell(i, true, false, false, false);
    }
    else
    {
        target->RestoreDisplayId();

        if (target->getClass() == CLASS_DRUID)
            target->SetPowerType(POWER_MANA);

        target->SetShapeshiftForm(FORM_NONE);

        switch (form)
        {
            // Nordrassil Harness - bonus
            case FORM_BEAR:
            case FORM_DIREBEAR:
            case FORM_CAT:
                if (Aura* dummy = target->GetDummyAura(37315))
                    target->CastSpell(target, 37316, TRIGGERED_OLD_TRIGGERED, nullptr, dummy);
                break;
            // Nordrassil Regalia - bonus
            case FORM_MOONKIN:
                if (Aura* dummy = target->GetDummyAura(37324))
                    target->CastSpell(target, 37325, TRIGGERED_OLD_TRIGGERED, nullptr, dummy);
                break;
            default:
                break;
        }

        // look at the comment in apply-part
        if (target->GetTypeId() == TYPEID_PLAYER)
            for (unsigned int i : ssEntry->spellId)
                if (i)
                    ((Player*)target)->removeSpell(i, false, false, false);
    }

    // adding/removing linked auras
    // add/remove the shapeshift aura's boosts
    HandleShapeshiftBoosts(apply);

    if (target->GetTypeId() == TYPEID_PLAYER)
        ((Player*)target)->InitDataForForm();
}

void Aura::HandleAuraTransform(bool apply, bool Real)
{
    Unit* target = GetTarget();
    if (apply)
    {
        // special case (spell specific functionality)
        if (m_modifier.m_miscvalue == 0)
        {
            switch (GetId())
            {
                case 16739:                                 // Orb of Deception
                {
                    uint32 orb_model = target->GetNativeDisplayId();
                    switch (orb_model)
                    {
                        // Troll Female
                        case 1479: m_modifier.m_amount = 10134; break;
                        // Troll Male
                        case 1478: m_modifier.m_amount = 10135; break;
                        // Tauren Male
                        case 59:   m_modifier.m_amount = 10136; break;
                        // Human Male
                        case 49:   m_modifier.m_amount = 10137; break;
                        // Human Female
                        case 50:   m_modifier.m_amount = 10138; break;
                        // Orc Male
                        case 51:   m_modifier.m_amount = 10139; break;
                        // Orc Female
                        case 52:   m_modifier.m_amount = 10140; break;
                        // Dwarf Male
                        case 53:   m_modifier.m_amount = 10141; break;
                        // Dwarf Female
                        case 54:   m_modifier.m_amount = 10142; break;
                        // NightElf Male
                        case 55:   m_modifier.m_amount = 10143; break;
                        // NightElf Female
                        case 56:   m_modifier.m_amount = 10144; break;
                        // Undead Female
                        case 58:   m_modifier.m_amount = 10145; break;
                        // Undead Male
                        case 57:   m_modifier.m_amount = 10146; break;
                        // Tauren Female
                        case 60:   m_modifier.m_amount = 10147; break;
                        // Gnome Male
                        case 1563: m_modifier.m_amount = 10148; break;
                        // Gnome Female
                        case 1564: m_modifier.m_amount = 10149; break;
                        // BloodElf Female
                        case 15475: m_modifier.m_amount = 17830; break;
                        // BloodElf Male
                        case 15476: m_modifier.m_amount = 17829; break;
                        // Dranei Female
                        case 16126: m_modifier.m_amount = 17828; break;
                        // Dranei Male
                        case 16125: m_modifier.m_amount = 17827; break;
                        default: break;
                    }
                    break;
                }
                case 42365:                                 // Murloc costume
                    m_modifier.m_amount = 21723;
                    break;
                // case 44186:                          // Gossip NPC Appearance - All, Brewfest
                // break;
                // case 48305:                          // Gossip NPC Appearance - All, Spirit of Competition
                // break;
                case 50517:                                 // Dread Corsair
                case 51926:                                 // Corsair Costume
                {
                    // expected for players
                    uint32 race = target->getRace();

                    switch (race)
                    {
                        case RACE_HUMAN:
                            m_modifier.m_amount = target->getGender() == GENDER_MALE ? 25037 : 25048;
                            break;
                        case RACE_ORC:
                            m_modifier.m_amount = target->getGender() == GENDER_MALE ? 25039 : 25050;
                            break;
                        case RACE_DWARF:
                            m_modifier.m_amount = target->getGender() == GENDER_MALE ? 25034 : 25045;
                            break;
                        case RACE_NIGHTELF:
                            m_modifier.m_amount = target->getGender() == GENDER_MALE ? 25038 : 25049;
                            break;
                        case RACE_UNDEAD:
                            m_modifier.m_amount = target->getGender() == GENDER_MALE ? 25042 : 25053;
                            break;
                        case RACE_TAUREN:
                            m_modifier.m_amount = target->getGender() == GENDER_MALE ? 25040 : 25051;
                            break;
                        case RACE_GNOME:
                            m_modifier.m_amount = target->getGender() == GENDER_MALE ? 25035 : 25046;
                            break;
                        case RACE_TROLL:
                            m_modifier.m_amount = target->getGender() == GENDER_MALE ? 25041 : 25052;
                            break;
                        case RACE_GOBLIN:                   // not really player race (3.x), but model exist
                            m_modifier.m_amount = target->getGender() == GENDER_MALE ? 25036 : 25047;
                            break;
                        case RACE_BLOODELF:
                            m_modifier.m_amount = target->getGender() == GENDER_MALE ? 25032 : 25043;
                            break;
                        case RACE_DRAENEI:
                            m_modifier.m_amount = target->getGender() == GENDER_MALE ? 25033 : 25044;
                            break;
                    }
                    break;
                }
                // case 50531:                              // Gossip NPC Appearance - All, Pirate Day
                // break;
                // case 51010:                              // Dire Brew
                // break;
                // case 53806:                              // Pygmy Oil
                // break;
                // case 62847:                              // NPC Appearance - Valiant 02
                // break;
                // case 62852:                              // NPC Appearance - Champion 01
                // break;
                // case 63965:                              // NPC Appearance - Champion 02
                // break;
                // case 63966:                              // NPC Appearance - Valiant 03
                // break;
                case 65386:                                 // Honor the Dead
                case 65495:
                {
                    switch (target->getGender())
                    {
                        case GENDER_MALE:
                            m_modifier.m_amount = 29203;    // Chapman
                            break;
                        case GENDER_FEMALE:
                        case GENDER_NONE:
                            m_modifier.m_amount = 29204;    // Catrina
                            break;
                    }
                    break;
                }
                // case 65511:                              // Gossip NPC Appearance - Brewfest
                // break;
                // case 65522:                              // Gossip NPC Appearance - Winter Veil
                // break;
                // case 65523:                              // Gossip NPC Appearance - Default
                // break;
                // case 65524:                              // Gossip NPC Appearance - Lunar Festival
                // break;
                // case 65525:                              // Gossip NPC Appearance - Hallow's End
                // break;
                // case 65526:                              // Gossip NPC Appearance - Midsummer
                // break;
                // case 65527:                              // Gossip NPC Appearance - Spirit of Competition
                // break;
                case 65528:                                 // Gossip NPC Appearance - Pirates' Day
                {
                    // expecting npc's using this spell to have models with race info.
                    // random gender, regardless of current gender
                    switch (target->getRace())
                    {
                        case RACE_HUMAN:
                            m_modifier.m_amount = roll_chance_i(50) ? 25037 : 25048;
                            break;
                        case RACE_ORC:
                            m_modifier.m_amount = roll_chance_i(50) ? 25039 : 25050;
                            break;
                        case RACE_DWARF:
                            m_modifier.m_amount = roll_chance_i(50) ? 25034 : 25045;
                            break;
                        case RACE_NIGHTELF:
                            m_modifier.m_amount = roll_chance_i(50) ? 25038 : 25049;
                            break;
                        case RACE_UNDEAD:
                            m_modifier.m_amount = roll_chance_i(50) ? 25042 : 25053;
                            break;
                        case RACE_TAUREN:
                            m_modifier.m_amount = roll_chance_i(50) ? 25040 : 25051;
                            break;
                        case RACE_GNOME:
                            m_modifier.m_amount = roll_chance_i(50) ? 25035 : 25046;
                            break;
                        case RACE_TROLL:
                            m_modifier.m_amount = roll_chance_i(50) ? 25041 : 25052;
                            break;
                        case RACE_GOBLIN:
                            m_modifier.m_amount = roll_chance_i(50) ? 25036 : 25047;
                            break;
                        case RACE_BLOODELF:
                            m_modifier.m_amount = roll_chance_i(50) ? 25032 : 25043;
                            break;
                        case RACE_DRAENEI:
                            m_modifier.m_amount = roll_chance_i(50) ? 25033 : 25044;
                            break;
                    }

                    break;
                }
                case 65529:                                 // Gossip NPC Appearance - Day of the Dead (DotD)
                    // random, regardless of current gender
                    m_modifier.m_amount = roll_chance_i(50) ? 29203 : 29204;
                    break;
                // case 66236:                          // Incinerate Flesh
                // break;
                // case 69999:                          // [DND] Swap IDs
                // break;
                // case 70764:                          // Citizen Costume (note: many spells w/same name)
                // break;
                // case 71309:                          // [DND] Spawn Portal
                // break;
                case 71450:                                 // Crown Parcel Service Uniform
                    m_modifier.m_amount = target->getGender() == GENDER_MALE ? 31002 : 31003;
                    break;
                // case 75531:                          // Gnomeregan Pride
                // break;
                // case 75532:                          // Darkspear Pride
                // break;
                default:
                    sLog.outError("Aura::HandleAuraTransform, spell %u does not have creature entry defined, need custom defined model.", GetId());
                    break;
            }
        }
        else                                                // m_modifier.m_amount != 0
        {
            CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
            if (!cInfo)
            {
                m_modifier.m_amount = 16358;                           // pig pink ^_^
                sLog.outError("Auras: unknown creature id = %d (only need its modelid) Form Spell Aura Transform in Spell ID = %d", m_modifier.m_amount, GetId());
            }
            else
                m_modifier.m_amount = Creature::ChooseDisplayId(cInfo);   // Will use the default model here

            // Polymorph (sheep/penguin case)
            if (GetSpellProto()->SpellFamilyName == SPELLFAMILY_MAGE && GetSpellProto()->SpellIconID == 82)
                if (Unit* caster = GetCaster())
                    if (caster->HasAura(52648))             // Glyph of the Penguin
                        m_modifier.m_amount = 26452;

            // creature case, need to update equipment if additional provided
            if (cInfo && target->GetTypeId() == TYPEID_UNIT)
                ((Creature*)target)->LoadEquipment(cInfo->EquipmentTemplateId, false);
        }

        target->SetDisplayId(m_modifier.m_amount);

        // Dragonmaw Illusion (set mount model also)
        if (GetId() == 42016 && target->GetMountID() && !target->GetAurasByType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED).empty())
            target->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 16314);

        // polymorph case
        if (Real && target->GetTypeId() == TYPEID_PLAYER && target->IsPolymorphed())
        {
            // for players, start regeneration after 1s (in polymorph fast regeneration case)
            // only if caster is Player (after patch 2.4.2)
            if (GetCasterGuid().IsPlayer())
                ((Player*)target)->setRegenTimer(1 * IN_MILLISECONDS);

            // dismount polymorphed target (after patch 2.4.2)
            if (target->IsMounted())
                target->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED, GetHolder());
        }
    }
    else                                                    // !apply
    {
        // ApplyModifier(true) will reapply it if need
        target->RestoreDisplayId();

        // apply default equipment for creature case
        if (target->GetTypeId() == TYPEID_UNIT)
            ((Creature*)target)->LoadEquipment(((Creature*)target)->GetCreatureInfo()->EquipmentTemplateId, true);

        // re-apply some from still active with preference negative cases
        Unit::AuraList const& otherTransforms = target->GetAurasByType(SPELL_AURA_TRANSFORM);
        if (!otherTransforms.empty())
        {
            // look for other transform auras
            Aura* handledAura = *otherTransforms.begin();
            for (auto otherTransform : otherTransforms)
            {
                // negative auras are preferred
                if (!otherTransform->IsPositive())
                {
                    handledAura = otherTransform;
                    break;
                }
            }
            handledAura->ApplyModifier(true);
        }

        // Dragonmaw Illusion (restore mount model)
        if (GetId() == 42016 && target->GetMountID() == 16314)
        {
            if (!target->GetAurasByType(SPELL_AURA_MOUNTED).empty())
            {
                uint32 cr_id = target->GetAurasByType(SPELL_AURA_MOUNTED).front()->GetModifier()->m_miscvalue;
                if (CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(cr_id))
                {
                    uint32 display_id = Creature::ChooseDisplayId(ci);
                    CreatureModelInfo const* minfo = sObjectMgr.GetCreatureModelRandomGender(display_id);
                    if (minfo)
                        display_id = minfo->modelid;

                    target->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, display_id);
                }
            }
        }
    }
}

void Aura::HandleForceReaction(bool apply, bool Real)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    if (!Real)
        return;

    Player* player = (Player*)GetTarget();

    uint32 faction_id = m_modifier.m_miscvalue;
    ReputationRank faction_rank = ReputationRank(m_modifier.m_amount);

    player->GetReputationMgr().ApplyForceReaction(faction_id, faction_rank, apply);
    player->GetReputationMgr().SendForceReactions();

    // stop fighting if at apply forced rank friendly or at remove real rank friendly
    if ((apply && faction_rank >= REP_FRIENDLY) || (!apply && player->GetReputationRank(faction_id) >= REP_FRIENDLY))
        player->StopAttackFaction(faction_id);

    //TODO: hack alert! Need to remove that when its possible
    if (!apply)
        if (GetId() == 32756) // Shadowy disguise
            player->RemoveAurasDueToSpell(player->getGender() == GENDER_MALE ? 38080 : 38081);
}

void Aura::HandleAuraModSkill(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* target = static_cast<Player*>(GetTarget());

    Modifier const* mod = GetModifier();
    const uint16 skillId = uint16(GetSpellProto()->EffectMiscValue[m_effIndex]);
    const int16 amount = int16(mod->m_amount);
    const bool permanent = (mod->m_auraname == SPELL_AURA_MOD_SKILL_TALENT);

    target->ModifySkillBonus(skillId, (apply ? amount : -amount), permanent);
}

void Aura::HandleChannelDeathItem(bool apply, bool Real)
{
    if (Real && !apply)
    {
        if (m_removeMode != AURA_REMOVE_BY_DEATH)
            return;
        // Item amount
        if (m_modifier.m_amount <= 0)
            return;

        SpellEntry const* spellInfo = GetSpellProto();
        if (spellInfo->EffectItemType[m_effIndex] == 0)
            return;

        Unit* victim = GetTarget();
        Unit* caster = GetCaster();
        if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
            return;

        // Soul Shard (target req.)
        if (spellInfo->EffectItemType[m_effIndex] == 6265)
        {
            // Only from non-grey units
            if (!((Player*)caster)->isHonorOrXPTarget(victim))
                return;
            // Only if the creature is tapped by the player or his group
            if (victim->GetTypeId() == TYPEID_UNIT && !((Creature*)victim)->IsTappedBy((Player*)caster))
                return;
        }

        // Adding items
        uint32 noSpaceForCount = 0;
        uint32 count = m_modifier.m_amount;

        ItemPosCountVec dest;
        InventoryResult msg = ((Player*)caster)->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, spellInfo->EffectItemType[m_effIndex], count, &noSpaceForCount);
        if (msg != EQUIP_ERR_OK)
        {
            count -= noSpaceForCount;
            ((Player*)caster)->SendEquipError(msg, nullptr, nullptr, spellInfo->EffectItemType[m_effIndex]);
            if (count == 0)
                return;
        }

        Item* newitem = ((Player*)caster)->StoreNewItem(dest, spellInfo->EffectItemType[m_effIndex], true);
        ((Player*)caster)->SendNewItem(newitem, count, true, true);

        // Soul Shard (glyph bonus)
        if (spellInfo->EffectItemType[m_effIndex] == 6265)
        {
            // Glyph of Soul Shard
            if (caster->HasAura(58070) && roll_chance_i(40))
                caster->CastSpell(caster, 58068, TRIGGERED_OLD_TRIGGERED, nullptr, this);
        }
    }
}

void Aura::HandleBindSight(bool apply, bool /*Real*/)
{
    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Camera& camera = ((Player*)caster)->GetCamera();
    if (apply)
        camera.SetView(GetTarget());
    else
        camera.ResetView();
}

void Aura::HandleFarSight(bool apply, bool /*Real*/)
{
    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Camera& camera = ((Player*)caster)->GetCamera();
    if (apply)
        camera.SetView(GetTarget());
    else
        camera.ResetView();
}

void Aura::HandleAuraTrackCreatures(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    if (apply)
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());

    if (apply)
        GetTarget()->SetFlag(PLAYER_TRACK_CREATURES, uint32(1) << (m_modifier.m_miscvalue - 1));
    else
        GetTarget()->RemoveFlag(PLAYER_TRACK_CREATURES, uint32(1) << (m_modifier.m_miscvalue - 1));
}

void Aura::HandleAuraTrackResources(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    if (apply)
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());

    if (apply)
        GetTarget()->SetFlag(PLAYER_TRACK_RESOURCES, uint32(1) << (m_modifier.m_miscvalue - 1));
    else
        GetTarget()->RemoveFlag(PLAYER_TRACK_RESOURCES, uint32(1) << (m_modifier.m_miscvalue - 1));
}

void Aura::HandleAuraTrackStealthed(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    if (apply)
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());

    GetTarget()->ApplyModByteFlag(PLAYER_FIELD_BYTES, 0, PLAYER_FIELD_BYTE_TRACK_STEALTHED, apply);
}

void Aura::HandleAuraModScale(bool apply, bool /*Real*/)
{
    GetTarget()->ApplyPercentModFloatValue(OBJECT_FIELD_SCALE_X, float(m_modifier.m_amount), apply);
    GetTarget()->UpdateModelData();
}

void Aura::HandleModPossess(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    // not possess yourself
    if (GetCasterGuid() == target->GetObjectGuid())
        return;

    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER) // TODO:: well i know some bosses can take control of player???
        return;

    if (apply)
    {
        // Possess: advertised type of charm (unique) - remove existing advertised charm
        caster->BreakCharmOutgoing(true);

        caster->TakePossessOf(target);
    }
    else
    {
        caster->Uncharm(target);

        // clean dummy auras from caster : TODO check if its right in all case
        caster->RemoveAurasDueToSpell(GetId());
    }

    if (const SpellEntry* spellInfo = GetSpellProto())
    {
        switch (spellInfo->Id)
        {
            // Need to teleport to spawn position on possess end
            case 37868: // Arcano-Scorp Control
            case 37893:
            case 37895:
                if (!apply)
                {
                    float x, y, z, o;
                    Creature* creatureTarget = (Creature*)target;
                    creatureTarget->GetRespawnCoord(x, y, z, &o);
                    creatureTarget->NearTeleportTo(x, y, z, o);
                    caster->InterruptSpell(CURRENT_CHANNELED_SPELL);
                }
                break;
            case 37748: // Teron Gorefiend - remove aura from caster when posses is removed
                if (!apply)
                    caster->RemoveAurasDueToSpell(37748);
                break;
        }
    }
}

void Aura::HandleModPossessPet(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Unit* target = GetTarget();
    if (target->GetTypeId() != TYPEID_UNIT || !static_cast<Creature*>(target)->IsPet())
        return;

    if (apply)
    {
        // Possess pet: advertised type of charm (unique) - remove existing advertised charm
        caster->BreakCharmOutgoing(true);

        caster->TakePossessOf(target);
    }
    else
        caster->Uncharm(target);
}

void Aura::HandleAuraModPetTalentsPoints(bool /*Apply*/, bool Real)
{
    if (!Real)
        return;

    // Recalculate pet talent points
    if (Pet* pet = GetTarget()->GetPet())
        pet->InitTalentForLevel();
}

void Aura::HandleModCharm(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    // not charm yourself
    if (GetCasterGuid() == target->GetObjectGuid())
        return;

    Unit* caster = GetCaster();
    if (!caster)
        return;

    Player* playerCaster = caster->GetTypeId() == TYPEID_PLAYER ? static_cast<Player*>(caster) : nullptr;

    if (apply)
    {
        // Charm: normally advertised type of charm (unique), but with notable exceptions:
        // * Seems to be non-unique for NPCs - allows overwriting advertised charm by offloading existing one (e.g. Chromatic Mutation)
        // * Seems to be always unique for players - remove player's existing advertised charm (no evidence against this found yet)
        if (playerCaster)
            caster->BreakCharmOutgoing(true);

        caster->TakeCharmOf(target, GetId());
    }
    else
        caster->Uncharm(target, GetId());

    if (apply)
    {
        switch (GetId())
        {
            case 32830: // Possess - invisible
                caster->CastSpell(caster, 32832, TRIGGERED_OLD_TRIGGERED);
                break;
        }
    }
    else
    {
        switch (GetId())
        {
            case 32830: // Possess
                target->CastSpell(target, 13360, TRIGGERED_OLD_TRIGGERED);
                if (caster->GetTypeId() == TYPEID_UNIT)
                    static_cast<Creature*>(caster)->ForcedDespawn();
                break;
            case 34630: // Scrap Reaver X6000
                if (target->GetTypeId() == TYPEID_UNIT && target->AI())
                    target->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, target, (Creature*)target);
                break;
            case 33684:
                if (caster->GetTypeId() == TYPEID_UNIT)
                    static_cast<Creature*>(caster)->ForcedDespawn();
                break;
        }
    }
}


void Aura::HandleAoECharm(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    // not charm yourself
    if (GetCasterGuid() == target->GetObjectGuid())
        return;

    Unit* caster = GetCaster();
    if (!caster)
        return;

    if (apply)
        // AoE charm: non-advertised type of charm - co-exists with other charms
        caster->TakeCharmOf(target, GetId(), false);
    else
        caster->Uncharm(target, GetId());
}

void Aura::HandleModConfuse(bool apply, bool Real)
{
    if (!Real)
        return;

    // Do not remove it yet if more effects are up, do it for the last effect
    if (!apply && GetTarget()->HasAuraType(SPELL_AURA_MOD_CONFUSE))
        return;

    GetTarget()->SetConfused(apply, GetCasterGuid(), GetId(), m_removeMode);

    GetTarget()->getHostileRefManager().HandleSuppressed(apply);
}

void Aura::HandleModFear(bool apply, bool Real)
{
    if (!Real)
        return;

    // Do not remove it yet if more effects are up, do it for the last effect
    if (!apply && GetTarget()->HasAuraType(SPELL_AURA_MOD_FEAR))
        return;

    GetTarget()->SetFeared(apply, GetCasterGuid(), GetId());

    // 2.3.0 - fear no longer applies suppression - in case of uncomment, need to adjust IsSuppressedTarget
    // GetTarget()->getHostileRefManager().HandleSuppressed(apply);
}

void Aura::HandleFeignDeath(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    // Do not remove it yet if more effects are up, do it for the last effect
    if (!apply && target->HasAuraType(SPELL_AURA_FEIGN_DEATH))
        return;

    if (apply)
    {
        bool success = true;

        if (target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
        {
            // Players and player-controlled units do an additional success roll for this aura on application
            const SpellEntry* entry = GetSpellProto();
            const SpellSchoolMask schoolMask = GetSpellSchoolMask(entry);
            auto attackers = target->getAttackers();
            for (auto attacker : attackers)
            {
                if (attacker && !attacker->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
                {
                    if (target->MagicSpellHitResult(attacker, entry, schoolMask) != SPELL_MISS_NONE)
                    {
                        success = false;
                        break;
                    }
                }
            }
        }

        if (success)
            target->InterruptSpellsCastedOnMe();

        target->SetFeignDeath(apply, GetCasterGuid(), GetId(), true, success);
    }
    else
        target->SetFeignDeath(false);
}

void Aura::HandleAuraModDisarm(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (!apply && target->HasAuraType(GetModifier()->m_auraname))
        return;

    uint32 flags;
    uint32 field;
    WeaponAttackType attack_type;

    switch (GetModifier()->m_auraname)
    {
        default:
        case SPELL_AURA_MOD_DISARM:
        {
            field = UNIT_FIELD_FLAGS;
            flags = UNIT_FLAG_DISARMED;
            attack_type = BASE_ATTACK;
            break;
        }
        case SPELL_AURA_MOD_DISARM_OFFHAND:
        {
            field = UNIT_FIELD_FLAGS_2;
            flags = UNIT_FLAG2_DISARM_OFFHAND;
            attack_type = OFF_ATTACK;
            break;
        }
        case SPELL_AURA_MOD_DISARM_RANGED:
        {
            field = UNIT_FIELD_FLAGS_2;
            flags = UNIT_FLAG2_DISARM_RANGED;
            attack_type = RANGED_ATTACK;
            break;
        }
    }

    target->ApplyModFlag(field, flags, apply);

    // main-hand attack speed already set to special value for feral form already and don't must change and reset at remove.
    if (target->IsInFeralForm())
        return;

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        if (apply)
            target->SetAttackTime(attack_type, BASE_ATTACK_TIME);
        else
            ((Player*)target)->SetRegularAttackTime();
    }

    target->UpdateDamagePhysical(attack_type);
}

void Aura::HandleAuraModStun(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (apply)
    {
        // Frost stun aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
            target->ModifyAuraState(AURA_STATE_FROZEN, apply);

        Unit* caster = GetCaster();
        target->SetStunned(true, (caster ? caster->GetObjectGuid() : ObjectGuid()), GetSpellProto()->Id);

        if (caster)
            if (UnitAI* ai = caster->AI())
                ai->JustStunnedTarget(GetSpellProto(), target);

        if (GetSpellProto()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_DAMAGE)
            target->getHostileRefManager().HandleSuppressed(apply);

        // Summon the Naj'entus Spine GameObject on target if spell is Impaling Spine
        if (GetId() == 39837)
            target->CastSpell(nullptr, 39929, TRIGGERED_OLD_TRIGGERED);
    }
    else
    {
        // Frost stun aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
        {
            bool found_another = false;
            for (AuraType const* itr = &frozenAuraTypes[0]; *itr != SPELL_AURA_NONE; ++itr)
            {
                Unit::AuraList const& auras = target->GetAurasByType(*itr);
                for (auto aura : auras)
                {
                    if (GetSpellSchoolMask(aura->GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
                    {
                        found_another = true;
                        break;
                    }
                }
                if (found_another)
                    break;
            }

            if (!found_another)
                target->ModifyAuraState(AURA_STATE_FROZEN, apply);
        }

        if (GetSpellProto()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_DAMAGE)
            target->getHostileRefManager().HandleSuppressed(apply);

        // Real remove called after current aura remove from lists, check if other similar auras active
        if (target->HasAuraType(SPELL_AURA_MOD_STUN))
            return;

        target->SetStunned(false);

        // Wyvern Sting
        if (GetSpellProto()->SpellFamilyName == SPELLFAMILY_HUNTER && GetSpellProto()->SpellFamilyFlags & uint64(0x0000100000000000))
        {
            Unit* caster = GetCaster();
            if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spell_id;
            switch (GetId())
            {
                case 19386: spell_id = 24131; break;
                case 24132: spell_id = 24134; break;
                case 24133: spell_id = 24135; break;
                case 27068: spell_id = 27069; break;
                case 49011: spell_id = 49009; break;
                case 49012: spell_id = 49010; break;
                default:
                    sLog.outError("Spell selection called for unexpected original spell %u, new spell for this spell family?", GetId());
                    return;
            }

            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spell_id);

            if (!spellInfo)
                return;

            caster->CastSpell(target, spellInfo, TRIGGERED_OLD_TRIGGERED, nullptr, this);
        }
    }
}

void Aura::HandleModStealth(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (apply)
    {
        // drop flag at stealth in bg
        target->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);

        // only at real aura add
        if (Real)
        {
            target->SetStandFlags(UNIT_STAND_FLAGS_CREEP);

            if (target->GetTypeId() == TYPEID_PLAYER)
                target->SetByteFlag(PLAYER_FIELD_BYTES2, 3, PLAYER_FIELD_BYTE2_STEALTH);

            // apply only if not in GM invisibility (and overwrite invisibility state)
            if (target->GetVisibility() != VISIBILITY_OFF)
            {
                target->SetVisibility(VISIBILITY_GROUP_NO_DETECT);
                target->SetVisibility(VISIBILITY_GROUP_STEALTH);
            }

            // apply full stealth period bonuses only at first stealth aura in stack
            if (target->GetAurasByType(SPELL_AURA_MOD_STEALTH).size() <= 1)
            {
                Unit::AuraList const& mDummyAuras = target->GetAurasByType(SPELL_AURA_DUMMY);
                for (auto mDummyAura : mDummyAuras)
                {
                    // Master of Subtlety
                    if (mDummyAura->GetSpellProto()->SpellIconID == 2114)
                    {
                        target->RemoveAurasDueToSpell(31666);
                        int32 bp = mDummyAura->GetModifier()->m_amount;
                        target->CastCustomSpell(target, 31665, &bp, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
                    }
                    // Overkill
                    else if (mDummyAura->GetId() == 58426 && GetSpellProto()->SpellFamilyFlags & uint64(0x0000000000400000))
                    {
                        target->CastSpell(target, 58427, TRIGGERED_OLD_TRIGGERED);
                    }
                }
            }
        }
    }
    else
    {
        // only at real aura remove of _last_ SPELL_AURA_MOD_STEALTH
        if (Real && !target->HasAuraType(SPELL_AURA_MOD_STEALTH))
        {
            // if no GM invisibility
            if (target->GetVisibility() != VISIBILITY_OFF)
            {
                target->RemoveStandFlags(UNIT_STAND_FLAGS_CREEP);

                if (target->GetTypeId() == TYPEID_PLAYER)
                    target->RemoveByteFlag(PLAYER_FIELD_BYTES2, 3, PLAYER_FIELD_BYTE2_STEALTH);

                // restore invisibility if any
                if (target->HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
                {
                    target->SetVisibility(VISIBILITY_GROUP_NO_DETECT);
                    target->SetVisibility(VISIBILITY_GROUP_INVISIBILITY);
                }
                else
                    target->SetVisibility(VISIBILITY_ON);
            }

            // apply delayed talent bonus remover at last stealth aura remove
            Unit::AuraList const& mDummyAuras = target->GetAurasByType(SPELL_AURA_DUMMY);
            for (auto mDummyAura : mDummyAuras)
            {
                // Master of Subtlety
                if (mDummyAura->GetSpellProto()->SpellIconID == 2114)
                    target->CastSpell(target, 31666, TRIGGERED_OLD_TRIGGERED);
                // Overkill
                else if (mDummyAura->GetId() == 58426 && GetSpellProto()->SpellFamilyFlags & uint64(0x0000000000400000))
                {
                    if (SpellAuraHolder* holder = target->GetSpellAuraHolder(58427))
                    {
                        holder->SetAuraMaxDuration(20 * IN_MILLISECONDS);
                        holder->RefreshHolder();
                    }
                }
            }
        }

        if (GetId() == 29448) // Moroes Vanish
            target->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, target, target);
    }
}

void Aura::HandleInvisibility(bool apply, bool Real)
{
    Unit* target = GetTarget();

    target->AddInvisibilityValue(m_modifier.m_miscvalue, apply ? m_modifier.m_amount : -m_modifier.m_amount);
    int32 value = target->GetInvisibilityValue(m_modifier.m_miscvalue);
    bool trueApply = value > 0;
    target->SetInvisibilityMask(m_modifier.m_miscvalue, trueApply);
    if (trueApply)
    {
        target->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);

        if (Real && target->GetTypeId() == TYPEID_PLAYER)
        {
            if (((Player*)target)->IsSelfMover()) // check if the player doesnt have a mover, when player is hidden during MC of creature
            {
                // apply glow vision
                target->SetByteFlag(PLAYER_FIELD_BYTES2, 3, PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW);
            }
        }

        // apply only if not in GM invisibility and not stealth
        if (target->GetVisibility() == VISIBILITY_ON)
            target->SetVisibilityWithoutUpdate(VISIBILITY_GROUP_INVISIBILITY);
    }
    else
    {
        // only at real aura remove and if not have different invisibility auras.
        if (Real && target->GetInvisibilityMask() == 0)
        {
            // remove glow vision
            if (target->GetTypeId() == TYPEID_PLAYER)
                target->RemoveByteFlag(PLAYER_FIELD_BYTES2, 3, PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW);

            // apply only if not in GM invisibility & not stealthed while invisible
            if (target->GetVisibility() != VISIBILITY_OFF)
            {
                // if have stealth aura then already have stealth visibility
                if (!target->HasAuraType(SPELL_AURA_MOD_STEALTH))
                    target->SetVisibilityWithoutUpdate(VISIBILITY_ON);
            }
        }

        if (GetId() == 48809)                               // Binding Life
            target->CastSpell(target, GetSpellProto()->CalculateSimpleValue(m_effIndex), TRIGGERED_OLD_TRIGGERED);
    }

    if (target->IsInWorld())
        target->UpdateVisibilityAndView();

    if (!apply)
    {
        switch (GetId())
        {
            case 38544:
            {
                if (target->GetTypeId() != TYPEID_PLAYER)
                    break;

                Player* pPlayer = (Player*)target;

                if (pPlayer->GetMover() != target)
                {
                    if (Creature* mover = (Creature*)pPlayer->GetMover()) // this spell uses DoSummonPossesed so remove this on removal
                    {
                        pPlayer->BreakCharmOutgoing();
                        mover->ForcedDespawn();
                    }
                }
            }
            break;
        }
    }
}

void Aura::HandleInvisibilityDetect(bool apply, bool Real)
{
    Unit* target = GetTarget();

    target->SetInvisibilityDetectMask(m_modifier.m_miscvalue, apply);
    target->AddInvisibilityDetectValue(m_modifier.m_miscvalue, apply ? m_modifier.m_amount : -m_modifier.m_amount);
    if (!apply)
    {
        Unit::AuraList const& auras = target->GetAurasByType(SPELL_AURA_MOD_INVISIBILITY_DETECTION);
        for (Aura* aura : auras)
            target->SetInvisibilityDetectMask(aura->GetModifier()->m_miscvalue, true);
    }
    if (Real && target->GetTypeId() == TYPEID_PLAYER)
        ((Player*)target)->GetCamera().UpdateVisibilityForOwner();
}

void Aura::HandleDetectAmore(bool apply, bool /*real*/)
{
    GetTarget()->ApplyModByteFlag(PLAYER_FIELD_BYTES2, 3, (PLAYER_FIELD_BYTE2_DETECT_AMORE_0 << m_modifier.m_amount), apply);
}

void Aura::HandleAuraModRoot(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (apply)
    {
        // Frost root aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
            target->ModifyAuraState(AURA_STATE_FROZEN, apply);

        if (Unit* caster = GetCaster())
            if (UnitAI* ai = caster->AI())
                ai->JustRootedTarget(GetSpellProto(), target);
    }
    else
    {
        // Frost root aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
        {
            bool found_another = false;
            for (AuraType const* itr = &frozenAuraTypes[0]; *itr != SPELL_AURA_NONE; ++itr)
            {
                Unit::AuraList const& auras = target->GetAurasByType(*itr);
                for (auto aura : auras)
                {
                    if (GetSpellSchoolMask(aura->GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
                    {
                        found_another = true;
                        break;
                    }
                }
                if (found_another)
                    break;
            }

            if (!found_another)
                target->ModifyAuraState(AURA_STATE_FROZEN, apply);
        }

        // Real remove called after current aura remove from lists, check if other similar auras active
        if (target->HasAuraType(SPELL_AURA_MOD_ROOT))
            return;
    }

    target->SetImmobilizedState(apply);
}

void Aura::HandleAuraModSilence(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (apply)
    {
        target->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED);
        // Stop cast only spells vs PreventionType == SPELL_PREVENTION_TYPE_SILENCE
        for (uint32 i = CURRENT_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
            if (Spell* spell = target->GetCurrentSpell(CurrentSpellTypes(i)))
                if (spell->m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
                    // Stop spells on prepare or casting state
                    target->InterruptSpell(CurrentSpellTypes(i), false);
    }
    else
    {
        // Real remove called after current aura remove from lists, check if other similar auras active
        if (target->HasAuraType(SPELL_AURA_MOD_SILENCE))
            return;

        target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED);
    }
}

void Aura::HandleModThreat(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (!target->isAlive())
        return;

    int level_diff = 0;
    int multiplier = 0;
    switch (GetId())
    {
        // Arcane Shroud
        case 26400:
            level_diff = target->getLevel() - 60;
            multiplier = 2;
            break;
        // The Eye of Diminution
        case 28862:
            level_diff = target->getLevel() - 60;
            multiplier = 1;
            break;
    }

    if (level_diff > 0)
        m_modifier.m_amount += multiplier * level_diff;

    if (target->GetTypeId() == TYPEID_PLAYER)
        for (int8 x = 0; x < MAX_SPELL_SCHOOL; ++x)
            if (m_modifier.m_miscvalue & int32(1 << x))
                ApplyPercentModFloatVar(target->m_threatModifier[x], float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModTotalThreat(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (!target->isAlive() || target->GetTypeId() != TYPEID_PLAYER)
        return;

    Unit* caster = GetCaster();

    if (!caster || !caster->isAlive())
        return;

    target->getHostileRefManager().threatTemporaryFade(caster, m_modifier.m_amount, apply);
}

void Aura::HandleModTaunt(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (!target->isAlive() || !target->CanHaveThreatList())
        return;

    target->TauntUpdate();
}

void Aura::HandleAuraFakeInebriation(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        int32 point = target->GetInt32Value(PLAYER_FAKE_INEBRIATION);
        point += (apply ? 1 : -1) * GetBasePoints();

        target->SetInt32Value(PLAYER_FAKE_INEBRIATION, point);
    }

    target->UpdateObjectVisibility();
}

/*********************************************************/
/***                  MODIFY SPEED                     ***/
/*********************************************************/
void Aura::HandleAuraModIncreaseSpeed(bool /*apply*/, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    GetTarget()->UpdateSpeed(MOVE_RUN, true);
}

void Aura::HandleAuraModIncreaseMountedSpeed(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    target->UpdateSpeed(MOVE_RUN, true);

    // Festive Holiday Mount
    if (apply && GetSpellProto()->SpellIconID != 1794 && target->HasAura(62061))
        // Reindeer Transformation
        target->CastSpell(target, 25860, TRIGGERED_OLD_TRIGGERED, nullptr, this);
}

void Aura::HandleAuraModIncreaseFlightSpeed(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    // Enable Fly mode for flying mounts
    if (m_modifier.m_auraname == SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED)
    {
        target->SetCanFly(apply);

        // Players on flying mounts must be immune to polymorph
        if (target->GetTypeId() == TYPEID_PLAYER)
            target->ApplySpellImmune(this, IMMUNITY_MECHANIC, MECHANIC_POLYMORPH, apply);

        // Dragonmaw Illusion (overwrite mount model, mounted aura already applied)
        if (apply && target->HasAura(42016, EFFECT_INDEX_0) && target->GetMountID())
            target->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 16314);

        // Festive Holiday Mount
        if (apply && GetSpellProto()->SpellIconID != 1794 && target->HasAura(62061))
            // Reindeer Transformation
            target->CastSpell(target, 25860, TRIGGERED_OLD_TRIGGERED, nullptr, this);
    }

    // Swift Flight Form check for higher speed flying mounts
    if (apply && target->GetTypeId() == TYPEID_PLAYER && GetSpellProto()->Id == 40121)
    {
        for (PlayerSpellMap::const_iterator iter = ((Player*)target)->GetSpellMap().begin(); iter != ((Player*)target)->GetSpellMap().end(); ++iter)
        {
            if (iter->second.state != PLAYERSPELL_REMOVED)
            {
                bool changedSpeed = false;
                SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(iter->first);
                for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    if (spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED)
                    {
                        int32 mountSpeed = spellInfo->CalculateSimpleValue(SpellEffectIndex(i));
                        if (mountSpeed > m_modifier.m_amount)
                        {
                            m_modifier.m_amount = mountSpeed;
                            changedSpeed = true;
                            break;
                        }
                    }
                }
                if (changedSpeed)
                    break;
            }
        }
    }

    target->UpdateSpeed(MOVE_FLIGHT, true);
}

void Aura::HandleAuraModIncreaseSwimSpeed(bool /*apply*/, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    GetTarget()->UpdateSpeed(MOVE_SWIM, true);
}

void Aura::HandleAuraModDecreaseSpeed(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (apply)
    {
        // Gronn Lord's Grasp, becomes stoned
        if (GetId() == 33572)
        {
            if (GetStackAmount() >= 5 && !target->HasAura(33652))
                target->CastSpell(target, 33652, TRIGGERED_OLD_TRIGGERED);
        }
    }

    target->UpdateSpeed(MOVE_RUN, true);
    target->UpdateSpeed(MOVE_SWIM, true);
    target->UpdateSpeed(MOVE_FLIGHT, true);
}

void Aura::HandleAuraModUseNormalSpeed(bool /*apply*/, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    target->UpdateSpeed(MOVE_RUN, true);
    target->UpdateSpeed(MOVE_SWIM, true);
    target->UpdateSpeed(MOVE_FLIGHT, true);
}

/*********************************************************/
/***                     IMMUNITY                      ***/
/*********************************************************/

void Aura::HandleModMechanicImmunity(bool apply, bool /*Real*/)
{
    uint32 misc  = m_modifier.m_miscvalue;
    // Forbearance
    // in DBC wrong mechanic immune since 3.0.x
    if (GetId() == 25771)
        misc = MECHANIC_IMMUNE_SHIELD;

    Unit* target = GetTarget();

    if (apply && GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
    {
        uint32 mechanic = 1 << (misc - 1);

        // immune movement impairment and loss of control (spell data have special structure for mark this case)
        if (IsSpellRemoveAllMovementAndControlLossEffects(GetSpellProto()))
            mechanic = IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK;

        target->RemoveAurasAtMechanicImmunity(mechanic, GetId());
    }

    target->ApplySpellImmune(this, IMMUNITY_MECHANIC, misc, apply);

    // Bestial Wrath
    if (GetSpellProto()->SpellFamilyName == SPELLFAMILY_HUNTER && GetSpellProto()->SpellIconID == 1680)
    {
        // The Beast Within cast on owner if talent present
        if (Unit* owner = target->GetOwner())
        {
            // Search talent The Beast Within
            Unit::AuraList const& dummyAuras = owner->GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
            for (auto dummyAura : dummyAuras)
            {
                if (dummyAura->GetSpellProto()->SpellIconID == 2229)
                {
                    if (apply)
                        owner->CastSpell(owner, 34471, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    else
                        owner->RemoveAurasDueToSpell(34471);
                    break;
                }
            }
        }
    }
    // Heroic Fury (Intercept cooldown remove)
    else if (apply && GetSpellProto()->Id == 60970 && target->GetTypeId() == TYPEID_PLAYER)
        target->RemoveSpellCooldown(20252, true);

    switch (GetId())
    {
        case 18461: // Vanish Purge
            if (m_effIndex == EFFECT_INDEX_0)
                target->RemoveSpellsCausingAura(SPELL_AURA_MOD_STALKED);
            break;
        case 42292: // PvP trinket
            target->RemoveRankAurasDueToSpell(20184); // Judgement of justice - remove any rank
            break;
    }
}

void Aura::HandleModMechanicImmunityMask(bool apply, bool /*Real*/)
{
    uint32 mechanic  = m_modifier.m_miscvalue;

    if (apply && GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
        GetTarget()->RemoveAurasAtMechanicImmunity(mechanic, GetId());

    // check implemented in Unit::IsImmuneToSpell and Unit::IsImmuneToSpellEffect
}

// this method is called whenever we add / remove aura which gives m_target some imunity to some spell effect
void Aura::HandleAuraModEffectImmunity(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    // when removing flag aura, handle flag drop
    if (target->GetTypeId() == TYPEID_PLAYER && (GetSpellProto()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION))
    {
        Player* player = static_cast<Player*>(target);

        if (apply)
            player->pvpInfo.isPvPFlagCarrier = true;
        else
        {
            player->pvpInfo.isPvPFlagCarrier = false;

            if (BattleGround* bg = player->GetBattleGround())
                bg->EventPlayerDroppedFlag(player);
            else if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
                outdoorPvP->HandleDropFlag(player, GetSpellProto()->Id);
        }
    }

    target->ApplySpellImmune(this, IMMUNITY_EFFECT, m_modifier.m_miscvalue, apply);

    switch (GetSpellProto()->Id)
    {
        case 32430:                     // Battle Standard (Alliance - ZM OPVP)
        case 32431:                     // Battle Standard (Horde - ZM OPVP)
        {
            // Handle OPVP script condition change on aura apply; Specific for Zangarmarsh outdoor pvp
            if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(3521))
                outdoorPvP->HandleConditionStateChange(uint32(GetSpellProto()->Id == 32431), apply);
        }
    }
}

void Aura::HandleAuraModStateImmunity(bool apply, bool Real)
{
    if (apply && Real && GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
    {
        Unit::AuraList const& auraList = GetTarget()->GetAurasByType(AuraType(m_modifier.m_miscvalue));
        for (Unit::AuraList::const_iterator itr = auraList.begin(); itr != auraList.end();)
        {
            if (auraList.front() != this)                   // skip itself aura (it already added)
            {
                GetTarget()->RemoveAurasDueToSpell(auraList.front()->GetId());
                itr = auraList.begin();
            }
            else
                ++itr;
        }
    }

    GetTarget()->ApplySpellImmune(this, IMMUNITY_STATE, m_modifier.m_miscvalue, apply);
}

void Aura::HandleAuraModSchoolImmunity(bool apply, bool Real)
{
    Unit* target = GetTarget();
    target->ApplySpellImmune(this, IMMUNITY_SCHOOL, m_modifier.m_miscvalue, apply);

    // remove all flag auras (they are positive, but they must be removed when you are immune)
    if (GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY) && GetSpellProto()->HasAttribute(SPELL_ATTR_EX2_DAMAGE_REDUCED_SHIELD))
        target->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);

    // TODO: optimalize this cycle - use RemoveAurasWithInterruptFlags call or something else
    if (Real && apply
            && GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY)
            && IsPositiveSpell(GetId(), GetCaster(), target))                    // Only positive immunity removes auras
    {
        uint32 school_mask = m_modifier.m_miscvalue;
        Unit::SpellAuraHolderMap& Auras = target->GetSpellAuraHolderMap();
        for (Unit::SpellAuraHolderMap::iterator iter = Auras.begin(), next; iter != Auras.end(); iter = next)
        {
            next = iter;
            ++next;
            SpellEntry const* spell = iter->second->GetSpellProto();
            if ((GetSpellSchoolMask(spell) & school_mask)   // Check for school mask
                    && !iter->second->IsPassive()
                    && !spell->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY)   // Spells unaffected by invulnerability
                    && !iter->second->IsPositive()          // Don't remove positive spells
                    && spell->Id != GetId())                // Don't remove self
            {
                target->RemoveAurasDueToSpell(spell->Id);
                if (Auras.empty())
                    break;
                next = Auras.begin();
            }
        }
    }
    if (Real && GetSpellProto()->Mechanic == MECHANIC_BANISH)
    {
        if (apply)
            target->addUnitState(UNIT_STAT_ISOLATED);
        else
            target->clearUnitState(UNIT_STAT_ISOLATED);
    }

    GetTarget()->getHostileRefManager().HandleSuppressed(apply, true);
}

void Aura::HandleAuraModDmgImmunity(bool apply, bool /*Real*/)
{
    GetTarget()->ApplySpellImmune(this, IMMUNITY_DAMAGE, m_modifier.m_miscvalue, apply);

    GetTarget()->getHostileRefManager().HandleSuppressed(apply, true);
}

void Aura::HandleAuraModDispelImmunity(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    GetTarget()->ApplySpellDispelImmunity(this, DispelType(m_modifier.m_miscvalue), apply);
}

void Aura::HandleAuraProcTriggerSpell(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    switch (GetId())
    {
        // some spell have charges by functionality not have its in spell data
        case 28200:                                         // Ascendance (Talisman of Ascendance trinket)
            if (apply)
                GetHolder()->SetAuraCharges(6);
            break;
        case 50720:                                         // Vigilance (threat transfering)
            if (apply)
            {
                if (Unit* caster = GetCaster())
                    target->CastSpell(caster, 59665, TRIGGERED_OLD_TRIGGERED);
            }
            else
                target->getHostileRefManager().ResetThreatRedirection();
            break;
        default:
            break;
    }
}

void Aura::HandleAuraModStalked(bool apply, bool /*Real*/)
{
    // used by spells: Hunter's Mark, Mind Vision, Syndicate Tracker (MURP) DND
    if (apply)
        GetTarget()->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TRACK_UNIT);
    else
        GetTarget()->RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TRACK_UNIT);
}

/*********************************************************/
/***                   PERIODIC                        ***/
/*********************************************************/

void Aura::HandlePeriodicTriggerSpell(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;

    Unit* target = GetTarget();

    if (apply)
    {
        switch (GetId())
        {
            case 29946:
                if (target->HasAura(29947))
                    target->RemoveAurasDueToSpellByCancel(29947);
            default:
                break;
        }
    }
    else
    {
        switch (GetId())
        {
            case 66:                                        // Invisibility
                if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                    target->CastSpell(target, 32612, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                return;
            case 18173:                                     // Burning Adrenaline (Main Target version)
            case 23620:                                     // Burning Adrenaline (Caster version)
                // On aura removal, the target deals AoE damage to friendlies and kills himself/herself (prevent durability loss)
                target->CastSpell(target, 23478, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                target->CastSpell(target, 23644, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            case 29946:
                if (m_removeMode != AURA_REMOVE_BY_EXPIRE)
                    // Cast "crossed flames debuff"
                    target->CastSpell(target, 29947, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            case 33401:                                     // Possess
                if (Unit* caster = GetCaster())
                    caster->CastSpell(target, 32830, TRIGGERED_NONE);
                return;
            case 33711:                                     // Murmur's Touch normal and heroic
            case 38794:
                target->CastSpell(nullptr, 33686, TRIGGERED_OLD_TRIGGERED, nullptr, this); // cast Shockwave
                target->CastSpell(nullptr, 33673, TRIGGERED_OLD_TRIGGERED); // cast Shockwave knockup serverside
                return;
            case 35515:                                     // Salaadin's Tesla
                if ((m_removeMode != AURA_REMOVE_BY_STACK) && (!target->HasAura(35515)))
                    if (Creature* creature = (Creature*)target)
                        creature->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, creature, creature);
                return;
            case 37640:                                     // Leotheras Whirlwind
                if (Unit* pCaster = GetCaster())
                    pCaster->FixateTarget(nullptr);
                return;
            case 37670:                                     // Nether Charge Timer
                target->CastSpell(nullptr, GetSpellProto()->EffectTriggerSpell[m_effIndex], TRIGGERED_OLD_TRIGGERED);
                break;
            case 39828:                                     // Light of the Naaru
                target->CastSpell(nullptr, 39831, TRIGGERED_NONE);
                target->CastSpell(nullptr, 39832, TRIGGERED_NONE);
                break;
            case 42783:                                     // Wrath of the Astrom...
                if (m_removeMode == AURA_REMOVE_BY_EXPIRE && GetEffIndex() + 1 < MAX_EFFECT_INDEX)
                    target->CastSpell(target, GetSpellProto()->CalculateSimpleValue(SpellEffectIndex(GetEffIndex() + 1)), TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                return;
            case 46221:                                     // Animal Blood
                if (target->GetTypeId() == TYPEID_PLAYER && m_removeMode == AURA_REMOVE_BY_DEFAULT && target->IsInWater())
                {
                    float position_z = target->GetTerrain()->GetWaterLevel(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
                    // Spawn Blood Pool
                    target->CastSpell(target->GetPositionX(), target->GetPositionY(), position_z, 63471, TRIGGERED_OLD_TRIGGERED);
                }

                return;
            case 51912:                                     // Ultra-Advanced Proto-Typical Shortening Blaster
                if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                {
                    if (Unit* pCaster = GetCaster())
                        pCaster->CastSpell(target, GetSpellProto()->EffectTriggerSpell[GetEffIndex()], TRIGGERED_OLD_TRIGGERED, nullptr, this);
                }

                return;
            case 70405:                                     // Mutated Transformation (10n)
            case 72508:                                     // Mutated Transformation (25n)
            case 72509:                                     // Mutated Transformation (10h)
            case 72510:                                     // Mutated Transformation (25h)
                if (m_removeMode == AURA_REMOVE_BY_DEFAULT)
                {
                    if (target->IsVehicle() && target->GetTypeId() == TYPEID_UNIT)
                    {
                        target->RemoveSpellsCausingAura(SPELL_AURA_CONTROL_VEHICLE);
                        ((Creature*)target)->ForcedDespawn();
                    }
                }
            default:
                break;
        }
    }
}

void Aura::HandlePeriodicTriggerSpellWithValue(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;
}

void Aura::HandlePeriodicEnergize(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    // For prevent double apply bonuses
    bool loading = (target->GetTypeId() == TYPEID_PLAYER && ((Player*)target)->GetSession()->PlayerLoading());

    if (apply && !loading)
    {
        switch (GetId())
        {
            case 54833:                                     // Glyph of Innervate (value%/2 of casters base mana)
            {
                if (Unit* caster = GetCaster())
                    m_modifier.m_amount = int32(caster->GetCreateMana() * GetBasePoints() / (200 * GetAuraMaxTicks()));
                break;
            }
            case 29166:                                     // Innervate (value% of casters base mana)
            {
                if (Unit* caster = GetCaster())
                {
                    // Glyph of Innervate
                    if (caster->HasAura(54832))
                        caster->CastSpell(caster, 54833, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                    m_modifier.m_amount = int32(caster->GetCreateMana() * GetBasePoints() / (100 * GetAuraMaxTicks()));
                }
                break;
            }
            case 48391:                                     // Owlkin Frenzy 2% base mana
                m_modifier.m_amount = target->GetCreateMana() * 2 / 100;
                break;
            case 57669:                                     // Replenishment (0.2% from max)
            case 61782:                                     // Infinite Replenishment
                m_modifier.m_amount = target->GetMaxPower(POWER_MANA) * 2 / 1000;
                break;
            default:
                break;
        }
    }

    m_isPeriodic = apply;
}

void Aura::HandleAuraPowerBurn(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;
}

void Aura::HandlePrayerOfMending(bool apply, bool /*Real*/)
{
    if (apply) // only on initial cast apply SP
        if (const SpellEntry* entry = GetSpellProto())
            if (GetHolder()->GetAuraCharges() == entry->procCharges)
                m_modifier.m_amount = GetCaster()->SpellHealingBonusDone(GetTarget(), GetSpellProto(), m_modifier.m_amount, HEAL);
}

void Aura::HandleAuraPeriodicDummy(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    // For prevent double apply bonuses
    bool loading = (target->GetTypeId() == TYPEID_PLAYER && ((Player*)target)->GetSession()->PlayerLoading());

    SpellEntry const* spell = GetSpellProto();
    switch (spell->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
        {
            if (!target)
                return;

            switch (spell->Id)
            {
                case 36207:                                     // Steal Weapon
                {
                    if (target->GetTypeId() != TYPEID_UNIT)
                        return;

                    if (apply)
                    {
                        if (Player* playerCaster = GetCaster()->GetBeneficiaryPlayer())
                        {
                            if (Item* item = playerCaster->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
                            {
                                ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, item->GetEntry());
                            }
                        }
                    }
                    else
                    {
                        ((Creature*)target)->LoadEquipment(((Creature*)target)->GetCreatureInfo()->EquipmentTemplateId, true);
                    }
                }
                case 30019:                                     // Control Piece - Chess
                {
                    if (apply || target->GetTypeId() != TYPEID_PLAYER)
                        return;

                    target->CastSpell(target, 30529, TRIGGERED_OLD_TRIGGERED);
                    target->RemoveAurasDueToSpell(30019);
                    target->RemoveAurasDueToSpell(30532);

                    Unit* chessPiece = target->GetCharm();
                    if (chessPiece)
                        chessPiece->RemoveAurasDueToSpell(30019);

                    return;
                }
                case 47214: // Burninate Effect
                {
                    if (apply)
                    {
                        Unit* caster = GetCaster();
                        if (!caster)
                            return;

                        target->CastSpell(caster, 47208, TRIGGERED_NONE);
                        target->CastSpell(nullptr, 42726, TRIGGERED_NONE);
                    }
                    else // kill self on removal
                        target->CastSpell(nullptr, 51744, TRIGGERED_NONE);
                    break;
                }
            }
        }
        case SPELLFAMILY_ROGUE:
        {
            switch (spell->Id)
            {
                // Master of Subtlety
                case 31666:
                {
                    if (apply)
                    {
                        // for make duration visible
                        if (SpellAuraHolder* holder = target->GetSpellAuraHolder(31665))
                        {
                            holder->SetAuraMaxDuration(GetHolder()->GetAuraDuration());
                            holder->RefreshHolder();
                        }
                    }
                    else
                        target->RemoveAurasDueToSpell(31665);
                    break;
                }
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            Unit* caster = GetCaster();

            // Explosive Shot
            if (apply && !loading && caster)
                m_modifier.m_amount += int32(caster->GetTotalAttackPowerValue(RANGED_ATTACK) * 14 / 100);
            break;
        }
    }

    m_isPeriodic = apply;
}

void Aura::HandlePeriodicHeal(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;

    Unit* target = GetTarget();

    // For prevent double apply bonuses
    bool loading = (target->GetTypeId() == TYPEID_PLAYER && ((Player*)target)->GetSession()->PlayerLoading());

    // Custom damage calculation after
    if (apply)
    {
        if (loading)
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Gift of the Naaru (have diff spellfamilies)
        if (GetSpellProto()->SpellIconID == 329 && GetSpellProto()->SpellVisual[0] == 7625)
        {
            int32 ap = int32(0.22f * caster->GetTotalAttackPowerValue(BASE_ATTACK));
            int32 holy = caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(GetSpellProto()));
            if (holy < 0)
                holy = 0;
            holy = int32(holy * 377 / 1000);
            m_modifier.m_amount += ap > holy ? ap : holy;
        }
        // Lifeblood
        else if (GetSpellProto()->SpellIconID == 3088 && GetSpellProto()->SpellVisual[0] == 8145)
        {
            int32 healthBonus = int32(0.0032f * caster->GetMaxHealth());
            m_modifier.m_amount += healthBonus;
        }

        switch (GetSpellProto()->Id)
        {
            case 12939: m_modifier.m_amount = target->GetMaxHealth() / 3; break; // Polymorph Heal Effect
            default: m_modifier.m_amount = caster->SpellHealingBonusDone(target, GetSpellProto(), m_modifier.m_amount, DOT, GetStackAmount()); break;
        }

        // Rejuvenation
        if (GetSpellProto()->IsFitToFamily(SPELLFAMILY_DRUID, uint64(0x0000000000000010)))
            if (caster->HasAura(64760))                     // Item - Druid T8 Restoration 4P Bonus
                caster->CastCustomSpell(target, 64801, &m_modifier.m_amount, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr);
    }
}

void Aura::HandleDamagePercentTaken(bool apply, bool Real)
{
    m_isPeriodic = apply;

    Unit* target = GetTarget();

    if (!Real)
        return;

    // For prevent double apply bonuses
    bool loading = (target->GetTypeId() == TYPEID_PLAYER && ((Player*)target)->GetSession()->PlayerLoading());

    if (apply)
    {
        if (loading)
            return;

        // Hand of Salvation (only it have this aura and mask)
        if (GetSpellProto()->IsFitToFamily(SPELLFAMILY_PALADIN, uint64(0x0000000000000100)))
        {
            // Glyph of Salvation
            if (target->GetObjectGuid() == GetCasterGuid())
                if (Aura* aur = target->GetAura(63225, EFFECT_INDEX_0))
                    m_modifier.m_amount -= aur->GetModifier()->m_amount;
        }
    }
}

void Aura::HandlePeriodicDamage(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    m_isPeriodic = apply;

    Unit* target = GetTarget();
    SpellEntry const* spellProto = GetSpellProto();

    // For prevent double apply bonuses
    bool loading = (target->GetTypeId() == TYPEID_PLAYER && ((Player*)target)->GetSession()->PlayerLoading());

    // Custom damage calculation after
    if (apply)
    {
        if (loading)
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        switch (spellProto->SpellFamilyName)
        {
            case SPELLFAMILY_WARRIOR:
            {
                // Rend
                if (spellProto->SpellFamilyFlags & uint64(0x0000000000000020))
                {
                    // $0.2*(($MWB+$mwb)/2+$AP/14*$MWS) bonus per tick
                    float ap = caster->GetTotalAttackPowerValue(BASE_ATTACK);
                    int32 mws = caster->GetAttackTime(BASE_ATTACK);
                    float mwb_min = caster->GetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE);
                    float mwb_max = caster->GetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE);
                    m_modifier.m_amount += int32(((mwb_min + mwb_max) / 2 + ap * mws / 14000) * 0.2f);
                    // If used while target is above 75% health, Rend does 35% more damage
                    if (spellProto->CalculateSimpleValue(EFFECT_INDEX_1) != 0 &&
                            target->GetHealth() > target->GetMaxHealth() * spellProto->CalculateSimpleValue(EFFECT_INDEX_1) / 100)
                        m_modifier.m_amount += m_modifier.m_amount * spellProto->CalculateSimpleValue(EFFECT_INDEX_2) / 100;
                }
                break;
            }
            case SPELLFAMILY_DRUID:
            {
                // Rip
                if (spellProto->SpellFamilyFlags & uint64(0x000000000000800000))
                {
                    if (caster->GetTypeId() != TYPEID_PLAYER)
                        break;

                    // 0.01*$AP*cp
                    uint8 cp = ((Player*)caster)->GetComboPoints();

                    // Idol of Feral Shadows. Cant be handled as SpellMod in SpellAura:Dummy due its dependency from CPs
                    Unit::AuraList const& dummyAuras = caster->GetAurasByType(SPELL_AURA_DUMMY);
                    for (auto dummyAura : dummyAuras)
                    {
                        if (dummyAura->GetId() == 34241)
                        {
                            m_modifier.m_amount += cp * dummyAura->GetModifier()->m_amount;
                            break;
                        }
                    }
                    m_modifier.m_amount += int32(caster->GetTotalAttackPowerValue(BASE_ATTACK) * cp / 100);
                }
                break;
            }
            case SPELLFAMILY_ROGUE:
            {
                // Rupture
                if (spellProto->SpellFamilyFlags & uint64(0x000000000000100000))
                {
                    if (caster->GetTypeId() != TYPEID_PLAYER)
                        break;
                    // 1 point : ${($m1+$b1*1+0.015*$AP)*4} damage over 8 secs
                    // 2 points: ${($m1+$b1*2+0.024*$AP)*5} damage over 10 secs
                    // 3 points: ${($m1+$b1*3+0.03*$AP)*6} damage over 12 secs
                    // 4 points: ${($m1+$b1*4+0.03428571*$AP)*7} damage over 14 secs
                    // 5 points: ${($m1+$b1*5+0.0375*$AP)*8} damage over 16 secs
                    float AP_per_combo[6] = {0.0f, 0.015f, 0.024f, 0.03f, 0.03428571f, 0.0375f};
                    uint8 cp = ((Player*)caster)->GetComboPoints();
                    if (cp > 5) cp = 5;
                    m_modifier.m_amount += int32(caster->GetTotalAttackPowerValue(BASE_ATTACK) * AP_per_combo[cp]);
                }
                break;
            }
            case SPELLFAMILY_PALADIN:
            {
                // Holy Vengeance / Blood Corruption
                if (spellProto->SpellFamilyFlags & uint64(0x0000080000000000) && spellProto->SpellVisual[0] == 7902)
                {
                    // AP * 0.025 + SPH * 0.013 bonus per tick
                    float ap = caster->GetTotalAttackPowerValue(BASE_ATTACK);
                    int32 holy = caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(spellProto));
                    if (holy < 0)
                        holy = 0;
                    m_modifier.m_amount += int32(GetStackAmount()) * (int32(ap * 0.025f) + int32(holy * 13 / 1000));
                }
                break;
            }
            default:
                break;
        }

        if (m_modifier.m_auraname == SPELL_AURA_PERIODIC_DAMAGE)
        {
            // SpellDamageBonusDone for magic spells
            if (spellProto->DmgClass == SPELL_DAMAGE_CLASS_NONE || spellProto->DmgClass == SPELL_DAMAGE_CLASS_MAGIC)
                m_modifier.m_amount = caster->SpellDamageBonusDone(target, GetSpellProto(), m_modifier.m_amount, DOT, GetStackAmount());
            // MeleeDamagebonusDone for weapon based spells
            else
            {
                WeaponAttackType attackType = GetWeaponAttackType(GetSpellProto());
                m_modifier.m_amount = caster->MeleeDamageBonusDone(target, m_modifier.m_amount, attackType, SpellSchoolMask(spellProto->SchoolMask), spellProto, DOT, GetStackAmount());
            }
        }
    }
    // remove time effects
    else
    {
        switch (spellProto->Id)
        {
            case 30410: // Shadow Grasp cast Mind Exhaustion on removal
                target->CastSpell(target, 44032, TRIGGERED_OLD_TRIGGERED);
                break;
            case 35201: // Paralytic Poison
                if (m_removeMode == AURA_REMOVE_BY_DEFAULT)
                    target->CastSpell(target, 35202, TRIGGERED_OLD_TRIGGERED); // Paralysis
                break;
            case 41917: // Parasitic Shadowfiend - handle summoning of two Shadowfiends on DoT expire
                target->CastSpell(target, 41915, TRIGGERED_OLD_TRIGGERED);
                break;
        }
    }
}

void Aura::HandlePeriodicDamagePCT(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;
}

void Aura::HandlePeriodicLeech(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;

    // For prevent double apply bonuses
    bool loading = (GetTarget()->GetTypeId() == TYPEID_PLAYER && ((Player*)GetTarget())->GetSession()->PlayerLoading());

    // Custom damage calculation after
    if (apply)
    {
        if (loading)
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        m_modifier.m_amount = caster->SpellDamageBonusDone(GetTarget(), GetSpellProto(), m_modifier.m_amount, DOT, GetStackAmount());
    }
}

void Aura::HandlePeriodicManaLeech(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;
}

void Aura::HandlePeriodicHealthFunnel(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;

    // For prevent double apply bonuses
    bool loading = (GetTarget()->GetTypeId() == TYPEID_PLAYER && ((Player*)GetTarget())->GetSession()->PlayerLoading());

    // Custom damage calculation after
    if (apply)
    {
        if (loading)
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        m_modifier.m_amount = caster->SpellDamageBonusDone(GetTarget(), GetSpellProto(), m_modifier.m_amount, DOT, GetStackAmount());
    }
}

/*********************************************************/
/***                  MODIFY STATS                     ***/
/*********************************************************/

/********************************/
/***        RESISTANCE        ***/
/********************************/

void Aura::HandleAuraModResistanceExclusive(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    // Need to check if Exclusive aura is already in effect, if yes ignore application
    for (int8 x = SPELL_SCHOOL_NORMAL; x < MAX_SPELL_SCHOOL; ++x)
    {
        if (m_modifier.m_miscvalue & int32(1 << x))
        {
            int32 applyDiff = m_modifier.m_amount;
            int32 highestValue = 0;

            auto const& auras = target->GetAurasByType(m_modifier.m_auraname);
            for (Aura* aura : auras)
            {
                if (aura->GetId() != GetId() && (aura->GetMiscValue() & int32(1 << x)) && aura->GetModifier()->m_amount > highestValue)
                    highestValue = aura->GetModifier()->m_amount;
            }

            // If current value is higher value of currently existed value calculate application difference.
            // Ie. Current resistance 45 new 70 (70-45) = 35 difference will be applied
            if (apply)
            {
                if (m_modifier.m_amount > target->GetModifierValue(UnitMods(UNIT_MOD_RESISTANCE_START + x), BASE_EXCLUSIVE))
                    applyDiff -= target->GetModifierValue(UnitMods(UNIT_MOD_RESISTANCE_START + x), BASE_EXCLUSIVE);
                else
                    continue;
            }
            else
            {
                if (m_modifier.m_amount > highestValue)
                    applyDiff -= highestValue;
                else
                    continue;
            }

            target->HandleStatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + x), BASE_EXCLUSIVE, float(applyDiff), apply);
            if (target->GetTypeId() == TYPEID_PLAYER)
                target->ApplyResistanceBuffModsMod(SpellSchools(x), m_positive, float(applyDiff), apply);
        }
    }
}

void Aura::HandleAuraModResistance(bool apply, bool /*Real*/)
{
    for (int8 x = SPELL_SCHOOL_NORMAL; x < MAX_SPELL_SCHOOL; ++x)
    {
        if (m_modifier.m_miscvalue & int32(1 << x))
        {
            GetTarget()->HandleStatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + x), TOTAL_VALUE, float(m_modifier.m_amount), apply);
            if (GetTarget()->GetTypeId() == TYPEID_PLAYER || ((Creature*)GetTarget())->IsPet())
                GetTarget()->ApplyResistanceBuffModsMod(SpellSchools(x), m_positive, float(m_modifier.m_amount), apply);
        }
    }
}

void Aura::HandleAuraModBaseResistancePCT(bool apply, bool /*Real*/)
{
    // only players have base stats
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        // pets only have base armor
        if (((Creature*)GetTarget())->IsPet() && (m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL))
            GetTarget()->HandleStatModifier(UNIT_MOD_ARMOR, BASE_PCT, float(m_modifier.m_amount), apply);
    }
    else
    {
        for (int8 x = SPELL_SCHOOL_NORMAL; x < MAX_SPELL_SCHOOL; ++x)
        {
            if (m_modifier.m_miscvalue & int32(1 << x))
                GetTarget()->HandleStatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + x), BASE_PCT, float(m_modifier.m_amount), apply);
        }
    }
}

void Aura::HandleModResistancePercent(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    for (int8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        if (m_modifier.m_miscvalue & int32(1 << i))
        {
            target->HandleStatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + i), TOTAL_PCT, float(m_modifier.m_amount), apply);
            if (target->GetTypeId() == TYPEID_PLAYER || ((Creature*)target)->IsPet())
            {
                target->ApplyResistanceBuffModsPercentMod(SpellSchools(i), true, float(m_modifier.m_amount), apply);
                target->ApplyResistanceBuffModsPercentMod(SpellSchools(i), false, float(m_modifier.m_amount), apply);
            }
        }
    }
}

void Aura::HandleModBaseResistance(bool apply, bool /*Real*/)
{
    // only players have base stats
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        // only pets have base stats
        if (((Creature*)GetTarget())->IsPet() && (m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL))
            GetTarget()->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, float(m_modifier.m_amount), apply);
    }
    else
    {
        for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
            if (m_modifier.m_miscvalue & (1 << i))
                GetTarget()->HandleStatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + i), TOTAL_VALUE, float(m_modifier.m_amount), apply);
    }
}

/********************************/
/***           STAT           ***/
/********************************/

void Aura::HandleAuraModStat(bool apply, bool /*Real*/)
{
    if (m_modifier.m_miscvalue < -2 || m_modifier.m_miscvalue > 4)
    {
        sLog.outError("WARNING: Spell %u effect %u have unsupported misc value (%i) for SPELL_AURA_MOD_STAT ", GetId(), GetEffIndex(), m_modifier.m_miscvalue);
        return;
    }

    Unit* target = GetTarget();

    // Holy Strength amount decrease by 4% each level after 60 From Crusader Enchant
    if (apply && GetId() == 20007)
        if (GetCaster()->GetTypeId() == TYPEID_PLAYER && GetCaster()->getLevel() > 60)
            m_modifier.m_amount = int32(m_modifier.m_amount * (1 - (((float(GetCaster()->getLevel()) - 60) * 4) / 100)));

    if (GetSpellProto()->IsFitToFamilyMask(0x0000000000008000))
    {
        if (apply)
        {
            int32 staminaToRemove = 0;
            Unit::AuraList const& auraClassScripts = target->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
            for (Unit::AuraList::const_iterator itr = auraClassScripts.begin(); itr != auraClassScripts.end();)
            {
                switch ((*itr)->GetModifier()->m_miscvalue)
                {
                    case 2388: staminaToRemove = m_modifier.m_amount * 10 / 100; break;
                    case 2389: staminaToRemove = m_modifier.m_amount * 20 / 100; break;
                    case 2390: staminaToRemove = m_modifier.m_amount * 30 / 100; break;
                }
            }
            if (staminaToRemove)
                GetCaster()->CastCustomSpell(target, 19486, &staminaToRemove, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
        }
        else
            target->RemoveAurasTriggeredBySpell(GetId(), GetCasterGuid()); // just do it every time, lookup is too time consuming
    }

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        // -1 or -2 is all stats ( misc < -2 checked in function beginning )
        if (m_modifier.m_miscvalue < 0 || m_modifier.m_miscvalue == i)
        {
            // m_target->ApplyStatMod(Stats(i), m_modifier.m_amount,apply);
            target->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START + i), TOTAL_VALUE, float(m_modifier.m_amount), apply);
            if (target->GetTypeId() == TYPEID_PLAYER || ((Creature*)target)->IsPet())
                target->ApplyStatBuffMod(Stats(i), float(m_modifier.m_amount), apply);
        }
    }
}

void Aura::HandleModPercentStat(bool apply, bool /*Real*/)
{
    if (m_modifier.m_miscvalue < -1 || m_modifier.m_miscvalue > 4)
    {
        sLog.outError("WARNING: Misc Value for SPELL_AURA_MOD_PERCENT_STAT not valid");
        return;
    }

    // only players have base stats
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        if (m_modifier.m_miscvalue == i || m_modifier.m_miscvalue == -1)
            GetTarget()->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START + i), BASE_PCT, float(m_modifier.m_amount), apply);
    }
}

void Aura::HandleModSpellDamagePercentFromStat(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    // Recalculate bonus
    ((Player*)GetTarget())->UpdateSpellDamageBonus();
}

void Aura::HandleModSpellHealingPercentFromStat(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Recalculate bonus
    ((Player*)GetTarget())->UpdateSpellHealingBonus();
}

void Aura::HandleAuraModDispelResist(bool apply, bool Real)
{
    if (!Real || !apply)
        return;

    if (GetId() == 33206)
        GetTarget()->CastSpell(GetTarget(), 44416, TRIGGERED_OLD_TRIGGERED, nullptr, this, GetCasterGuid());
}

void Aura::HandleModSpellDamagePercentFromAttackPower(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    // Recalculate bonus
    ((Player*)GetTarget())->UpdateSpellDamageBonus();
}

void Aura::HandleModSpellHealingPercentFromAttackPower(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Recalculate bonus
    ((Player*)GetTarget())->UpdateSpellHealingBonus();
}

void Aura::HandleModHealingDone(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;
    // implemented in Unit::SpellHealingBonusDone
    // this information is for client side only
    ((Player*)GetTarget())->UpdateSpellHealingBonus();
}

void Aura::HandleModTotalPercentStat(bool apply, bool /*Real*/)
{
    if (m_modifier.m_miscvalue < -1 || m_modifier.m_miscvalue > 4)
    {
        sLog.outError("WARNING: Misc Value for SPELL_AURA_MOD_PERCENT_STAT not valid");
        return;
    }

    Unit* target = GetTarget();

    // save current and max HP before applying aura
    uint32 curHPValue = target->GetHealth();
    uint32 maxHPValue = target->GetMaxHealth();

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        if (m_modifier.m_miscvalue == i || m_modifier.m_miscvalue == -1)
        {
            target->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START + i), TOTAL_PCT, float(m_modifier.m_amount), apply);
            if (target->GetTypeId() == TYPEID_PLAYER || ((Creature*)target)->IsPet())
                target->ApplyStatPercentBuffMod(Stats(i), float(m_modifier.m_amount), apply);
        }
    }

    // recalculate current HP/MP after applying aura modifications (only for spells with 0x10 flag)
    if (m_modifier.m_miscvalue == STAT_STAMINA && maxHPValue > 0 && GetSpellProto()->HasAttribute(SPELL_ATTR_ABILITY))
    {
        // newHP = (curHP / maxHP) * newMaxHP = (newMaxHP * curHP) / maxHP -> which is better because no int -> double -> int conversion is needed
        uint32 newHPValue = (target->GetMaxHealth() * curHPValue) / maxHPValue;
        target->SetHealth(newHPValue);
    }
}

void Aura::HandleAuraModResistenceOfStatPercent(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    if (m_modifier.m_miscvalue != SPELL_SCHOOL_MASK_NORMAL)
    {
        // support required adding replace UpdateArmor by loop by UpdateResistence at intellect update
        // and include in UpdateResistence same code as in UpdateArmor for aura mod apply.
        sLog.outError("Aura SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT(182) need adding support for non-armor resistances!");
        return;
    }

    // Recalculate Armor
    GetTarget()->UpdateArmor();
}

/********************************/
/***      HEAL & ENERGIZE     ***/
/********************************/
void Aura::HandleAuraModTotalHealthPercentRegen(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;
}

void Aura::HandleAuraModTotalManaPercentRegen(bool apply, bool /*Real*/)
{
    if (m_modifier.periodictime == 0)
        m_modifier.periodictime = 1000;

    m_periodicTimer = m_modifier.periodictime;
    m_isPeriodic = apply;

    if (GetId() == 30024 && !apply && m_removeMode == AURA_REMOVE_BY_DEFAULT) // Shade of Aran drink on interrupt
    {
        Unit* target = GetTarget();
        UnitAI* ai = GetTarget()->AI();
        if (ai && target->GetTypeId() == TYPEID_UNIT)
            ai->SendAIEvent(AI_EVENT_CUSTOM_A, target, static_cast<Creature*>(target));
    }
}

void Aura::HandleModRegen(bool apply, bool /*Real*/)        // eating
{
    if (m_modifier.periodictime == 0)
        m_modifier.periodictime = 5000;

    m_periodicTimer = 5000;
    m_isPeriodic = apply;
}

void Aura::HandleModPowerRegen(bool apply, bool Real)       // drinking
{
    if (!Real)
        return;

    Powers powerType = GetTarget()->GetPowerType();
    if (m_modifier.periodictime == 0)
    {
        // Anger Management (only spell use this aura for rage)
        if (powerType == POWER_RAGE)
            m_modifier.periodictime = 3000;
        else
            m_modifier.periodictime = 2000;
    }

    m_periodicTimer = 5000;

    if (GetTarget()->GetTypeId() == TYPEID_PLAYER && m_modifier.m_miscvalue == POWER_MANA)
        ((Player*)GetTarget())->UpdateManaRegen();

    m_isPeriodic = apply;
}

void Aura::HandleModPowerRegenPCT(bool /*apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Update manaregen value
    if (m_modifier.m_miscvalue == POWER_MANA)
        ((Player*)GetTarget())->UpdateManaRegen();
}

void Aura::HandleModManaRegen(bool /*apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Note: an increase in regen does NOT cause threat.
    ((Player*)GetTarget())->UpdateManaRegen();
}

void Aura::HandleComprehendLanguage(bool apply, bool /*Real*/)
{
    if (apply)
        GetTarget()->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_COMPREHEND_LANG);
    else
        GetTarget()->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_COMPREHEND_LANG);
}

void Aura::HandleAuraModIncreaseHealth(bool apply, bool Real)
{
    Unit* target = GetTarget();

    switch (GetId())
    {
        // Special case with temporary increase max/current health
        // Cases where we need to manually calculate the amount for the spell (by percentage)
        // recalculate to full amount at apply for proper remove
        case 54443:                                         // Demonic Empowerment (Voidwalker)
        case 55233:                                         // Vampiric Blood
        case 61254:                                         // Will of Sartharion (Obsidian Sanctum)
            if (Real && apply)
                m_modifier.m_amount = target->GetMaxHealth() * m_modifier.m_amount / 100;
        // no break here

        // Cases where m_amount already has the correct value (spells cast with CastCustomSpell or absolute values)
        case 12976:                                         // Warrior Last Stand triggered spell (Cast with percentage-value by CastCustomSpell)
        case 28726:                                         // Nightmare Seed
        case 31616:                                         // Nature's Guardian (Cast with percentage-value by CastCustomSpell)
        case 34511:                                         // Valor (Bulwark of Kings, Bulwark of the Ancient Kings)
        case 44055: case 55915: case 55917: case 67596:     // Tremendous Fortitude (Battlemaster's Alacrity)
        case 50322:                                         // Survival Instincts (Cast with percentage-value by CastCustomSpell)
        case 53479:                                         // Hunter pet - Last Stand (Cast with percentage-value by CastCustomSpell)
        case 59465:                                         // Brood Rage (Ahn'Kahet)
        {
            if (Real)
            {
                if (apply)
                {
                    target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_amount), apply);
                    target->ModifyHealth(m_modifier.m_amount);
                }
                else
                {
                    if (m_removeMode != AURA_REMOVE_BY_DEATH)
                    {
                        if (int32(target->GetHealth()) > m_modifier.m_amount)
                            target->ModifyHealth(-m_modifier.m_amount);
                        else
                            target->SetHealth(1);
                    }

                    target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_amount), apply);
                }
            }
            return;
        }
        case 30421:
        {
            if (m_removeMode != AURA_REMOVE_BY_GAINED_STACK)
                target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, float(30000 + m_modifier.m_amount), apply);
            else
                target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_recentAmount), apply);
            if (apply)
                target->SetHealth(target->GetMaxHealth());
            else if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                target->CastSpell(target, 38637, TRIGGERED_OLD_TRIGGERED);
            return;
        }
        // generic case
        default:
            if (m_removeMode != AURA_REMOVE_BY_GAINED_STACK)
                target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_amount), apply);
            else
                target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_recentAmount), apply);
            break;
    }
}

void  Aura::HandleAuraModIncreaseMaxHealth(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();
    uint32 oldhealth = target->GetHealth();
    double healthPercentage = (double)oldhealth / (double)target->GetMaxHealth();

    target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_amount), apply);

    // refresh percentage
    if (oldhealth > 0)
    {
        uint32 newhealth = uint32(ceil((double)target->GetMaxHealth() * healthPercentage));
        if (newhealth == 0)
            newhealth = 1;

        target->SetHealth(newhealth);
    }
}

void Aura::HandleAuraModIncreaseEnergy(bool apply, bool Real)
{
    Unit* target = GetTarget();
    Powers powerType = target->GetPowerType();
    if (int32(powerType) != m_modifier.m_miscvalue)
        return;

    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + powerType);

    // Special case with temporary increase max/current power (percent)
    if (GetId() == 64904)                                   // Hymn of Hope
    {
        if (Real)
        {
            uint32 val = target->GetPower(powerType);
            target->HandleStatModifier(unitMod, TOTAL_PCT, float(m_modifier.m_amount), apply);
            target->SetPower(powerType, apply ? val * (100 + m_modifier.m_amount) / 100 : val * 100 / (100 + m_modifier.m_amount));
        }
        return;
    }

    // generic flat case
    target->HandleStatModifier(unitMod, TOTAL_VALUE, float(m_removeMode == AURA_REMOVE_BY_GAINED_STACK ? m_modifier.m_recentAmount : m_modifier.m_amount), apply);
}

void Aura::HandleAuraModIncreaseEnergyPercent(bool apply, bool /*Real*/)
{
    Powers powerType = GetTarget()->GetPowerType();
    if (int32(powerType) != m_modifier.m_miscvalue)
        return;

    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + powerType);

    GetTarget()->HandleStatModifier(unitMod, TOTAL_PCT, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModIncreaseHealthPercent(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_PCT, float(m_modifier.m_amount), apply);

    // spell special cases when current health set to max value at apply
    switch (GetId())
    {
        case 60430:                                         // Molten Fury
        case 64193:                                         // Heartbreak
        case 65737:                                         // Heartbreak
            target->SetHealth(target->GetMaxHealth());
            break;
        default:
            break;
    }
}

void Aura::HandleAuraIncreaseBaseHealthPercent(bool apply, bool /*Real*/)
{
    GetTarget()->HandleStatModifier(UNIT_MOD_HEALTH, BASE_PCT, float(m_modifier.m_amount), apply);
}

/********************************/
/***          FIGHT           ***/
/********************************/

void Aura::HandleAuraModParryPercent(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
    {
        target->m_modParryChance += (apply ? m_modifier.m_amount : -m_modifier.m_amount);
        return;
    }

    ((Player*)target)->UpdateParryPercentage();
}

void Aura::HandleAuraModDodgePercent(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
    {
        target->m_modDodgeChance += (apply ? m_modifier.m_amount : -m_modifier.m_amount);
        return;
    }

    ((Player*)target)->UpdateDodgePercentage();
    // sLog.outError("BONUS DODGE CHANCE: + %f", float(m_modifier.m_amount));
}

void Aura::HandleAuraModBlockPercent(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
    {
        target->m_modBlockChance += (apply ? m_modifier.m_amount : -m_modifier.m_amount);
        return;
    }

    ((Player*)target)->UpdateBlockPercentage();
    // sLog.outError("BONUS BLOCK CHANCE: + %f", float(m_modifier.m_amount));
}

void Aura::HandleAuraModRegenInterrupt(bool /*apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)GetTarget())->UpdateManaRegen();
}

void Aura::HandleAuraModCritPercent(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
    {
        for (float& i : target->m_modCritChance)
            i += (apply ? m_modifier.m_amount : -m_modifier.m_amount);
        return;
    }

    // apply item specific bonuses for already equipped weapon
    if (Real)
    {
        for (int i = 0; i < MAX_ATTACK; ++i)
            if (Item* pItem = ((Player*)target)->GetWeaponForAttack(WeaponAttackType(i), true, false))
                ((Player*)target)->_ApplyWeaponDependentAuraCritMod(pItem, WeaponAttackType(i), this, apply);
    }

    // mods must be applied base at equipped weapon class and subclass comparison
    // with spell->EquippedItemClass and  EquippedItemSubClassMask and EquippedItemInventoryTypeMask
    // m_modifier.m_miscvalue comparison with item generated damage types

    if (GetSpellProto()->EquippedItemClass == -1)
    {
        ((Player*)target)->HandleBaseModValue(CRIT_PERCENTAGE,         FLAT_MOD, float(m_modifier.m_amount), apply);
        ((Player*)target)->HandleBaseModValue(OFFHAND_CRIT_PERCENTAGE, FLAT_MOD, float(m_modifier.m_amount), apply);
        ((Player*)target)->HandleBaseModValue(RANGED_CRIT_PERCENTAGE,  FLAT_MOD, float(m_modifier.m_amount), apply);
    }
    else
    {
        // done in Player::_ApplyWeaponDependentAuraMods
    }
}

void Aura::HandleModHitChance(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)target)->UpdateMeleeHitChances();
        ((Player*)target)->UpdateRangedHitChances();
    }
    else
    {
        target->m_modMeleeHitChance += (apply ? m_modifier.m_amount : -m_modifier.m_amount);
        target->m_modRangedHitChance += (apply ? m_modifier.m_amount : -m_modifier.m_amount);
    }
}

void Aura::HandleModSpellHitChance(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)GetTarget())->UpdateSpellHitChances();
    }
    else
    {
        GetTarget()->m_modSpellHitChance += (apply ? m_modifier.m_amount : -m_modifier.m_amount);
    }
}

void Aura::HandleModSpellCritChance(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (target->GetTypeId() == TYPEID_UNIT)
    {
        for (uint8 school = SPELL_SCHOOL_NORMAL; school < MAX_SPELL_SCHOOL; ++school)
            target->m_modSpellCritChance[school] += (apply ? m_modifier.m_amount : -m_modifier.m_amount);
        return;
    }

    ((Player*)target)->UpdateAllSpellCritChances();
}

void Aura::HandleModSpellCritChanceShool(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    for (uint8 school = SPELL_SCHOOL_NORMAL; school < MAX_SPELL_SCHOOL; ++school)
    {
        if (m_modifier.m_miscvalue & (1 << school))
        {
            if (target->GetTypeId() == TYPEID_UNIT)
                target->m_modSpellCritChance[school] += (apply ? m_modifier.m_amount : -m_modifier.m_amount);
            else
                ((Player*)target)->UpdateSpellCritChance(school);
        }
    }
}

/********************************/
/***         ATTACK SPEED     ***/
/********************************/

void Aura::HandleModCastingSpeed(bool apply, bool /*Real*/)
{
    GetTarget()->ApplyCastTimePercentMod(float(m_modifier.m_amount), apply);
}

void Aura::HandleModMeleeRangedSpeedPct(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();
    target->ApplyAttackTimePercentMod(BASE_ATTACK, float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(OFF_ATTACK, float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(RANGED_ATTACK, float(m_modifier.m_amount), apply);
}

void Aura::HandleModCombatSpeedPct(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();
    target->ApplyCastTimePercentMod(float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(BASE_ATTACK, float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(OFF_ATTACK, float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(RANGED_ATTACK, float(m_modifier.m_amount), apply);
}

void Aura::HandleModAttackSpeed(bool apply, bool /*Real*/)
{
    GetTarget()->ApplyAttackTimePercentMod(BASE_ATTACK, float(m_modifier.m_amount), apply);
}

void Aura::HandleModMeleeSpeedPct(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();
    target->ApplyAttackTimePercentMod(BASE_ATTACK, float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(OFF_ATTACK, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModRangedHaste(bool apply, bool /*Real*/)
{
    GetTarget()->ApplyAttackTimePercentMod(RANGED_ATTACK, float(m_modifier.m_amount), apply);
}

void Aura::HandleRangedAmmoHaste(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;
    GetTarget()->ApplyAttackTimePercentMod(RANGED_ATTACK, float(m_modifier.m_amount), apply);
}

/********************************/
/***        ATTACK POWER      ***/
/********************************/

void Aura::HandleAuraModAttackPower(bool apply, bool /*Real*/)
{
    GetTarget()->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModRangedAttackPower(bool apply, bool /*Real*/)
{
    if ((GetTarget()->getClassMask() & CLASSMASK_WAND_USERS) != 0)
        return;

    GetTarget()->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModAttackPowerPercent(bool apply, bool /*Real*/)
{
    // UNIT_FIELD_ATTACK_POWER_MULTIPLIER = multiplier - 1
    GetTarget()->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModRangedAttackPowerPercent(bool apply, bool /*Real*/)
{
    if ((GetTarget()->getClassMask() & CLASSMASK_WAND_USERS) != 0)
        return;

    // UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER = multiplier - 1
    GetTarget()->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_PCT, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModRangedAttackPowerOfStatPercent(bool /*apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    // Recalculate bonus
    if (GetTarget()->GetTypeId() == TYPEID_PLAYER && !(GetTarget()->getClassMask() & CLASSMASK_WAND_USERS))
        ((Player*)GetTarget())->UpdateAttackPowerAndDamage(true);
}

void Aura::HandleAuraModAttackPowerOfStatPercent(bool /*apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    // Recalculate bonus
    if (GetTarget()->GetTypeId() == TYPEID_PLAYER)
        ((Player*)GetTarget())->UpdateAttackPowerAndDamage(false);
}

void Aura::HandleAuraModAttackPowerOfArmor(bool /*apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    // Recalculate bonus
    if (GetTarget()->GetTypeId() == TYPEID_PLAYER)
        ((Player*)GetTarget())->UpdateAttackPowerAndDamage(false);
}
/********************************/
/***        DAMAGE BONUS      ***/
/********************************/
void Aura::HandleModDamageDone(bool apply, bool Real)
{
    Unit* target = GetTarget();

    // apply item specific bonuses for already equipped weapon
    if (Real && target->GetTypeId() == TYPEID_PLAYER)
    {
        for (int i = 0; i < MAX_ATTACK; ++i)
            if (Item* pItem = ((Player*)target)->GetWeaponForAttack(WeaponAttackType(i), true, false))
                ((Player*)target)->_ApplyWeaponDependentAuraDamageMod(pItem, WeaponAttackType(i), this, apply);
    }

    // m_modifier.m_miscvalue is bitmask of spell schools
    // 1 ( 0-bit ) - normal school damage (SPELL_SCHOOL_MASK_NORMAL)
    // 126 - full bitmask all magic damages (SPELL_SCHOOL_MASK_MAGIC) including wands
    // 127 - full bitmask any damages
    //
    // mods must be applied base at equipped weapon class and subclass comparison
    // with spell->EquippedItemClass and  EquippedItemSubClassMask and EquippedItemInventoryTypeMask
    // m_modifier.m_miscvalue comparison with item generated damage types

    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL) != 0)
    {
        // apply generic physical damage bonuses including wand case
        if (GetSpellProto()->EquippedItemClass == -1 || target->GetTypeId() != TYPEID_PLAYER)
        {
            target->HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_VALUE, float(m_modifier.m_amount), apply);
            target->HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_VALUE, float(m_modifier.m_amount), apply);
            target->HandleStatModifier(UNIT_MOD_DAMAGE_RANGED, TOTAL_VALUE, float(m_modifier.m_amount), apply);
        }
        else
        {
            // done in Player::_ApplyWeaponDependentAuraMods
        }

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            if (m_positive)
                target->ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS, m_modifier.m_amount, apply);
            else
                target->ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG, m_modifier.m_amount, apply);
        }
    }

    // Skip non magic case for speedup
    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_MAGIC) == 0)
        return;

    if (GetSpellProto()->EquippedItemClass != -1 || GetSpellProto()->EquippedItemInventoryTypeMask != 0)
    {
        // wand magic case (skip generic to all item spell bonuses)
        // done in Player::_ApplyWeaponDependentAuraMods

        // Skip item specific requirements for not wand magic damage
        return;
    }

    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        if (m_positive)
        {
            for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
            {
                if ((m_modifier.m_miscvalue & (1 << i)) != 0)
                    target->ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, m_modifier.m_amount, apply);
            }
        }
        else
        {
            for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
            {
                if ((m_modifier.m_miscvalue & (1 << i)) != 0)
                    target->ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i, m_modifier.m_amount, apply);
            }
        }
        Pet* pet = target->GetPet();
        if (pet)
            pet->UpdateAttackPowerAndDamage();
    }
}

void Aura::HandleModDamagePercentDone(bool apply, bool Real)
{
    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "AURA MOD DAMAGE type:%u negative:%u", m_modifier.m_miscvalue, m_positive ? 0 : 1);
    Unit* target = GetTarget();

    // apply item specific bonuses for already equipped weapon
    if (Real && target->GetTypeId() == TYPEID_PLAYER)
    {
        for (int i = 0; i < MAX_ATTACK; ++i)
            if (Item* pItem = ((Player*)target)->GetWeaponForAttack(WeaponAttackType(i), true, false))
                ((Player*)target)->_ApplyWeaponDependentAuraDamageMod(pItem, WeaponAttackType(i), this, apply);
    }

    // m_modifier.m_miscvalue is bitmask of spell schools
    // 1 ( 0-bit ) - normal school damage (SPELL_SCHOOL_MASK_NORMAL)
    // 126 - full bitmask all magic damages (SPELL_SCHOOL_MASK_MAGIC) including wand
    // 127 - full bitmask any damages
    //
    // mods must be applied base at equipped weapon class and subclass comparison
    // with spell->EquippedItemClass and  EquippedItemSubClassMask and EquippedItemInventoryTypeMask
    // m_modifier.m_miscvalue comparison with item generated damage types

    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL) != 0)
    {
        // apply generic physical damage bonuses including wand case
        if (GetSpellProto()->EquippedItemClass == -1 || target->GetTypeId() != TYPEID_PLAYER)
        {
            target->HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, float(m_modifier.m_amount), apply);
            target->HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT, float(m_modifier.m_amount), apply);
            target->HandleStatModifier(UNIT_MOD_DAMAGE_RANGED, TOTAL_PCT, float(m_modifier.m_amount), apply);
        }
        else
        {
            // done in Player::_ApplyWeaponDependentAuraMods
        }
        // For show in client
        if (target->GetTypeId() == TYPEID_PLAYER)
            target->ApplyModSignedFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT, m_modifier.m_amount / 100.0f, apply);
    }

    // Skip non magic case for speedup
    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_MAGIC) == 0)
        return;

    if (GetSpellProto()->EquippedItemClass != -1 || GetSpellProto()->EquippedItemInventoryTypeMask != 0)
    {
        // wand magic case (skip generic to all item spell bonuses)
        // done in Player::_ApplyWeaponDependentAuraMods

        // Skip item specific requirements for not wand magic damage
        return;
    }

    // Magic damage percent modifiers implemented in Unit::SpellDamageBonusDone
    // Send info to client
    if (target->GetTypeId() == TYPEID_PLAYER)
        for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
            target->ApplyModSignedFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + i, m_modifier.m_amount / 100.0f, apply);

    if (!apply && m_removeMode == AURA_REMOVE_BY_EXPIRE)
        if (GetId() == 30423)
            target->CastSpell(target, 38639, TRIGGERED_OLD_TRIGGERED);
}

void Aura::HandleModOffhandDamagePercent(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "AURA MOD OFFHAND DAMAGE");

    GetTarget()->HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT, float(m_modifier.m_amount), apply);
}

/********************************/
/***        POWER COST        ***/
/********************************/

void Aura::HandleModPowerCostPCT(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    float amount = (m_removeMode == AURA_REMOVE_BY_GAINED_STACK ? m_modifier.m_recentAmount : m_modifier.m_amount) / 100.0f;
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
        if (m_modifier.m_miscvalue & (1 << i))
            target->ApplyModSignedFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + i, amount, apply);

    if (!apply && m_removeMode == AURA_REMOVE_BY_EXPIRE)
        if (GetId() == 30422)
            target->CastSpell(target, 38638, TRIGGERED_OLD_TRIGGERED);
}

void Aura::HandleModPowerCost(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
        if (m_modifier.m_miscvalue & (1 << i))
            GetTarget()->ApplyModInt32Value(UNIT_FIELD_POWER_COST_MODIFIER + i, m_modifier.m_amount, apply);
}

void Aura::HandleNoReagentUseAura(bool /*Apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;
    Unit* target = GetTarget();
    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    ClassFamilyMask mask;
    Unit::AuraList const& noReagent = target->GetAurasByType(SPELL_AURA_NO_REAGENT_USE);
    for (auto i : noReagent)
        mask |= i->GetAuraSpellClassMask();

    target->SetUInt64Value(PLAYER_NO_REAGENT_COST_1 + 0, mask.Flags);
    target->SetUInt32Value(PLAYER_NO_REAGENT_COST_1 + 2, mask.Flags2);
}

/*********************************************************/
/***                    OTHERS                         ***/
/*********************************************************/

void Aura::HandleShapeshiftBoosts(bool apply)
{
    uint32 spellId1 = 0;
    uint32 spellId2 = 0;
    uint32 HotWSpellId = 0;
    uint32 MasterShaperSpellId = 0;

    ShapeshiftForm form = ShapeshiftForm(GetModifier()->m_miscvalue);

    Unit* target = GetTarget();

    switch (form)
    {
        case FORM_CAT:
            spellId1 = 3025;
            HotWSpellId = 24900;
            MasterShaperSpellId = 48420;
            break;
        case FORM_TREE:
            spellId1 = 5420;
            spellId2 = 34123;
            MasterShaperSpellId = 48422;
            break;
        case FORM_TRAVEL:
            spellId1 = 5419;
            break;
        case FORM_AQUA:
            spellId1 = 5421;
            break;
        case FORM_BEAR:
            spellId1 = 1178;
            spellId2 = 21178;
            HotWSpellId = 24899;
            MasterShaperSpellId = 48418;
            break;
        case FORM_DIREBEAR:
            spellId1 = 9635;
            spellId2 = 21178;
            HotWSpellId = 24899;
            MasterShaperSpellId = 48418;
            break;
        case FORM_BATTLESTANCE:
            spellId1 = 21156;
            break;
        case FORM_DEFENSIVESTANCE:
            spellId1 = 7376;
            break;
        case FORM_BERSERKERSTANCE:
            spellId1 = 7381;
            break;
        case FORM_MOONKIN:
            spellId1 = 24905;
            spellId2 = 69366;
            MasterShaperSpellId = 48421;
            break;
        case FORM_FLIGHT:
            spellId1 = 33948;
            spellId2 = 34764;
            break;
        case FORM_FLIGHT_EPIC:
            spellId1 = 40122;
            spellId2 = 40121;
            break;
        case FORM_METAMORPHOSIS:
            spellId1 = 54817;
            spellId2 = 54879;
            break;
        case FORM_SPIRITOFREDEMPTION:
            spellId1 = 27792;
            spellId2 = 27795;                               // must be second, this important at aura remove to prevent to early iterator invalidation.
            break;
        case FORM_SHADOW:
            spellId1 = 49868;

            if (target->GetTypeId() == TYPEID_PLAYER)     // Spell 49868 have same category as main form spell and share cooldown
                target->RemoveSpellCooldown(49868);
            break;
        case FORM_GHOSTWOLF:
            spellId1 = 67116;
            break;
        case FORM_AMBIENT:
        case FORM_GHOUL:
        case FORM_STEALTH:
        case FORM_CREATURECAT:
        case FORM_CREATUREBEAR:
        case FORM_STEVES_GHOUL:
        case FORM_THARONJA_SKELETON:
        case FORM_TEST_OF_STRENGTH:
        case FORM_BLB_PLAYER:
        case FORM_SHADOW_DANCE:
        case FORM_TEST:
        case FORM_ZOMBIE:
        case FORM_UNDEAD:
        case FORM_FRENZY:
        case FORM_NONE:
            break;
        default:
            break;
    }

    if (apply)
    {
        if (spellId1)
            target->CastSpell(target, spellId1, TRIGGERED_OLD_TRIGGERED, nullptr, this);
        if (spellId2)
            target->CastSpell(target, spellId2, TRIGGERED_OLD_TRIGGERED, nullptr, this);

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            const PlayerSpellMap& sp_list = ((Player*)target)->GetSpellMap();
            for (const auto& itr : sp_list)
            {
                if (itr.second.state == PLAYERSPELL_REMOVED) continue;
                if (itr.first == spellId1 || itr.first == spellId2) continue;
                SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(itr.first);
                if (!spellInfo || !IsNeedCastSpellAtFormApply(spellInfo, form))
                    continue;
                target->CastSpell(target, itr.first, TRIGGERED_OLD_TRIGGERED, nullptr, this);
            }
            // remove auras that do not require shapeshift, but are not active in this specific form (like Improved Barkskin)
            Unit::SpellAuraHolderMap& tAuras = target->GetSpellAuraHolderMap();
            for (Unit::SpellAuraHolderMap::iterator itr = tAuras.begin(); itr != tAuras.end();)
            {
                SpellEntry const* spellInfo = itr->second->GetSpellProto();
                if (itr->second->IsPassive() && spellInfo->HasAttribute(SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT)
                        && (spellInfo->StancesNot[0] & (1 << (form - 1))))
                {
                    target->RemoveAurasDueToSpell(itr->second->GetId());
                    itr = tAuras.begin();
                }
                else
                    ++itr;
            }

            // Master Shapeshifter
            if (MasterShaperSpellId)
            {
                Unit::AuraList const& ShapeShifterAuras = target->GetAurasByType(SPELL_AURA_DUMMY);
                for (auto ShapeShifterAura : ShapeShifterAuras)
                {
                    if (ShapeShifterAura->GetSpellProto()->SpellIconID == 2851)
                    {
                        int32 ShiftMod = ShapeShifterAura->GetModifier()->m_amount;
                        target->CastCustomSpell(target, MasterShaperSpellId, &ShiftMod, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
                        break;
                    }
                }
            }

            // Leader of the Pack
            if (((Player*)target)->HasSpell(17007))
            {
                SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(24932);
                if (spellInfo && spellInfo->Stances[0] & (1 << (form - 1)))
                    target->CastSpell(target, 24932, TRIGGERED_OLD_TRIGGERED, nullptr, this);
            }

            // Savage Roar
            if (form == FORM_CAT && ((Player*)target)->HasAura(52610))
                target->CastSpell(target, 62071, TRIGGERED_OLD_TRIGGERED);

            // Survival of the Fittest (Armor part)
            if (form == FORM_BEAR || form == FORM_DIREBEAR)
            {
                Unit::AuraList const& modAuras = target->GetAurasByType(SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE);
                for (auto modAura : modAuras)
                {
                    if (modAura->GetSpellProto()->SpellFamilyName == SPELLFAMILY_DRUID &&
                        modAura->GetSpellProto()->SpellIconID == 961)
                    {
                        int32 bp = modAura->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_2);
                        if (bp)
                            target->CastCustomSpell(target, 62069, &bp, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        break;
                    }
                }
            }

            // Improved Moonkin Form
            if (form == FORM_MOONKIN)
            {
                Unit::AuraList const& dummyAuras = target->GetAurasByType(SPELL_AURA_DUMMY);
                for (auto dummyAura : dummyAuras)
                {
                    if (dummyAura->GetSpellProto()->SpellFamilyName == SPELLFAMILY_DRUID &&
                        dummyAura->GetSpellProto()->SpellIconID == 2855)
                    {
                        uint32 spell_id = 0;
                        switch (dummyAura->GetId())
                        {
                            case 48384: spell_id = 50170; break; // Rank 1
                            case 48395: spell_id = 50171; break; // Rank 2
                            case 48396: spell_id = 50172; break; // Rank 3
                            default:
                                sLog.outError("Aura::HandleShapeshiftBoosts: Not handled rank of IMF (Spell: %u)", dummyAura->GetId());
                                break;
                        }

                        if (spell_id)
                            target->CastSpell(target, spell_id, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        break;
                    }
                }
            }

            // Heart of the Wild
            if (HotWSpellId)
            {
                Unit::AuraList const& mModTotalStatPct = target->GetAurasByType(SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE);
                for (auto i : mModTotalStatPct)
                {
                    if (i->GetSpellProto()->SpellIconID == 240 && i->GetModifier()->m_miscvalue == 3)
                    {
                        int32 HotWMod = i->GetModifier()->m_amount;
                        if (GetModifier()->m_miscvalue == FORM_CAT)
                            HotWMod /= 2;

                        target->CastCustomSpell(target, HotWSpellId, &HotWMod, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        break;
                    }
                }
            }
        }
    }
    else
    {
        if (spellId1)
            target->RemoveAurasDueToSpell(spellId1);
        if (spellId2)
            target->RemoveAurasDueToSpell(spellId2);
        if (MasterShaperSpellId)
            target->RemoveAurasDueToSpell(MasterShaperSpellId);

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            // re-apply passive spells that don't need shapeshift but were inactive in current form:
            const PlayerSpellMap& sp_list = ((Player*)target)->GetSpellMap();
            for (const auto& itr : sp_list)
            {
                if (itr.second.state == PLAYERSPELL_REMOVED) continue;
                if (itr.first == spellId1 || itr.first == spellId2) continue;
                SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(itr.first);
                if (!spellInfo || !IsPassiveSpell(spellInfo))
                    continue;
                if (spellInfo->HasAttribute(SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT) && (spellInfo->StancesNot[0] & (1 << (form - 1))))
                    target->CastSpell(target, itr.first, TRIGGERED_OLD_TRIGGERED, nullptr, this);
            }
        }

        Unit::SpellAuraHolderMap& tAuras = target->GetSpellAuraHolderMap();
        for (Unit::SpellAuraHolderMap::iterator itr = tAuras.begin(); itr != tAuras.end();)
        {
            if (itr->second->IsRemovedOnShapeLost())
            {
                target->RemoveAurasDueToSpell(itr->second->GetId());
                itr = tAuras.begin();
            }
            else
                ++itr;
        }
    }
}

void Aura::HandleAuraEmpathy(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    // This aura is expected to only work with CREATURE_TYPE_BEAST or players
    CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(target->GetEntry());
    if (target->GetTypeId() == TYPEID_PLAYER || (target->GetTypeId() == TYPEID_UNIT && ci && ci->CreatureType == CREATURE_TYPE_BEAST))
        target->ApplyModUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_SPECIALINFO, apply);
}

void Aura::HandleAuraUntrackable(bool apply, bool /*Real*/)
{
    if (apply)
        GetTarget()->SetByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_UNTRACKABLE);
    else
        GetTarget()->RemoveByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_UNTRACKABLE);
}

void Aura::HandleAuraModPacify(bool apply, bool /*Real*/)
{
    if (apply)
        GetTarget()->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED);
    else
        GetTarget()->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED);
}

void Aura::HandleAuraModPacifyAndSilence(bool apply, bool Real)
{
    HandleAuraModPacify(apply, Real);
    HandleAuraModSilence(apply, Real);
}

void Aura::HandleAuraGhost(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* player = (Player*)GetTarget();

    if (apply)
    {
        player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST);
        if (!player->HasAuraType(SPELL_AURA_WATER_WALK))
            player->SetWaterWalk(true);
    }
    else
    {
        player->RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST);
        if (!player->HasAuraType(SPELL_AURA_WATER_WALK))
            player->SetWaterWalk(false);
    }

    if (player->GetGroup())
        player->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_STATUS);
}

void Aura::HandleAuraAllowFlight(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    GetTarget()->SetCanFly(apply);
}

void Aura::HandleModRating(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    for (uint32 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
        if (m_modifier.m_miscvalue & (1 << rating))
            ((Player*)GetTarget())->ApplyRatingMod(CombatRating(rating), m_modifier.m_amount, apply);
}

void Aura::HandleModRatingFromStat(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;
    // Just recalculate ratings
    for (uint32 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
        if (m_modifier.m_miscvalue & (1 << rating))
            ((Player*)GetTarget())->ApplyRatingMod(CombatRating(rating), 0, apply);
}

void Aura::HandleForceMoveForward(bool apply, bool Real)
{
    if (!Real)
        return;

    if (apply)
        GetTarget()->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FORCE_MOVE);
    else
        GetTarget()->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FORCE_MOVE);
}

void Aura::HandleAuraModExpertise(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)GetTarget())->UpdateExpertise(BASE_ATTACK);
    ((Player*)GetTarget())->UpdateExpertise(OFF_ATTACK);
}

void Aura::HandleModTargetResistance(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;
    Unit* target = GetTarget();
    // applied to damage as HandleNoImmediateEffect in Unit::CalculateAbsorbAndResist and Unit::CalcArmorReducedDamage
    // show armor penetration
    if (target->GetTypeId() == TYPEID_PLAYER && (m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL))
        target->ApplyModInt32Value(PLAYER_FIELD_MOD_TARGET_PHYSICAL_RESISTANCE, m_modifier.m_amount, apply);

    // show as spell penetration only full spell penetration bonuses (all resistances except armor and holy
    if (target->GetTypeId() == TYPEID_PLAYER && (m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_SPELL) == SPELL_SCHOOL_MASK_SPELL)
        target->ApplyModInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE, m_modifier.m_amount, apply);
}

void Aura::HandleShieldBlockValue(bool apply, bool /*Real*/)
{
    BaseModType modType = FLAT_MOD;
    if (m_modifier.m_auraname == SPELL_AURA_MOD_SHIELD_BLOCKVALUE_PCT)
        modType = PCT_MOD;

    if (GetTarget()->GetTypeId() == TYPEID_PLAYER)
        ((Player*)GetTarget())->HandleBaseModValue(SHIELD_BLOCK_VALUE, modType, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraRetainComboPoints(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* target = (Player*)GetTarget();

    // combo points was added in SPELL_EFFECT_ADD_COMBO_POINTS handler
    // remove only if aura expire by time (in case combo points amount change aura removed without combo points lost)
    if (!apply && m_removeMode == AURA_REMOVE_BY_EXPIRE && target->GetComboTargetGuid())
        if (Unit* unit = ObjectAccessor::GetUnit(*GetTarget(), target->GetComboTargetGuid()))
            target->AddComboPoints(unit, -m_modifier.m_amount);
}

void Aura::HandleModUnattackable(bool Apply, bool Real)
{
    if (Real && Apply)
    {
        GetTarget()->CombatStop();
        GetTarget()->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);
    }
    GetTarget()->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE_2, Apply);
}

void Aura::HandleSpiritOfRedemption(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    // prepare spirit state
    if (apply)
    {
        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            // disable breath/etc timers
            ((Player*)target)->StopMirrorTimers();

            // set stand state (expected in this form)
            if (!target->IsStandState())
                target->SetStandState(UNIT_STAND_STATE_STAND);
        }

        // interrupt casting when entering Spirit of Redemption
        if (target->IsNonMeleeSpellCasted(false))
            target->InterruptNonMeleeSpells(false);

        // set health and mana to maximum
        target->SetHealth(target->GetMaxHealth());
        target->SetPower(POWER_MANA, target->GetMaxPower(POWER_MANA));
    }
    // die at aura end
    else
        target->DealDamage(target, target->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, GetSpellProto(), false);
}

void Aura::HandleSchoolAbsorb(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* caster = GetCaster();
    if (!caster)
        return;

    Unit* target = GetTarget();
    SpellEntry const* spellProto = GetSpellProto();
    if (apply)
    {
        // prevent double apply bonuses
        if (target->GetTypeId() != TYPEID_PLAYER || !((Player*)target)->GetSession()->PlayerLoading())
        {
            float DoneActualBenefit = 0.0f;
            switch (spellProto->SpellFamilyName)
            {
                case SPELLFAMILY_GENERIC:
                    // Stoicism
                    if (spellProto->Id == 70845)
                        DoneActualBenefit = caster->GetMaxHealth() * 0.20f;
                    break;
                case SPELLFAMILY_PRIEST:
                    // Power Word: Shield
                    if (spellProto->SpellFamilyFlags & uint64(0x0000000000000001))
                    {
                        //+80.68% from +spell bonus
                        DoneActualBenefit = caster->SpellBaseHealingBonusDone(GetSpellSchoolMask(spellProto)) * 0.8068f;
                        // Borrowed Time
                        Unit::AuraList const& borrowedTime = caster->GetAurasByType(SPELL_AURA_DUMMY);
                        for (auto itr : borrowedTime)
                        {
                            SpellEntry const* i_spell = itr->GetSpellProto();
                            if (i_spell->SpellFamilyName == SPELLFAMILY_PRIEST && i_spell->SpellIconID == 2899 && i_spell->EffectMiscValue[itr->GetEffIndex()] == 24)
                            {
                                DoneActualBenefit += DoneActualBenefit * itr->GetModifier()->m_amount / 100;
                                break;
                            }
                        }
                    }

                    break;
                case SPELLFAMILY_MAGE:
                    // Frost Ward, Fire Ward
                    if (spellProto->IsFitToFamilyMask(uint64(0x0000000000000108)))
                        //+10% from +spell bonus
                        DoneActualBenefit = caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(spellProto)) * 0.1f;
                    // Ice Barrier
                    else if (spellProto->IsFitToFamilyMask(uint64(0x0000000100000000)))
                        //+80.67% from +spell bonus
                        DoneActualBenefit = caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(spellProto)) * 0.8067f;
                    break;
                case SPELLFAMILY_WARLOCK:
                    // Shadow Ward
                    if (spellProto->IsFitToFamilyMask(uint64(0x0000000000000000), 0x00000040))
                        //+30% from +spell bonus
                        DoneActualBenefit = caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(spellProto)) * 0.30f;
                    break;
                case SPELLFAMILY_PALADIN:
                    // Sacred Shield
                    // (check not strictly needed, only Sacred Shield has SPELL_AURA_SCHOOL_ABSORB in SPELLFAMILY_PALADIN at this time)
                    if (spellProto->IsFitToFamilyMask(uint64(0x0008000000000000)))
                    {
                        // +75% from spell power
                        DoneActualBenefit = caster->SpellBaseHealingBonusDone(GetSpellSchoolMask(spellProto)) * 0.75f;
                    }
                    break;
                default:
                    break;
            }

            DoneActualBenefit *= caster->CalculateLevelPenalty(GetSpellProto());

            m_modifier.m_amount += (int32)DoneActualBenefit;
        }
    }
    else
    {
        if (spellProto->Id == 33810 && m_removeMode == AURA_REMOVE_BY_SHIELD_BREAK) // Rock Shell
        {
            caster->CastSpell(caster, 33811, TRIGGERED_OLD_TRIGGERED, nullptr, this);
            return;
        }
        if (caster &&
                // Power Word: Shield
                spellProto->SpellFamilyName == SPELLFAMILY_PRIEST && spellProto->Mechanic == MECHANIC_SHIELD &&
                (spellProto->SpellFamilyFlags & uint64(0x0000000000000001)) &&
                // completely absorbed or dispelled
                (m_removeMode == AURA_REMOVE_BY_SHIELD_BREAK || m_removeMode == AURA_REMOVE_BY_DISPEL))
        {
            Unit::AuraList const& vDummyAuras = caster->GetAurasByType(SPELL_AURA_DUMMY);
            for (auto vDummyAura : vDummyAuras)
            {
                SpellEntry const* vSpell = vDummyAura->GetSpellProto();

                // Rapture (main spell)
                if (vSpell->SpellFamilyName == SPELLFAMILY_PRIEST && vSpell->SpellIconID == 2894 && vSpell->Effect[EFFECT_INDEX_1])
                {
                    switch (vDummyAura->GetEffIndex())
                    {
                        case EFFECT_INDEX_0:
                        {
                            // energize caster
                            int32 manapct1000 = 5 * (vDummyAura->GetModifier()->m_amount + sSpellMgr.GetSpellRank(vSpell->Id));
                            int32 basepoints0 = caster->GetMaxPower(POWER_MANA) * manapct1000 / 1000;
                            caster->CastCustomSpell(caster, 47755, &basepoints0, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
                            break;
                        }
                        case EFFECT_INDEX_1:
                        {
                            // energize target
                            if (!roll_chance_i(vDummyAura->GetModifier()->m_amount) || caster->HasAura(63853))
                                break;

                            switch (target->GetPowerType())
                            {
                                case POWER_RUNIC_POWER:
                                    target->CastSpell(target, 63652, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                                    break;
                                case POWER_RAGE:
                                    target->CastSpell(target, 63653, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                                    break;
                                case POWER_MANA:
                                {
                                    int32 basepoints0 = target->GetMaxPower(POWER_MANA) * 2 / 100;
                                    target->CastCustomSpell(target, 63654, &basepoints0, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
                                    break;
                                }
                                case POWER_ENERGY:
                                    target->CastSpell(target, 63655, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                                    break;
                                default:
                                    break;
                            }

                            // cooldown aura
                            caster->CastSpell(caster, 63853, TRIGGERED_OLD_TRIGGERED);
                            break;
                        }
                        default:
                            sLog.outError("Changes in R-dummy spell???: effect 3");
                            break;
                    }
                }
            }
        }
    }
}

void Aura::PeriodicTick()
{
    Unit* target = GetTarget();
    // passive periodic trigger spells should not be updated when dead, only death persistent should
    if (!target->isAlive() && GetHolder()->IsPassive())
        return;

    SpellEntry const* spellProto = GetSpellProto();

    switch (m_modifier.m_auraname)
    {
        case SPELL_AURA_PERIODIC_DAMAGE:
        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
        {
            // don't damage target if not alive, possible death persistent effects
            if (!target->isAlive())
                return;

            Unit* pCaster = GetCaster();
            if (!pCaster)
                return;

            if (spellProto->Effect[GetEffIndex()] == SPELL_EFFECT_PERSISTENT_AREA_AURA &&
                    pCaster->SpellHitResult(target, spellProto, (1 << GetEffIndex()), false) != SPELL_MISS_NONE)
                return;

            // Check for immune (not use charges)
            if (target->IsImmuneToDamage(GetSpellSchoolMask(spellProto)))
            {
                pCaster->SendSpellOrDamageImmune(target, spellProto->Id);
                return;
            }

            // ignore non positive values (can be result apply spellmods to aura damage
            uint32 amount = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            uint32 pdamage;
            if (m_modifier.m_auraname == SPELL_AURA_PERIODIC_DAMAGE)
                pdamage = amount;
            else
                pdamage = uint32(target->GetMaxHealth() * amount / 100);

            // some auras remove at specific health level or more or have damage interactions
            switch (GetId())
            {
                case 43093: case 31956: case 38801:
                case 35321: case 38363: case 39215:
                case 48920:
                {
                    if (target->GetHealth() == target->GetMaxHealth())
                    {
                        target->RemoveAurasDueToSpell(GetId());
                        return;
                    }
                    break;
                }
                case 38772:
                {
                    uint32 percent =
                        GetEffIndex() < EFFECT_INDEX_2 && spellProto->Effect[GetEffIndex()] == SPELL_EFFECT_DUMMY ?
                        pCaster->CalculateSpellDamage(target, spellProto, SpellEffectIndex(GetEffIndex() + 1)) :
                        100;
                    if (target->GetHealth() * 100 >= target->GetMaxHealth() * percent)
                    {
                        target->RemoveAurasDueToSpell(GetId());
                        return;
                    }
                    break;
                }
                case 29964: // Dragons Breath
                {
                    target->CastSpell(nullptr, 29965, TRIGGERED_OLD_TRIGGERED);
                    break;
                }
                case 31258: // Death & Decay - Rage Winterchill
                    if (target->GetEntry() == 17772) // Only Jaina receives less damage
                        pdamage = uint32(target->GetMaxHealth() * 0.5f / 100);
                    break;
                default:
                    break;
            }

            uint32 absorb = 0;
            uint32 resist = 0;
            CleanDamage cleanDamage =  CleanDamage(0, BASE_ATTACK, MELEE_HIT_NORMAL);

            // SpellDamageBonus for magic spells
            if (spellProto->DmgClass == SPELL_DAMAGE_CLASS_NONE || spellProto->DmgClass == SPELL_DAMAGE_CLASS_MAGIC)
                pdamage = target->SpellDamageBonusTaken(pCaster, spellProto, pdamage, DOT, GetStackAmount());
            // MeleeDamagebonus for weapon based spells
            else
            {
                WeaponAttackType attackType = GetWeaponAttackType(spellProto);
                pdamage = target->MeleeDamageBonusTaken(pCaster, pdamage, attackType, SpellSchoolMask(spellProto->SchoolMask), spellProto, DOT, GetStackAmount());
            }

            // Curse of Agony damage-per-tick calculation
            if (spellProto->SpellFamilyName == SPELLFAMILY_WARLOCK && (spellProto->SpellFamilyFlags & uint64(0x0000000000000400)) && spellProto->SpellIconID == 544)
            {
                // 1..4 ticks, 1/2 from normal tick damage
                if (GetAuraTicks() <= 4)
                    pdamage = pdamage / 2;
                // 9..12 ticks, 3/2 from normal tick damage
                else if (GetAuraTicks() >= 9)
                    pdamage += (pdamage + 1) / 2;       // +1 prevent 0.5 damage possible lost at 1..4 ticks
                // 5..8 ticks have normal tick damage
            }

            // This method can modify pdamage
            bool isCrit = IsCritFromAbilityAura(pCaster, pdamage);

            // send critical in hit info for threat calculation
            if (isCrit)
                cleanDamage.hitOutCome = MELEE_HIT_CRIT;

            // only from players
            // FIXME: need use SpellDamageBonus instead?
            if (pCaster->GetTypeId() == TYPEID_PLAYER)
                pdamage -= target->GetResilienceRatingDamageReduction(pdamage, SpellDmgClass(spellProto->DmgClass), true);

            target->CalculateDamageAbsorbAndResist(pCaster, GetSpellSchoolMask(spellProto), DOT, pdamage, &absorb, &resist, IsReflectableSpell(spellProto), IsResistableSpell(spellProto));

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s attacked %s for %u dmg inflicted by %u abs is %u",
                              GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(), pdamage, GetId(), absorb);

            pCaster->DealDamageMods(target, pdamage, &absorb, DOT, spellProto);

            // Set trigger flag
            uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC; //  | PROC_FLAG_SUCCESSFUL_HARMFUL_SPELL_HIT;
            uint32 procVictim   = PROC_FLAG_ON_TAKE_PERIODIC;// | PROC_FLAG_TAKEN_HARMFUL_SPELL_HIT;
            uint32 procEx = isCrit ? PROC_EX_CRITICAL_HIT : PROC_EX_NORMAL_HIT;

            pdamage = (pdamage <= absorb + resist) ? 0 : (pdamage - absorb - resist);

            uint32 overkill = pdamage > target->GetHealth() ? pdamage - target->GetHealth() : 0;
            SpellPeriodicAuraLogInfo pInfo(this, pdamage, overkill, absorb, int32(resist), 0.0f, isCrit);
            target->SendPeriodicAuraLog(&pInfo);

            if (pdamage)
                procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;

            pCaster->DealDamage(target, pdamage, &cleanDamage, DOT, GetSpellSchoolMask(spellProto), spellProto, true);

            pCaster->ProcDamageAndSpell(ProcSystemArguments(target, procAttacker, procVictim, procEx, pdamage, BASE_ATTACK, spellProto));

            // Drain Soul (chance soul shard)
            if (pCaster->GetTypeId() == TYPEID_PLAYER && spellProto->SpellFamilyName == SPELLFAMILY_WARLOCK && spellProto->SpellFamilyFlags & uint64(0x0000000000004000))
            {
                // Only from non-grey units
                if (roll_chance_i(10) &&                                                                        // 1-2 from drain with final and without glyph, 0-1 from damage
                        ((Player*)pCaster)->isHonorOrXPTarget(target) &&                                             // Gain XP or Honor requirement
                        (target->GetTypeId() == TYPEID_UNIT && !((Creature*)target)->IsTappedBy((Player*)pCaster)))  // Tapped by player requirement
                {
                    pCaster->CastSpell(pCaster, 43836, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                }
            }
            break;
        }
        case SPELL_AURA_PERIODIC_LEECH:
        case SPELL_AURA_PERIODIC_HEALTH_FUNNEL:
        {
            // don't damage target if not alive, possible death persistent effects
            if (!target->isAlive())
                return;

            Unit* pCaster = GetCaster();
            if (!pCaster)
                return;

            if (!pCaster->isAlive())
                return;

            if (spellProto->Effect[GetEffIndex()] == SPELL_EFFECT_PERSISTENT_AREA_AURA &&
                    pCaster->SpellHitResult(target, spellProto, (1 << GetEffIndex()), false) != SPELL_MISS_NONE)
                return;

            // Check for immune
            if (target->IsImmuneToDamage(GetSpellSchoolMask(spellProto)))
            {
                pCaster->SendSpellOrDamageImmune(target, spellProto->Id);
                return;
            }

            uint32 absorb = 0;
            uint32 resist = 0;
            CleanDamage cleanDamage =  CleanDamage(0, BASE_ATTACK, MELEE_HIT_NORMAL);

            uint32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            pdamage = target->SpellDamageBonusTaken(pCaster, spellProto, pdamage, DOT, GetStackAmount());

            bool isCrit = IsCritFromAbilityAura(pCaster, pdamage);

            // send critical in hit info for threat calculation
            if (isCrit)
                cleanDamage.hitOutCome = MELEE_HIT_CRIT;

            // only from players
            // FIXME: need use SpellDamageBonus instead?
            if (GetCasterGuid().IsPlayer())
                pdamage -= target->GetResilienceRatingDamageReduction(pdamage, SpellDmgClass(spellProto->DmgClass), true);

            target->CalculateDamageAbsorbAndResist(pCaster, GetSpellSchoolMask(spellProto), DOT, pdamage, &absorb, &resist, IsReflectableSpell(spellProto), IsResistableSpell(spellProto));

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s health leech of %s for %u dmg inflicted by %u abs is %u",
                              GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(), pdamage, GetId(), absorb);

            pCaster->DealDamageMods(target, pdamage, &absorb, DOT, spellProto);

            pCaster->SendSpellNonMeleeDamageLog(target, GetId(), pdamage, GetSpellSchoolMask(spellProto), absorb, resist, true, 0, isCrit);

            float multiplier = spellProto->EffectMultipleValue[GetEffIndex()] > 0 ? spellProto->EffectMultipleValue[GetEffIndex()] : 1;

            // Set trigger flag
            uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC; //  | PROC_FLAG_SUCCESSFUL_HARMFUL_SPELL_HIT;
            uint32 procVictim   = PROC_FLAG_ON_TAKE_PERIODIC;// | PROC_FLAG_TAKEN_HARMFUL_SPELL_HIT;
            uint32 procEx = isCrit ? PROC_EX_CRITICAL_HIT : PROC_EX_NORMAL_HIT;

            pdamage = (pdamage <= absorb + resist) ? 0 : (pdamage - absorb - resist);
            if (pdamage)
                procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;

            int32 new_damage = pCaster->DealDamage(target, pdamage, &cleanDamage, DOT, GetSpellSchoolMask(spellProto), spellProto, false);
            pCaster->ProcDamageAndSpell(ProcSystemArguments(target, procAttacker, procVictim, procEx, pdamage, BASE_ATTACK, spellProto));

            if (!target->isAlive() && pCaster->IsNonMeleeSpellCasted(false))
                for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
                    if (Spell* spell = pCaster->GetCurrentSpell(CurrentSpellTypes(i)))
                        if (spell->m_spellInfo->Id == GetId())
                            spell->cancel();

            if (Player* modOwner = pCaster->GetSpellModOwner())
            {
                modOwner->ApplySpellMod(GetId(), SPELLMOD_ALL_EFFECTS, new_damage);
                modOwner->ApplySpellMod(GetId(), SPELLMOD_MULTIPLE_VALUE, multiplier);
            }

            int32 heal = pCaster->SpellHealingBonusTaken(pCaster, spellProto, int32(new_damage * multiplier), DOT, GetStackAmount());

            uint32 absorbHeal = 0;
            pCaster->CalculateHealAbsorb(heal, &absorbHeal);

            int32 gain = pCaster->DealHeal(pCaster, heal - absorbHeal, spellProto, false, absorbHeal);
            // Health Leech effects do not generate healing aggro
            if (m_modifier.m_auraname == SPELL_AURA_PERIODIC_LEECH)
                break;
            pCaster->getHostileRefManager().threatAssist(pCaster, gain * 0.5f * sSpellMgr.GetSpellThreatMultiplier(spellProto), spellProto);
            break;
        }
        case SPELL_AURA_PERIODIC_HEAL:
        case SPELL_AURA_OBS_MOD_HEALTH:
        {
            Unit* pCaster = GetCaster();
            if (!pCaster)
                return;

            // don't heal target if max health or if not alive, mostly death persistent effects from items
            if (!target->isAlive() || (target->GetHealth() == target->GetMaxHealth()))
                return;

            // heal for caster damage (must be alive)
            if (target != pCaster && spellProto->SpellVisual[0] == 163 && !pCaster->isAlive())
                return;

            if (target->IsImmuneToSchool(spellProto))
            {
                pCaster->SendSpellOrDamageImmune(target, spellProto->Id);
                return;
            }

            // ignore non positive values (can be result apply spellmods to aura damage
            uint32 amount = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            uint32 pdamage;

            if (m_modifier.m_auraname == SPELL_AURA_OBS_MOD_HEALTH)
                pdamage = uint32(target->GetMaxHealth() * amount / 100);
            else
            {
                pdamage = amount;

                // Wild Growth (1/7 - 6 + 2*ramainTicks) %
                if (spellProto->SpellFamilyName == SPELLFAMILY_DRUID && spellProto->SpellIconID == 2864)
                {
                    int32 ticks = GetAuraMaxTicks();
                    int32 remainingTicks = ticks - GetAuraTicks();
                    int32 addition = int32(amount) * ticks * (-6 + 2 * remainingTicks) / 100;

                    if (GetAuraTicks() != 1)
                        // Item - Druid T10 Restoration 2P Bonus
                        if (Aura* aura = pCaster->GetAura(70658, EFFECT_INDEX_0))
                            addition += abs(int32((addition * aura->GetModifier()->m_amount) / ((ticks - 1) * 100)));

                    pdamage = int32(pdamage) + addition;
                }
            }

            pdamage = target->SpellHealingBonusTaken(pCaster, spellProto, pdamage, DOT, GetStackAmount());

            // This method can modify pdamage
            bool isCrit = IsCritFromAbilityAura(pCaster, pdamage);

            uint32 absorbHeal = 0;
            pCaster->CalculateHealAbsorb(pdamage, &absorbHeal);
            pdamage -= absorbHeal;

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s heal of %s for %u health  (absorbed %u) inflicted by %u",
                GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(), pdamage, absorbHeal, GetId());

            int32 gain = target->ModifyHealth(pdamage);
            SpellPeriodicAuraLogInfo pInfo(this, pdamage, (pdamage - uint32(gain)), absorbHeal, 0, 0.0f, isCrit);
            target->SendPeriodicAuraLog(&pInfo);

            // Set trigger flag
            uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC;
            uint32 procVictim = PROC_FLAG_ON_TAKE_PERIODIC;
            uint32 procEx = PROC_EX_INTERNAL_HOT | (isCrit ? PROC_EX_CRITICAL_HIT : PROC_EX_NORMAL_HIT);

            // add HoTs to amount healed in bgs
            if (pCaster->GetTypeId() == TYPEID_PLAYER)
                if (BattleGround* bg = ((Player*)pCaster)->GetBattleGround())
                    bg->UpdatePlayerScore(((Player*)pCaster), SCORE_HEALING_DONE, gain);

            if (pCaster->isInCombat() && !pCaster->IsIncapacitated())
                target->getHostileRefManager().threatAssist(pCaster, float(gain) * 0.5f * sSpellMgr.GetSpellThreatMultiplier(spellProto), spellProto);

            pCaster->ProcDamageAndSpell(ProcSystemArguments(target, procAttacker, procVictim, procEx, gain, BASE_ATTACK, spellProto, nullptr, gain));

            // apply damage part to caster if needed (ex. health funnel)
            if (target != pCaster && spellProto->SpellVisual[0] == 163)
            {
                uint32 damage = spellProto->manaPerSecond;
                uint32 absorb = 0;

                pCaster->DealDamageMods(pCaster, damage, &absorb, NODAMAGE, spellProto);
                if (pCaster->GetHealth() > damage)
                {
                    pCaster->SendSpellNonMeleeDamageLog(pCaster, GetId(), damage, GetSpellSchoolMask(spellProto), absorb, 0, true, 0, false);
                    CleanDamage cleanDamage = CleanDamage(0, BASE_ATTACK, MELEE_HIT_NORMAL);
                    pCaster->DealDamage(pCaster, damage, &cleanDamage, NODAMAGE, GetSpellSchoolMask(spellProto), spellProto, true);
                }
                else
                {
                    // cannot apply damage part so we have to cancel responsible aura
                    pCaster->RemoveAurasDueToSpell(GetId());

                    // finish current generic/channeling spells, don't affect autorepeat
                    pCaster->FinishSpell(CURRENT_GENERIC_SPELL);
                    pCaster->FinishSpell(CURRENT_CHANNELED_SPELL);
                }
            }
            break;
        }
        case SPELL_AURA_PERIODIC_MANA_LEECH:
        {
            // don't damage target if not alive, possible death persistent effects
            if (!target->isAlive())
                return;

            if (m_modifier.m_miscvalue < 0 || m_modifier.m_miscvalue >= MAX_POWERS)
                return;

            Powers power = Powers(m_modifier.m_miscvalue);

            // power type might have changed between aura applying and tick (druid's shapeshift)
            if (target->GetPowerType() != power)
                return;

            Unit* pCaster = GetCaster();
            if (!pCaster)
                return;

            if (!pCaster->isAlive())
                return;

            if (GetSpellProto()->Effect[GetEffIndex()] == SPELL_EFFECT_PERSISTENT_AREA_AURA &&
                    pCaster->SpellHitResult(target, spellProto, (1 << GetEffIndex()), false) != SPELL_MISS_NONE)
                return;

            // Check for immune (not use charges)
            if (target->IsImmuneToDamage(GetSpellSchoolMask(spellProto)))
            {
                pCaster->SendSpellOrDamageImmune(target, spellProto->Id);
                return;
            }

            // ignore non positive values (can be result apply spellmods to aura damage
            uint32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            // Special case: draining x% of mana (up to a maximum of 2*x% of the caster's maximum mana)
            // It's mana percent cost spells, m_modifier.m_amount is percent drain from target
            if (spellProto->ManaCostPercentage)
            {
                // max value
                uint32 maxmana = pCaster->GetMaxPower(power)  * pdamage * 2 / 100;
                pdamage = target->GetMaxPower(power) * pdamage / 100;
                if (pdamage > maxmana)
                    pdamage = maxmana;
            }

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s power leech of %s for %u dmg inflicted by %u",
                              GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(), pdamage, GetId());

            int32 drain_amount = target->GetPower(power) > pdamage ? pdamage : target->GetPower(power);

            drain_amount -= target->GetResilienceRatingDamageReduction(uint32(drain_amount), SpellDmgClass(spellProto->DmgClass), false, power);

            target->ModifyPower(power, -drain_amount);

            float gain_multiplier = 0.0f;

            if (pCaster->GetMaxPower(power) > 0)
            {
                gain_multiplier = spellProto->EffectMultipleValue[GetEffIndex()];

                if (Player* modOwner = pCaster->GetSpellModOwner())
                    modOwner->ApplySpellMod(GetId(), SPELLMOD_MULTIPLE_VALUE, gain_multiplier);
            }

            SpellPeriodicAuraLogInfo pInfo(this, drain_amount, 0, 0, 0, gain_multiplier);
            target->SendPeriodicAuraLog(&pInfo);

            if (int32 gain_amount = int32(drain_amount * gain_multiplier))
            {
                int32 gain = pCaster->ModifyPower(power, gain_amount);

                if (GetSpellProto()->IsFitToFamily(SPELLFAMILY_WARLOCK, 0x0000000000000010)) // Drain Mana
                    if (Aura* petPart = GetHolder()->GetAuraByEffectIndex(EFFECT_INDEX_1))
                        if (int pet_gain = gain_amount * petPart->GetModifier()->m_amount / 100)
                            pCaster->CastCustomSpell(pCaster, 32554, &pet_gain, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);

                target->AddThreat(pCaster, float(gain) * 0.5f, pInfo.critical, GetSpellSchoolMask(spellProto), spellProto);
            }

            // Some special cases
            switch (GetId())
            {
                case 32960:                                 // Mark of Kazzak
                {
                    if (target->GetTypeId() == TYPEID_PLAYER && target->GetPowerType() == POWER_MANA)
                    {
                        // Drain 5% of target's mana
                        pdamage = target->GetMaxPower(POWER_MANA) * 5 / 100;
                        drain_amount = target->GetPower(POWER_MANA) > pdamage ? pdamage : target->GetPower(POWER_MANA);
                        target->ModifyPower(POWER_MANA, -drain_amount);

                        SpellPeriodicAuraLogInfo info(this, drain_amount, 0, 0, 0, 0.0f);
                        target->SendPeriodicAuraLog(&info);
                    }
                    // no break here
                }
                case 21056:                                 // Mark of Kazzak
                case 31447:                                 // Mark of Kaz'rogal
                {
                    uint32 triggerSpell = 0;
                    switch (GetId())
                    {
                        case 21056: triggerSpell = 21058; break;
                        case 31447: triggerSpell = 31463; break;
                        case 32960: triggerSpell = 32961; break;
                    }
                    if (target->GetTypeId() == TYPEID_PLAYER && target->GetPower(power) == 0)
                    {
                        target->CastSpell(target, triggerSpell, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                        target->RemoveAurasDueToSpell(GetId());
                    }
                    break;
                }
            }
            break;
        }
        case SPELL_AURA_PERIODIC_ENERGIZE:
        {
            // don't energize target if not alive, possible death persistent effects
            if (!target->isAlive())
                return;

            Unit* pCaster = GetCaster();

            if (pCaster)
            {
                if (target->IsImmuneToSchool(spellProto))
                {
                    pCaster->SendSpellOrDamageImmune(target, spellProto->Id);
                    return;
                }
            }

            // ignore non positive values (can be result apply spellmods to aura damage
            uint32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s energize %s for %u dmg inflicted by %u",
                              GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(), pdamage, GetId());

            if (m_modifier.m_miscvalue < 0 || m_modifier.m_miscvalue >= MAX_POWERS)
                break;

            Powers power = Powers(m_modifier.m_miscvalue);

            if (target->GetMaxPower(power) == 0)
                break;

            SpellPeriodicAuraLogInfo pInfo(this, pdamage, 0, 0, 0, 0.0f);
            target->SendPeriodicAuraLog(&pInfo);

            int32 gain = target->ModifyPower(power, pdamage);

            if (pCaster)
                target->getHostileRefManager().threatAssist(pCaster, float(gain) * 0.5f * sSpellMgr.GetSpellThreatMultiplier(spellProto), spellProto);
            break;
        }
        case SPELL_AURA_OBS_MOD_MANA:
        {
            // don't energize target if not alive, possible death persistent effects
            if (!target->isAlive())
                return;

            Unit* pCaster = GetCaster();

            if (pCaster)
            {
                if (target->IsImmuneToSchool(spellProto))
                {
                    pCaster->SendSpellOrDamageImmune(target, spellProto->Id);
                    return;
                }
            }

            // ignore non positive values (can be result apply spellmods to aura damage
            uint32 amount = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            uint32 pdamage = uint32(target->GetMaxPower(POWER_MANA) * amount / 100);

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s energize %s for %u mana inflicted by %u",
                              GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(), pdamage, GetId());

            if (target->GetMaxPower(POWER_MANA) == 0)
                break;

            SpellPeriodicAuraLogInfo pInfo(this, pdamage, 0, 0, 0, 0.0f);
            target->SendPeriodicAuraLog(&pInfo);

            int32 gain = target->ModifyPower(POWER_MANA, pdamage);

            if (pCaster)
                target->getHostileRefManager().threatAssist(pCaster, float(gain) * 0.5f * sSpellMgr.GetSpellThreatMultiplier(spellProto), spellProto);
            break;
        }
        case SPELL_AURA_POWER_BURN_MANA:
        {
            // don't mana burn target if not alive, possible death persistent effects
            if (!target->isAlive())
                return;

            Unit* pCaster = GetCaster();
            if (!pCaster)
                return;

            // Check for immune (not use charges)
            if (target->IsImmuneToDamage(GetSpellSchoolMask(spellProto)))
            {
                pCaster->SendSpellOrDamageImmune(target, spellProto->Id);
                return;
            }

            int32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            Powers powerType = Powers(m_modifier.m_miscvalue);

            if (!target->isAlive() || target->GetPowerType() != powerType)
                return;

            pdamage -= target->GetResilienceRatingDamageReduction(uint32(pdamage), SpellDmgClass(spellProto->DmgClass), false, powerType);

            uint32 gain = uint32(-target->ModifyPower(powerType, -pdamage));

            gain = uint32(gain * spellProto->EffectMultipleValue[GetEffIndex()]);

            // maybe has to be sent different to client, but not by SMSG_PERIODICAURALOG
            SpellNonMeleeDamage spellDamageInfo(pCaster, target, spellProto->Id, SpellSchoolMask(spellProto->SchoolMask));
            spellDamageInfo.periodicLog = true;

            pCaster->CalculateSpellDamage(&spellDamageInfo, gain, spellProto);

            spellDamageInfo.target->CalculateAbsorbResistBlock(pCaster, &spellDamageInfo, spellProto);

            pCaster->DealDamageMods(spellDamageInfo.target, spellDamageInfo.damage, &spellDamageInfo.absorb, SPELL_DIRECT_DAMAGE, spellProto);

            pCaster->SendSpellNonMeleeDamageLog(&spellDamageInfo);

            // Set trigger flag
            uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC; //  | PROC_FLAG_SUCCESSFUL_HARMFUL_SPELL_HIT;
            uint32 procVictim   = PROC_FLAG_ON_TAKE_PERIODIC;// | PROC_FLAG_TAKEN_HARMFUL_SPELL_HIT;
            uint32 procEx       = createProcExtendMask(&spellDamageInfo, SPELL_MISS_NONE);
            if (spellDamageInfo.damage)
                procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;

            pCaster->DealSpellDamage(&spellDamageInfo, true);

            pCaster->ProcDamageAndSpell(ProcSystemArguments(spellDamageInfo.target, procAttacker, procVictim, procEx, spellDamageInfo.damage, BASE_ATTACK, spellProto));
            break;
        }
        case SPELL_AURA_MOD_REGEN:
        {
            // don't heal target if not alive, possible death persistent effects
            if (!target->isAlive())
                return;

            int32 gain = target->ModifyHealth(m_modifier.m_amount);
            if (Unit* caster = GetCaster())
                target->getHostileRefManager().threatAssist(caster, float(gain) * 0.5f  * sSpellMgr.GetSpellThreatMultiplier(spellProto), spellProto);
            break;
        }
        case SPELL_AURA_MOD_POWER_REGEN:
        {
            // don't energize target if not alive, possible death persistent effects
            if (!target->isAlive())
                return;

            Powers powerType = target->GetPowerType();
            if (int32(powerType) != m_modifier.m_miscvalue)
                return;

            if (spellProto->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
            {
                // eating anim
                target->HandleEmoteCommand(EMOTE_ONESHOT_EAT);
            }
            else if (GetId() == 20577)
            {
                // cannibalize anim
                target->HandleEmoteCommand(EMOTE_STATE_CANNIBALIZE);
            }

            // Anger Management
            // amount = 1+ 16 = 17 = 3,4*5 = 10,2*5/3
            // so 17 is rounded amount for 5 sec tick grow ~ 1 range grow in 3 sec
            if (powerType == POWER_RAGE && target->isInCombat())
                target->ModifyPower(powerType, m_modifier.m_amount * 3 / 5);
            // Butchery
            else if (powerType == POWER_RUNIC_POWER && target->isInCombat())
                target->ModifyPower(powerType, m_modifier.m_amount);
            break;
        }
        // Here tick dummy auras
        case SPELL_AURA_DUMMY:                              // some spells have dummy aura
        case SPELL_AURA_PERIODIC_DUMMY:
        {
            PeriodicDummyTick();
            break;
        }
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
        {
            TriggerSpell();
            break;
        }
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE:
        {
            TriggerSpellWithValue();
            break;
        }
        default:
            break;
    }
}

void Aura::PeriodicDummyTick()
{
    SpellEntry const* spell = GetSpellProto();
    Unit* target = GetTarget();
    switch (spell->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (spell->Id)
            {
                // Forsaken Skills
                case 7054:
                {
                    // Possibly need cast one of them (but
                    // 7038 Forsaken Skill: Swords
                    // 7039 Forsaken Skill: Axes
                    // 7040 Forsaken Skill: Daggers
                    // 7041 Forsaken Skill: Maces
                    // 7042 Forsaken Skill: Staves
                    // 7043 Forsaken Skill: Bows
                    // 7044 Forsaken Skill: Guns
                    // 7045 Forsaken Skill: 2H Axes
                    // 7046 Forsaken Skill: 2H Maces
                    // 7047 Forsaken Skill: 2H Swords
                    // 7048 Forsaken Skill: Defense
                    // 7049 Forsaken Skill: Fire
                    // 7050 Forsaken Skill: Frost
                    // 7051 Forsaken Skill: Holy
                    // 7053 Forsaken Skill: Shadow
                    return;
                }
                case 7057:                                  // Haunting Spirits
                    if (roll_chance_i(33))
                        target->CastSpell(target, m_modifier.m_amount, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
//              // Panda
//              case 19230: break;
                case 21094:                                 // Separation Anxiety (Majordomo Executus)
                case 23487:                                 // Separation Anxiety (Garr)
                    if (Unit* caster = GetCaster())
                    {
                        float m_radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(spell->EffectRadiusIndex[m_effIndex]));
                        if (caster->isAlive() && !caster->IsWithinDistInMap(target, m_radius))
                            target->CastSpell(target, (spell->Id == 21094 ? 21095 : 23492), TRIGGERED_OLD_TRIGGERED, nullptr);      // Spell 21095: Separation Anxiety for Majordomo Executus' adds, 23492: Separation Anxiety for Garr's adds
                    }
                    return;
                case 27769:                                 // Whisper Gulch: Yogg-Saron Whisper
                {
                    if (roll_chance_i(20))
                        target->CastSpell(nullptr, 29072, TRIGGERED_OLD_TRIGGERED);
                    return;
                }
//              // Gossip NPC Periodic - Talk
                case 32441:                                 // Brittle Bones
                    if (roll_chance_i(33))
                        target->CastSpell(target, 32437, true, nullptr, this);  // Rattled
                    return;
//              case 33208: break;
//              // Gossip NPC Periodic - Despawn
//              case 33209: break;
//              // Steal Weapon
//              case 36207: break;
//              // Simon Game START timer, (DND)
//              case 39993: break;
//              // Knockdown Fel Cannon: break; The Aggro Burst
//              case 40119: break;
//              // Old Mount Spell
//              case 40154: break;
//              // Magnetic Pull
//              case 40581: break;
//              // Ethereal Ring: break; The Bolt Burst
//              case 40801: break;
//              // Crystal Prison
//              case 40846: break;
//              // Copy Weapon
//              case 41054: break;
//              // Dementia
//              case 41404: break;
//              // Ethereal Ring Visual, Lightning Aura
//              case 41477: break;
//              // Ethereal Ring Visual, Lightning Aura (Fork)
//              case 41525: break;
//              // Ethereal Ring Visual, Lightning Jumper Aura
//              case 41567: break;
//              // No Man's Land
//              case 41955: break;
//              // Headless Horseman - Fire
//              case 42074: break;
//              // Headless Horseman - Visual - Large Fire
//              case 42075: break;
//              // Headless Horseman - Start Fire, Periodic Aura
//              case 42140: break;
//              // Ram Speed Boost
//              case 42152: break;
//              // Headless Horseman - Fires Out Victory Aura
//              case 42235: break;
//              // Pumpkin Life Cycle
//              case 42280: break;
//              // Brewfest Request Chick Chuck Mug Aura
//              case 42537: break;
//              // Squashling
//              case 42596: break;
//              // Headless Horseman Climax, Head: Periodic
//              case 42603: break;
                case 42621:                                 // Fire Bomb
                {
                    // Cast the summon spells (42622 to 42627) with increasing chance
                    uint32 rand = urand(0, 99);
                    for (uint32 i = 1; i <= 6; ++i)
                    {
                        if (rand < i * (i + 1) / 2 * 5)
                        {
                            target->CastSpell(target, spell->Id + i, TRIGGERED_OLD_TRIGGERED);
                            break;
                        }
                    }
                    return;
                }
//              // Headless Horseman - Conflagrate, Periodic Aura
//              case 42637: break;
//              // Headless Horseman - Create Pumpkin Treats Aura
//              case 42774: break;
//              // Headless Horseman Climax - Summoning Rhyme Aura
//              case 42879: break;
                case 42919:                                 // Tricky Treat
                    target->CastSpell(target, 42966, TRIGGERED_OLD_TRIGGERED);
                    return;
//              // Giddyup!
//              case 42924: break;
//              // Ram - Trot
//              case 42992: break;
//              // Ram - Canter
//              case 42993: break;
//              // Ram - Gallop
//              case 42994: break;
//              // Ram Level - Neutral
//              case 43310: break;
//              // Headless Horseman - Maniacal Laugh, Maniacal, Delayed 17
//              case 43884: break;
//              // Wretched!
//              case 43963: break;
//              // Headless Horseman - Maniacal Laugh, Maniacal, other, Delayed 17
//              case 44000: break;
//              // Energy Feedback
//              case 44328: break;
//              // Romantic Picnic
//              case 45102: break;
//              // Romantic Picnic
//              case 45123: break;
//              // Looking for Love
//              case 45124: break;
//              // Kite - Lightning Strike Kite Aura
//              case 45197: break;
//              // Rocket Chicken
//              case 45202: break;
//              // Copy Offhand Weapon
//              case 45205: break;
//              // Upper Deck - Kite - Lightning Periodic Aura
//              case 45207: break;
//              // Kite -Sky  Lightning Strike Kite Aura
//              case 45251: break;
//              // Ribbon Pole Dancer Check Aura
//              case 45390: break;
//              // Holiday - Midsummer, Ribbon Pole Periodic Visual
//              case 45406: break;
//              // Parachute
//              case 45472: break;
//              // Alliance Flag, Extra Damage Debuff
//              case 45898: break;
//              // Horde Flag, Extra Damage Debuff
//              case 45899: break;
//              // Ahune - Summoning Rhyme Aura
//              case 45926: break;
//              // Ahune - Slippery Floor
//              case 45945: break;
//              // Ahune's Shield
//              case 45954: break;
//              // Nether Vapor Lightning
//              case 45960: break;
//              // Darkness
//              case 45996: break;
                case 46041:                                 // Summon Blood Elves Periodic
                    target->CastSpell(target, 46037, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    target->CastSpell(target, roll_chance_i(50) ? 46038 : 46039, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    target->CastSpell(target, 46040, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
//              // Transform Visual Missile Periodic
//              case 46205: break;
//              // Find Opening Beam End
//              case 46333: break;
                case 46371:                                 // Ice Spear Control Aura
                    target->CastSpell(target, 46372, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
//              // Hailstone Chill
//              case 46458: break;
//              // Hailstone Chill, Internal
//              case 46465: break;
//              // Chill, Internal Shifter
//              case 46549: break;
//              // Summon Ice Spear Knockback Delayer
//              case 46878: break;
//              // Burninate Effect
//              case 47214: break;
//              // Fizzcrank Practice Parachute
//              case 47228: break;
//              // Send Mug Control Aura
//              case 47369: break;
//              // Direbrew's Disarm (precast)
//              case 47407: break;
//              // Mole Machine Port Schedule
//              case 47489: break;
//              case 47941: break; // Crystal Spike
//              case 48200: break; // Healer Aura
                case 48630:                                 // Summon Gauntlet Mobs Periodic
                case 59275:                                 // Below may need some adjustment, pattern for amount of summon and where is not verified 100% (except for odd/even tick)
                {
                    bool chance = roll_chance_i(50);

                    target->CastSpell(target, chance ? 48631 : 48632, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                    if (GetAuraTicks() % 2)                 // which doctor at odd tick
                        target->CastSpell(target, chance ? 48636 : 48635, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    else                                    // or harponeer, at even tick
                        target->CastSpell(target, chance ? 48634 : 48633, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                    return;
                }
//              case 49313: break; // Proximity Mine Area Aura
//              // Mole Machine Portal Schedule
//              case 49466: break;
                case 49555:                                 // Corpse Explode (Drak'tharon Keep - Trollgore)
                case 59807:                                 // Corpse Explode (heroic)
                {
                    if (GetAuraTicks() == 3 && target->GetTypeId() == TYPEID_UNIT)
                        ((Creature*)target)->ForcedDespawn();
                    if (GetAuraTicks() != 2)
                        return;

                    if (Unit* pCaster = GetCaster())
                        pCaster->CastSpell(target, spell->Id == 49555 ? 49618 : 59809, TRIGGERED_OLD_TRIGGERED);

                    return;
                }
//              case 49592: break; // Temporal Rift
//              case 49957: break; // Cutting Laser
//              case 50085: break; // Slow Fall
//              // Listening to Music
//              case 50493: break; // TODO: Implement
//              // Love Rocket Barrage
//              case 50530: break;
                case 50789:                                 // Summon iron dwarf (left or right)
                case 59860:
                    target->CastSpell(target, roll_chance_i(50) ? 50790 : 50791, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                case 50792:                                 // Summon iron trogg (left or right)
                case 59859:
                    target->CastSpell(target, roll_chance_i(50) ? 50793 : 50794, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                case 50801:                                 // Summon malformed ooze (left or right)
                case 59858:
                    target->CastSpell(target, roll_chance_i(50) ? 50802 : 50803, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                case 50824:                                 // Summon earthen dwarf
                    target->CastSpell(target, roll_chance_i(50) ? 50825 : 50826, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                case 52441:                                 // Cool Down
                    target->CastSpell(target, 52443, TRIGGERED_OLD_TRIGGERED);
                    return;
                case 53035:                                 // Summon Anub'ar Champion Periodic (Azjol Nerub)
                case 53036:                                 // Summon Anub'ar Necromancer Periodic (Azjol Nerub)
                case 53037:                                 // Summon Anub'ar Crypt Fiend Periodic (Azjol Nerub)
                {
                    uint32 summonSpells[3][3] =
                    {
                        {53090, 53014, 53064},              // Summon Anub'ar Champion
                        {53092, 53015, 53066},              // Summon Anub'ar Necromancer
                        {53091, 53016, 53065}               // Summon Anub'ar Crypt Fiend
                    };

                    // Cast different spell depending on trigger position
                    // This will summon a different npc entry on each location - each of those has individual movement patern
                    if (target->GetPositionZ() < 750.0f)
                        target->CastSpell(target, summonSpells[spell->Id - 53035][0], TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    else if (target->GetPositionX() > 500.0f)
                        target->CastSpell(target, summonSpells[spell->Id - 53035][1], TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    else
                        target->CastSpell(target, summonSpells[spell->Id - 53035][2], TRIGGERED_OLD_TRIGGERED, nullptr, this);

                    return;
                }
                case 53520:                                 // Carrion Beetles
                    target->CastSpell(target, 53521, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    target->CastSpell(target, 53521, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                case 55592:                                 // Clean
                    switch (urand(0, 2))
                    {
                        case 0: target->CastSpell(target, 55731, TRIGGERED_OLD_TRIGGERED); break;
                        case 1: target->CastSpell(target, 55738, TRIGGERED_OLD_TRIGGERED); break;
                        case 2: target->CastSpell(target, 55739, TRIGGERED_OLD_TRIGGERED); break;
                    }
                    return;
                case 61968:                                 // Flash Freeze
                {
                    if (GetAuraTicks() == 1 && !target->HasAura(62464))
                        target->CastSpell(target, 61970, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                }
                case 62018:                                 // Collapse
                {
                    // lose 1% of health every second
                    target->DealDamage(target, target->GetMaxHealth() * .01, nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NONE, nullptr, false);
                    return;
                }
                case 62019:                                 // Rune of Summoning
                {
                    target->CastSpell(target, 62020, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                }
                case 62038:                                 // Biting Cold
                {
                    if (target->GetTypeId() != TYPEID_PLAYER)
                        return;

                    // if player is moving remove one aura stack
                    if (target->IsMoving())
                        target->RemoveAuraHolderFromStack(62039);
                    // otherwise add one aura stack each 3 seconds
                    else if (GetAuraTicks() % 3 && !target->HasAura(62821))
                        target->CastSpell(target, 62039, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                }
                case 62039:                                 // Biting Cold
                {
                    target->CastSpell(target, 62188, TRIGGERED_OLD_TRIGGERED);
                    return;
                }
                case 62566:                                 // Healthy Spore Summon Periodic
                {
                    target->CastSpell(target, 62582, TRIGGERED_OLD_TRIGGERED);
                    target->CastSpell(target, 62591, TRIGGERED_OLD_TRIGGERED);
                    target->CastSpell(target, 62592, TRIGGERED_OLD_TRIGGERED);
                    target->CastSpell(target, 62593, TRIGGERED_OLD_TRIGGERED);
                    return;
                }
                case 62717:                                 // Slag Pot
                {
                    target->CastSpell(target, target->GetMap()->IsRegularDifficulty() ? 65722 : 65723, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                    // cast Slag Imbued if the target survives up to the last tick
                    if (GetAuraTicks() == 10)
                        target->CastSpell(target, 63536, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                }
                case 63050:                                 // Sanity
                {
                    if (GetHolder()->GetStackAmount() <= 25 && !target->HasAura(63752))
                        target->CastSpell(target, 63752, TRIGGERED_OLD_TRIGGERED);
                    else if (GetHolder()->GetStackAmount() > 25 && target->HasAura(63752))
                        target->RemoveAurasDueToSpell(63752);
                    return;
                }
                case 63382:                                 // Rapid Burst
                {
                    if (GetAuraTicks() % 2)
                        target->CastSpell(target, target->GetMap()->IsRegularDifficulty() ? 64019 : 64532, TRIGGERED_OLD_TRIGGERED);
                    else
                        target->CastSpell(target, target->GetMap()->IsRegularDifficulty() ? 63387 : 64531, TRIGGERED_OLD_TRIGGERED);
                    return;
                }
                case 64101:                                 // Defend
                {
                    target->CastSpell(target, 62719, TRIGGERED_OLD_TRIGGERED);
                    target->CastSpell(target, 64192, TRIGGERED_OLD_TRIGGERED);
                    return;
                }
                case 64217:                                 // Overcharged
                {
                    if (GetHolder()->GetStackAmount() >= 10)
                    {
                        target->CastSpell(target, 64219, TRIGGERED_OLD_TRIGGERED);
                        target->DealDamage(target, target->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
                    }
                    return;
                }
                case 64412:                                 // Phase Punch
                {
                    if (SpellAuraHolder* phaseAura = target->GetSpellAuraHolder(64412))
                    {
                        uint32 uiAuraId = 0;
                        switch (phaseAura->GetStackAmount())
                        {
                            case 1: uiAuraId = 64435; break;
                            case 2: uiAuraId = 64434; break;
                            case 3: uiAuraId = 64428; break;
                            case 4: uiAuraId = 64421; break;
                            case 5: uiAuraId = 64417; break;
                        }

                        if (uiAuraId && !target->HasAura(uiAuraId))
                        {
                            target->CastSpell(target, uiAuraId, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                            // remove original aura if phased
                            if (uiAuraId == 64417)
                            {
                                target->RemoveAurasDueToSpell(64412);
                                target->CastSpell(target, 62169, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                            }
                        }
                    }
                    return;
                }
                case 65272:                                 // Shatter Chest
                {
                    target->CastSpell(target, 62501, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                }
                case 66118:                                 // Leeching Swarm
                case 67630:                                 // Leeching Swarm
                case 68646:                                 // Leeching Swarm
                case 68647:                                 // Leeching Swarm
                {
                    Unit* caster = GetCaster();
                    if (!caster)
                        return;

                    int32 lifeLeeched = int32(target->GetHealth() * m_modifier.m_amount * 0.01f);

                    if (lifeLeeched < 250)
                        lifeLeeched = 250;

                    // Leeching swarm damage
                    caster->CastCustomSpell(target, 66240, &lifeLeeched, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                    // Leeching swarm heal
                    target->CastCustomSpell(caster, 66125, &lifeLeeched, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);

                    return;
                }
                case 66798:                                 // Death's Respite
                {
                    Unit* caster = GetCaster();
                    if (!caster)
                        return;

                    caster->CastSpell(target, 66797, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    target->RemoveAurasDueToSpell(GetId());
                    return;
                }
                case 68875:                                 // Wailing Souls
                case 68876:                                 // Wailing Souls
                {
                    // Sweep around
                    float newAngle = target->GetOrientation();
                    if (spell->Id == 68875)
                        newAngle += 0.09f;
                    else
                        newAngle -= 0.09f;

                    newAngle = MapManager::NormalizeOrientation(newAngle);

                    target->SetFacingTo(newAngle);

                    // Should actually be SMSG_SPELL_START, too
                    target->CastSpell(target, 68873, TRIGGERED_OLD_TRIGGERED);
                    return;
                }
                case 70069:                                 // Ooze Flood Periodic Trigger
                {
                    target->CastSpell(target, GetSpellProto()->CalculateSimpleValue(m_effIndex), TRIGGERED_OLD_TRIGGERED);
                    return;
                }
// Exist more after, need add later
                default:
                    break;
            }

            // Drink (item drink spells)
            if (GetEffIndex() > EFFECT_INDEX_0 && spell->EffectApplyAuraName[GetEffIndex() - 1] == SPELL_AURA_MOD_POWER_REGEN)
            {
                if (target->GetTypeId() != TYPEID_PLAYER)
                    return;
                // Search SPELL_AURA_MOD_POWER_REGEN aura for this spell and add bonus
                if (Aura* aura = GetHolder()->GetAuraByEffectIndex(SpellEffectIndex(GetEffIndex() - 1)))
                {
                    aura->GetModifier()->m_amount = m_modifier.m_amount;
                    ((Player*)target)->UpdateManaRegen();
                    // Disable continue
                    m_isPeriodic = false;
                    return;
                }
                return;
            }

            // Prey on the Weak
            if (spell->SpellIconID == 2983)
            {
                Unit* victim = target->getVictim();
                if (victim && (target->GetHealth() * 100 / target->GetMaxHealth() > victim->GetHealth() * 100 / victim->GetMaxHealth()))
                {
                    if (!target->HasAura(58670))
                    {
                        int32 basepoints = GetBasePoints();
                        target->CastCustomSpell(target, 58670, &basepoints, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
                    }
                }
                else
                    target->RemoveAurasDueToSpell(58670);
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            switch (spell->Id)
            {
                case 55342:                                 // Mirror Image
                {
                    if (GetAuraTicks() != 1)
                        return;
                    if (Unit* pCaster = GetCaster())
                        pCaster->CastSpell(pCaster, spell->EffectTriggerSpell[GetEffIndex()], TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                }
                default:
                    break;
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            switch (spell->Id)
            {
                // Frenzied Regeneration
                case 22842:
                {
                    // Converts up to 10 rage per second into health for $d.  Each point of rage is converted into ${$m2/10}.1% of max health.
                    // Should be manauser
                    if (target->GetPowerType() != POWER_RAGE)
                        return;
                    uint32 rage = target->GetPower(POWER_RAGE);
                    // Nothing todo
                    if (rage == 0)
                        return;
                    int32 mod = (rage < 100) ? rage : 100;
                    int32 points = target->CalculateSpellDamage(target, spell, EFFECT_INDEX_1);
                    int32 regen = target->GetMaxHealth() * (mod * points / 10) / 1000;
                    target->CastCustomSpell(target, 22845, &regen, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    target->SetPower(POWER_RAGE, rage - mod);
                    return;
                }
                // Force of Nature
                case 33831:
                    return;
                default:
                    break;
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            switch (spell->Id)
            {
                // Killing Spree
                case 51690:
                {
                    if (target->hasUnitState(UNIT_STAT_STUNNED) || target->isFeared())
                        return;

                    UnitList targets;
                    {
                        // eff_radius ==0
                        float radius = GetSpellMaxRange(sSpellRangeStore.LookupEntry(spell->rangeIndex));

                        MaNGOS::AnyUnfriendlyVisibleUnitInObjectRangeCheck u_check(target, target, radius);
                        MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyVisibleUnitInObjectRangeCheck> checker(targets, u_check);
                        Cell::VisitAllObjects(target, checker, radius);
                    }

                    if (targets.empty())
                        return;

                    UnitList::const_iterator itr = targets.begin();
                    std::advance(itr, urand() % targets.size());
                    Unit* victim = *itr;

                    target->CastSpell(victim, 57840, TRIGGERED_OLD_TRIGGERED);
                    target->CastSpell(victim, 57841, TRIGGERED_OLD_TRIGGERED);
                    return;
                }
                default:
                    break;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            // Explosive Shot
            if (spell->SpellFamilyFlags & uint64(0x8000000000000000))
            {
                target->CastCustomSpell(target, 53352, &m_modifier.m_amount, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this, GetCasterGuid());
                return;
            }
            switch (spell->Id)
            {
                // Harpooner's Mark
                // case 40084:
                //    return;
                // Feeding Frenzy Rank 1 & 2
                case 53511:
                case 53512:
                {
                    Unit* victim = target->getVictim();
                    if (victim && victim->GetHealth() * 100 < victim->GetMaxHealth() * 35)
                        target->CastSpell(target, spell->Id == 53511 ? 60096 : 60097, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                    return;
                }
                default:
                    break;
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            // Astral Shift
            if (spell->Id == 52179)
            {
                // Periodic need for remove visual on stun/fear/silence lost
                if (!target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED | UNIT_FLAG_FLEEING | UNIT_FLAG_SILENCED))
                    target->RemoveAurasDueToSpell(52179);
                return;
            }
            break;
        }
        case SPELLFAMILY_DEATHKNIGHT:
        {
            // Death and Decay
            if (spell->SpellFamilyFlags & uint64(0x0000000000000020))
            {
                if (Unit* caster = GetCaster())
                    caster->CastCustomSpell(target, 52212, &m_modifier.m_amount, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
            // Raise Dead
//            if (spell->SpellFamilyFlags & uint64(0x0000000000001000))
//                return;
            // Chains of Ice
            if (spell->SpellFamilyFlags & uint64(0x0000400000000000))
            {
                // Get 0 effect aura
                Aura* slow = target->GetAura(GetId(), EFFECT_INDEX_0);
                if (slow)
                {
                    slow->ApplyModifier(false, true);
                    Modifier* mod = slow->GetModifier();
                    mod->m_amount += m_modifier.m_amount;
                    if (mod->m_amount > 0) mod->m_amount = 0;
                    slow->ApplyModifier(true, true);
                }
                return;
            }
            // Summon Gargoyle
//            if (spell->SpellFamilyFlags & uint64(0x0000008000000000))
//                return;
            // Death Rune Mastery
//            if (spell->SpellFamilyFlags & uint64(0x0000000000004000))
//                return;
            // Bladed Armor
            if (spell->SpellIconID == 2653)
            {
                // Increases your attack power by $s1 for every $s2 armor value you have.
                // Calculate AP bonus (from 1 efect of this spell)
                int32 apBonus = m_modifier.m_amount * target->GetArmor() / target->CalculateSpellDamage(target, spell, EFFECT_INDEX_1);
                target->CastCustomSpell(target, 61217, &apBonus, &apBonus, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, this);
                return;
            }
            // Reaping
//            if (spell->SpellIconID == 22)
//                return;
            // Blood of the North
//            if (spell->SpellIconID == 30412)
//                return;
            // Hysteria
            if (spell->SpellFamilyFlags & uint64(0x0000000020000000))
            {
                // damage not expected to be show in logs, not any damage spell related to damage apply
                uint32 deal = m_modifier.m_amount * target->GetMaxHealth() / 100;
                target->DealDamage(target, deal, nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
                return;
            }
            break;
        }
        default:
            break;
    }

    if (Unit* caster = GetCaster())
    {
        if (target && target->GetTypeId() == TYPEID_UNIT)
            sScriptDevAIMgr.OnEffectDummy(caster, GetId(), GetEffIndex(), (Creature*)target, ObjectGuid());
    }
}

void Aura::HandlePreventFleeing(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit::AuraList const& fearAuras = GetTarget()->GetAurasByType(SPELL_AURA_MOD_FEAR);
    if (!fearAuras.empty())
    {
        const Aura* first = fearAuras.front();
        if (apply)
            GetTarget()->SetFeared(false, first->GetCasterGuid());
        else
            GetTarget()->SetFeared(true, first->GetCasterGuid(), first->GetId());
    }
}

void Aura::HandleManaShield(bool apply, bool Real)
{
    if (!Real)
        return;

    // prevent double apply bonuses
    if (apply && (GetTarget()->GetTypeId() != TYPEID_PLAYER || !((Player*)GetTarget())->GetSession()->PlayerLoading()))
    {
        if (Unit* caster = GetCaster())
        {
            float DoneActualBenefit = 0.0f;
            switch (GetSpellProto()->SpellFamilyName)
            {
                case SPELLFAMILY_MAGE:
                    if (GetSpellProto()->SpellFamilyFlags & uint64(0x0000000000008000))
                    {
                        // Mana Shield
                        // +50% from +spd bonus
                        DoneActualBenefit = caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(GetSpellProto())) * 0.5f;
                        break;
                    }
                    break;
                default:
                    break;
            }

            DoneActualBenefit *= caster->CalculateLevelPenalty(GetSpellProto());

            m_modifier.m_amount += (int32)DoneActualBenefit;
        }
    }
}

void Aura::HandleArenaPreparation(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    target->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PREPARATION, apply);

    if (apply)
    {
        // max regen powers at start preparation
        target->SetHealth(target->GetMaxHealth());
        target->SetPower(POWER_MANA, target->GetMaxPower(POWER_MANA));
        target->SetPower(POWER_ENERGY, target->GetMaxPower(POWER_ENERGY));
    }
    else
    {
        // reset originally 0 powers at start/leave
        target->SetPower(POWER_RAGE, 0);
        target->SetPower(POWER_RUNIC_POWER, 0);
    }
}

/**
 * Such auras are applied from a caster(=player) to a vehicle.
 * This has been verified using spell #49256
 */
void Aura::HandleAuraControlVehicle(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();
    if (!target->IsVehicle())
        return;

    Unit* caster = GetCaster();
    if (!caster)
        return;

    if (apply)
    {
        //sLog.outString("Boarding %s %s on %s %s", caster->GetName(), caster->GetGuidStr().c_str(), target->GetName(), target->GetGuidStr().c_str());
        target->GetVehicleInfo()->Board(caster, GetBasePoints() - 1);
    }
    else
    {
        //sLog.outString("Unboarding %s %s from %s %s", caster->GetName(), caster->GetGuidStr().c_str(), target->GetName(), target->GetGuidStr().c_str());
        target->GetVehicleInfo()->UnBoard(caster, m_removeMode == AURA_REMOVE_BY_TRACKING);
    }
}

void Aura::HandleAuraAddMechanicAbilities(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (!target || target->GetTypeId() != TYPEID_PLAYER)    // only players should be affected by this aura
        return;

    uint16 i_OverrideSetId = GetMiscValue();

    const OverrideSpellDataEntry* spellSet = sOverrideSpellDataStore.LookupEntry(i_OverrideSetId);
    if (!spellSet)
        return;

    if (apply)
    {
        // spell give the player a new castbar with some spells.. this is a clientside process..
        // serverside just needs to register the new spells so that player isn't kicked as cheater
        for (unsigned int spellId : spellSet->Spells)
            if (spellId)
                static_cast<Player*>(target)->addSpell(spellId, true, false, false, false);

        target->SetUInt16Value(PLAYER_FIELD_BYTES2, 0, i_OverrideSetId);
    }
    else
    {
        target->SetUInt16Value(PLAYER_FIELD_BYTES2, 0, 0);
        for (unsigned int spellId : spellSet->Spells)
            if (spellId)
                static_cast<Player*>(target)->removeSpell(spellId, false, false, false);
    }
}

void Aura::HandleAuraOpenStable(bool apply, bool Real)
{
    if (!Real || GetTarget()->GetTypeId() != TYPEID_PLAYER || !GetTarget()->IsInWorld())
        return;

    Player* player = (Player*)GetTarget();

    if (apply)
        player->GetSession()->SendStablePet(player->GetObjectGuid());

    // client auto close stable dialog at !apply aura
}

void Aura::HandleAuraMirrorImage(bool apply, bool Real)
{
    if (!Real)
        return;

    // Target of aura should always be creature (ref Spell::CheckCast)
    Creature* pCreature = (Creature*)GetTarget();

    if (apply)
    {
        // Caster can be player or creature, the unit who pCreature will become an clone of.
        Unit* caster = GetCaster();

        if (caster->GetTypeId() == TYPEID_PLAYER)           // TODO - Verify! Does it take a 'pseudo-race' (from display-id) for creature-mirroring, and what is sent in SMSG_MIRRORIMAGE_DATA
            pCreature->SetByteValue(UNIT_FIELD_BYTES_0, 0, caster->getRace());

        pCreature->SetByteValue(UNIT_FIELD_BYTES_0, 1, caster->getClass());
        pCreature->SetByteValue(UNIT_FIELD_BYTES_0, 2, caster->getGender());
        pCreature->SetByteValue(UNIT_FIELD_BYTES_0, 3, caster->GetPowerType());

        pCreature->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_CLONED);

        pCreature->SetDisplayId(caster->GetNativeDisplayId());
    }
    else
    {
        const CreatureInfo* cinfo = pCreature->GetCreatureInfo();
        const CreatureModelInfo* minfo = sObjectMgr.GetCreatureModelInfo(pCreature->GetNativeDisplayId());

        pCreature->SetByteValue(UNIT_FIELD_BYTES_0, 0, 0);
        pCreature->SetByteValue(UNIT_FIELD_BYTES_0, 1, cinfo->UnitClass);
        pCreature->SetByteValue(UNIT_FIELD_BYTES_0, 2, minfo->gender);
        pCreature->SetByteValue(UNIT_FIELD_BYTES_0, 3, 0);

        pCreature->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_CLONED);

        pCreature->SetDisplayId(pCreature->GetNativeDisplayId());
    }
}

void Aura::HandleMirrorName(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* caster = GetCaster();
    Unit* target = GetTarget();

    if (!target || !caster || target->GetTypeId() != TYPEID_UNIT)
        return;

    if (apply)
        target->SetName(caster->GetName());
    else
    {
        CreatureInfo const* cinfo = ((Creature*)target)->GetCreatureInfo();
        if (!cinfo)
            return;

        target->SetName(cinfo->Name);
    }
}

void Aura::HandleAuraConvertRune(bool apply, bool Real)
{
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* plr = (Player*)GetTarget();

    if (plr->getClass() != CLASS_DEATH_KNIGHT)
        return;

    RuneType runeFrom = RuneType(GetSpellProto()->EffectMiscValue[m_effIndex]);
    RuneType runeTo   = RuneType(GetSpellProto()->EffectMiscValueB[m_effIndex]);

    if (apply)
    {
        for (uint32 i = 0; i < MAX_RUNES; ++i)
        {
            if (plr->GetCurrentRune(i) == runeFrom && !plr->GetRuneCooldown(i))
            {
                plr->ConvertRune(i, runeTo);
                break;
            }
        }
    }
    else
    {
        for (uint32 i = 0; i < MAX_RUNES; ++i)
        {
            if (plr->GetCurrentRune(i) == runeTo && plr->GetBaseRune(i) == runeFrom)
            {
                plr->ConvertRune(i, runeFrom);
                break;
            }
        }
    }
}

void Aura::HandlePhase(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    // always non stackable
    if (apply)
    {
        Unit::AuraList const& phases = target->GetAurasByType(SPELL_AURA_PHASE);
        if (!phases.empty())
            target->RemoveAurasDueToSpell(phases.front()->GetId(), GetHolder());
    }

    target->SetPhaseMask(apply ? GetMiscValue() : uint32(PHASEMASK_NORMAL), true);
    // no-phase is also phase state so same code for apply and remove
    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAuraMapBounds(GetId());
        if (saBounds.first != saBounds.second)
        {
            uint32 zone, area;
            target->GetZoneAndAreaId(zone, area);

            for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
                itr->second->ApplyOrRemoveSpellIfCan((Player*)target, zone, area, false);
        }
    }
}

void Aura::HandleAuraDetaunt(bool Apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* caster = GetCaster();

    if (!caster || !caster->isAlive() || !caster->CanHaveThreatList())
        return;

    caster->TauntUpdate();
}

void Aura::HandleAuraSafeFall(bool Apply, bool Real)
{
    // implemented in WorldSession::HandleMovementOpcodes

    // only special case
    if (Apply && Real && GetId() == 32474 && GetTarget()->GetTypeId() == TYPEID_PLAYER && GetHolder()->GetState() != SPELLAURAHOLDER_STATE_DB_LOAD)
        ((Player*)GetTarget())->ActivateTaxiPathTo(506, GetId()); // on DB load flight path is initiated on its own after its safe to do so
}

bool Aura::IsCritFromAbilityAura(Unit* caster, uint32& damage) const
{
    if (!GetSpellProto()->IsFitToFamily(SPELLFAMILY_ROGUE, uint64(0x100000)) && // Rupture
            !caster->HasAffectedAura(SPELL_AURA_ABILITY_PERIODIC_CRIT, GetSpellProto()))
        return false;

    if (caster->RollSpellCritOutcome(GetTarget(), GetSpellSchoolMask(GetSpellProto()), GetSpellProto()))
    {
        damage = caster->CalculateCritAmount(GetTarget(), damage, GetSpellProto());
        return true;
    }

    return false;
}

void Aura::HandleModTargetArmorPct(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)GetTarget())->UpdateArmorPenetration();
}

void Aura::HandleAuraModAllCritChance(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)target)->HandleBaseModValue(CRIT_PERCENTAGE,         FLAT_MOD, float(m_modifier.m_amount), apply);
    ((Player*)target)->HandleBaseModValue(OFFHAND_CRIT_PERCENTAGE, FLAT_MOD, float(m_modifier.m_amount), apply);
    ((Player*)target)->HandleBaseModValue(RANGED_CRIT_PERCENTAGE,  FLAT_MOD, float(m_modifier.m_amount), apply);

    // included in Player::UpdateSpellCritChance calculation
    ((Player*)target)->UpdateAllSpellCritChances();
}

void Aura::HandleAuraStopNaturalManaRegen(bool apply, bool Real)
{
    if (!Real)
        return;

    GetTarget()->ApplyModFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_REGENERATE_POWER, !apply && !GetTarget()->IsUnderLastManaUseEffect());
}

void Aura::HandleAuraSetVehicleId(bool apply, bool Real)
{
    if (!Real)
        return;

    GetTarget()->SetVehicleId(apply ? GetMiscValue() : 0, 0);
}

void Aura::HandlePreventResurrection(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();
    if (!target || target->GetTypeId() != TYPEID_PLAYER)
        return;

    if (apply)
        target->RemoveByteFlag(PLAYER_FIELD_BYTES, 0, PLAYER_FIELD_BYTE_RELEASE_TIMER);
    else if (!target->GetMap()->Instanceable())
        target->SetByteFlag(PLAYER_FIELD_BYTES, 0, PLAYER_FIELD_BYTE_RELEASE_TIMER);
}

void Aura::HandleFactionOverride(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();
    if (!target || !sFactionTemplateStore.LookupEntry(GetMiscValue()))
        return;

    if (apply)
        target->setFaction(GetMiscValue());
    else
        target->RestoreOriginalFaction();
}

void Aura::HandleTriggerLinkedAura(bool apply, bool Real)
{
    if (!Real)
        return;

    uint32 linkedSpell = GetSpellProto()->EffectTriggerSpell[m_effIndex];
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(linkedSpell);
    if (!spellInfo)
    {
        sLog.outError("Aura::HandleTriggerLinkedAura for spell %u effect %u triggering unknown spell id %u", GetSpellProto()->Id, m_effIndex, linkedSpell);
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {
        // ToDo: handle various cases where base points need to be applied!
        target->CastSpell(target, spellInfo, TRIGGERED_OLD_TRIGGERED, nullptr, this);
    }
    else
        target->RemoveAurasByCasterSpell(linkedSpell, GetCasterGuid());
}

bool Aura::IsLastAuraOnHolder()
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (i != GetEffIndex() && GetHolder()->m_auras[i])
            return false;
    return true;
}

bool Aura::HasMechanic(uint32 mechanic) const
{
    return GetSpellProto()->Mechanic == mechanic ||
           GetSpellProto()->EffectMechanic[m_effIndex] == mechanic;
}

inline bool IsRemovedOnShapeshiftLost(SpellEntry const* spellproto, ObjectGuid const& casterGuid, ObjectGuid const& targetGuid)
{
    if (casterGuid == targetGuid)
    {
        if (spellproto->Stances[0])
        {
            switch (spellproto->Id)
            {
                case 11327: // vanish stealth aura improvements are removed on stealth removal
                case 11329: // but they have attribute SPELL_ATTR_NOT_SHAPESHIFT
                case 26888: // maybe relic from when they had Effect1?
                    return true;
                default:
                    break;
            }

            if (!spellproto->HasAttribute(SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT) && !spellproto->HasAttribute(SPELL_ATTR_NOT_SHAPESHIFT))
                return true;
        }
        else if (spellproto->SpellFamilyName == SPELLFAMILY_DRUID && spellproto->EffectApplyAuraName[0] == SPELL_AURA_MOD_DODGE_PERCENT)
            return true;
    }

    return false;
    /*TODO: investigate spellid 24864  or (SpellFamilyName = 7 and EffectApplyAuraName_1 = 49 and stances = 0)*/
}

SpellAuraHolder::SpellAuraHolder(SpellEntry const* spellproto, Unit* target, WorldObject* caster, Item* castItem, SpellEntry const* triggeredBy) :
    m_spellProto(spellproto), m_target(target),
    m_castItemGuid(castItem ? castItem->GetObjectGuid() : ObjectGuid()), m_triggeredBy(triggeredBy),
    m_spellAuraHolderState(SPELLAURAHOLDER_STATE_CREATED), m_auraSlot(MAX_AURAS), m_auraFlags(AFLAG_NONE),
    m_auraLevel(1), m_procCharges(0),
    m_stackAmount(1), m_timeCla(1000), m_removeMode(AURA_REMOVE_BY_DEFAULT),
    m_AuraDRGroup(DIMINISHING_NONE), m_permanent(false), m_isRemovedOnShapeLost(true),
    m_deleted(false), m_skipUpdate(false)
{
    MANGOS_ASSERT(target);
    MANGOS_ASSERT(spellproto && spellproto == sSpellTemplate.LookupEntry<SpellEntry>(spellproto->Id) && "`info` must be pointer to sSpellTemplate element");

    if (!caster)
        m_casterGuid = target->GetObjectGuid();
    else
    {
        // remove this assert when not unit casters will be supported
        MANGOS_ASSERT(caster->isType(TYPEMASK_UNIT))
        m_casterGuid = caster->GetObjectGuid();
    }

    m_applyTime      = time(nullptr);
    m_isPassive      = IsPassiveSpell(spellproto);
    m_isDeathPersist = IsDeathPersistentSpell(spellproto);
    m_trackedAuraType = sSpellMgr.IsSingleTargetSpell(spellproto) ? TRACK_AURA_TYPE_SINGLE_TARGET : IsSpellHaveAura(spellproto, SPELL_AURA_CONTROL_VEHICLE) ? TRACK_AURA_TYPE_CONTROL_VEHICLE : TRACK_AURA_TYPE_NOT_TRACKED;
    m_procCharges    = spellproto->procCharges;

    m_isRemovedOnShapeLost = IsRemovedOnShapeshiftLost(m_spellProto, GetCasterGuid(), target->GetObjectGuid());

    Unit* unitCaster = caster && caster->isType(TYPEMASK_UNIT) ? (Unit*)caster : nullptr;

    m_duration = m_maxDuration = CalculateSpellDuration(spellproto, unitCaster);

    if (m_maxDuration == -1 || (m_isPassive && spellproto->DurationIndex == 0))
        m_permanent = true;

    if (unitCaster)
    {
        if (Player* modOwner = unitCaster->GetSpellModOwner())
            modOwner->ApplySpellMod(GetId(), SPELLMOD_CHARGES, m_procCharges);
    }

    // some custom stack values at aura holder create
    switch (m_spellProto->Id)
    {
        // some auras applied with max stack
        case 24575:                                         // Brittle Armor
        case 24659:                                         // Unstable Power
        case 24662:                                         // Restless Strength
        case 26464:                                         // Mercurial Shield
        case 32065:                                         // Fungal Decay
        case 34027:                                         // Kill Command
        case 35244:                                         // Choking Vines
        case 36659:                                         // Tail Sting
        case 55166:                                         // Tidal Force
        case 58914:                                         // Kill Command (pet part)
        case 62519:                                         // Attuned to Nature
        case 63050:                                         // Sanity
        case 64455:                                         // Feral Essence
        case 65294:                                         // Empowered
        case 70672:                                         // Gaseous Bloat
        case 71564:                                         // Deadly Precision
        case 74396:                                         // Fingers of Frost
            m_stackAmount = m_spellProto->StackAmount;
            break;
    }

    for (auto& m_aura : m_auras)
        m_aura = nullptr;
}

void SpellAuraHolder::AddAura(Aura* aura, SpellEffectIndex index)
{
    m_auras[index] = aura;
    m_auraFlags |= (1 << index);
}

void SpellAuraHolder::RemoveAura(SpellEffectIndex index)
{
    m_auras[index] = nullptr;
    m_auraFlags &= ~(1 << index);
}

void SpellAuraHolder::ApplyAuraModifiers(bool apply, bool real)
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX && !IsDeleted(); ++i)
        if (Aura* aur = GetAuraByEffectIndex(SpellEffectIndex(i)))
            aur->ApplyModifier(apply, real);
}

void SpellAuraHolder::_AddSpellAuraHolder()
{
    if (!GetId())
        return;
    if (!m_target)
        return;

    // Try find slot for aura
    uint8 slot = NULL_AURA_SLOT;

    // Lookup free slot
    if (m_target->GetVisibleAurasCount() < MAX_AURAS)
    {
        Unit::VisibleAuraMap const& visibleAuras = m_target->GetVisibleAuras();
        for (uint8 i = 0; i < MAX_AURAS; ++i)
        {
            Unit::VisibleAuraMap::const_iterator itr = visibleAuras.find(i);
            if (itr == visibleAuras.end())
            {
                slot = i;
                // update for out of range group members (on 1 slot use)
                m_target->UpdateAuraForGroup(slot);
                break;
            }
        }
    }

    Unit* caster = GetCaster();

    uint8 flags = 0;
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (m_auras[i])
            flags |= (1 << i);
    }
    flags |= ((GetCasterGuid() == GetTarget()->GetObjectGuid()) ? AFLAG_NOT_CASTER : AFLAG_NONE) | ((GetSpellMaxDuration(m_spellProto) > 0) ? AFLAG_DURATION : AFLAG_NONE) | (IsPositive() ? AFLAG_POSITIVE : AFLAG_NEGATIVE);
    SetAuraFlags(flags);

    SetAuraLevel(caster ? caster->getLevel() : sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));

    if (IsNeedVisibleSlot(caster))
    {
        SetAuraSlot(slot);
        if (slot < MAX_AURAS)                       // slot found send data to client
        {
            SetVisibleAura(false);
            SendAuraUpdate(false);
        }

        //*****************************************************
        // Update target aura state flag on holder apply
        // TODO: Make it easer
        //*****************************************************

        // Sitdown on apply aura req seated
        if (m_spellProto->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED && !m_target->IsSitState())
            m_target->SetStandState(UNIT_STAND_STATE_SIT);

        // register aura diminishing on apply
        if (getDiminishGroup() != DIMINISHING_NONE)
            m_target->ApplyDiminishingAura(getDiminishGroup(), true);

        // Update Seals information
        if (IsSealSpell(m_spellProto))
            m_target->ModifyAuraState(AURA_STATE_JUDGEMENT, true);

        // Conflagrate aura state on Immolate and Shadowflame
        if (m_spellProto->IsFitToFamily(SPELLFAMILY_WARLOCK, uint64(0x0000000000000004), 0x00000002))
            m_target->ModifyAuraState(AURA_STATE_CONFLAGRATE, true);

        // Faerie Fire (druid versions)
        if (m_spellProto->HasAttribute(SPELL_ATTR_SS_PREVENT_INVIS))
            m_target->ModifyAuraState(AURA_STATE_FAERIE_FIRE, true);

        // Victorious
        if (m_spellProto->IsFitToFamily(SPELLFAMILY_WARRIOR, uint64(0x0004000000000000)))
            m_target->ModifyAuraState(AURA_STATE_WARRIOR_VICTORY_RUSH, true);

        // Swiftmend state on Regrowth & Rejuvenation
        if (m_spellProto->IsFitToFamily(SPELLFAMILY_DRUID, uint64(0x0000000000000050)))
            m_target->ModifyAuraState(AURA_STATE_SWIFTMEND, true);

        // Deadly poison aura state
        if (m_spellProto->IsFitToFamily(SPELLFAMILY_ROGUE, uint64(0x0000000000010000)))
            m_target->ModifyAuraState(AURA_STATE_DEADLY_POISON, true);

        // Enrage aura state
        if (m_spellProto->Dispel == DISPEL_ENRAGE)
            m_target->ModifyAuraState(AURA_STATE_ENRAGE, true);

        // Bleeding aura state
        if (GetAllSpellMechanicMask(m_spellProto) & (1 << (MECHANIC_BLEED - 1)))
            m_target->ModifyAuraState(AURA_STATE_BLEEDING, true);
    }
}

void SpellAuraHolder::_RemoveSpellAuraHolder()
{
    // Remove all triggered by aura spells vs unlimited duration
    // except same aura replace case
    if (m_removeMode != AURA_REMOVE_BY_STACK)
        CleanupTriggeredSpells();

    Unit* caster = GetCaster();

    if (caster && IsPersistent())
        if (DynamicObject* dynObj = caster->GetDynObject(GetId()))
            dynObj->RemoveAffected(m_target);

    // remove at-store spell cast items (for all remove modes?)
    if (m_target->GetTypeId() == TYPEID_PLAYER && m_removeMode != AURA_REMOVE_BY_DEFAULT && m_removeMode != AURA_REMOVE_BY_DELETE)
        if (ObjectGuid castItemGuid = GetCastItemGuid())
            if (Item* castItem = ((Player*)m_target)->GetItemByGuid(castItemGuid))
                ((Player*)m_target)->DestroyItemWithOnStoreSpell(castItem, GetId());

    // passive auras do not get put in slots - said who? ;)
    // Note: but totem can be not accessible for aura target in time remove (to far for find in grid)
    // if (m_isPassive && !(caster && caster->GetTypeId() == TYPEID_UNIT && ((Creature*)caster)->IsTotem()))
    //    return;

    uint8 slot = GetAuraSlot();

    if (slot >= MAX_AURAS)                                  // slot not set
        return;

    if (m_target->GetVisibleAura(slot) == 0)
        return;

    // unregister aura diminishing (and store last time)
    if (getDiminishGroup() != DIMINISHING_NONE)
        m_target->ApplyDiminishingAura(getDiminishGroup(), false);

    SetAuraFlags(AFLAG_NONE);
    SetAuraLevel(0);
    SetVisibleAura(true);

    if (m_removeMode != AURA_REMOVE_BY_DELETE)
    {
        SendAuraUpdate(true);

        // update for out of range group members
        m_target->UpdateAuraForGroup(slot);

        //*****************************************************
        // Update target aura state flag (at last aura remove)
        //*****************************************************
        // Enrage aura state
        if (m_spellProto->Dispel == DISPEL_ENRAGE)
            m_target->ModifyAuraState(AURA_STATE_ENRAGE, false);

        // Bleeding aura state
        if (GetAllSpellMechanicMask(m_spellProto) & (1 << (MECHANIC_BLEED - 1)))
        {
            bool found = false;

            Unit::SpellAuraHolderMap const& holders = m_target->GetSpellAuraHolderMap();
            for (const auto& holder : holders)
            {
                if (GetAllSpellMechanicMask(holder.second->GetSpellProto()) & (1 << (MECHANIC_BLEED - 1)))
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                m_target->ModifyAuraState(AURA_STATE_BLEEDING, false);
        }

        uint32 removeState = 0;
        ClassFamilyMask removeFamilyFlag = m_spellProto->SpellFamilyFlags;
        switch (m_spellProto->SpellFamilyName)
        {
            case SPELLFAMILY_PALADIN:
                if (IsSealSpell(m_spellProto))
                    removeState = AURA_STATE_JUDGEMENT;     // Update Seals information
                break;
            case SPELLFAMILY_WARLOCK:
                // Conflagrate aura state on Immolate and Shadowflame,
                if (m_spellProto->IsFitToFamilyMask(uint64(0x0000000000000004), 0x00000002))
                {
                    removeFamilyFlag = ClassFamilyMask(uint64(0x0000000000000004), 0x00000002);
                    removeState = AURA_STATE_CONFLAGRATE;
                }
                break;
            case SPELLFAMILY_DRUID:
                if (m_spellProto->IsFitToFamilyMask(uint64(0x0000000000000050)))
                {
                    removeFamilyFlag = ClassFamilyMask(uint64(0x00000000000050));
                    removeState = AURA_STATE_SWIFTMEND;     // Swiftmend aura state
                }
                break;
            case SPELLFAMILY_WARRIOR:
                if (m_spellProto->IsFitToFamilyMask(uint64(0x0004000000000000)))
                    removeState = AURA_STATE_WARRIOR_VICTORY_RUSH; // Victorious
                break;
            case SPELLFAMILY_ROGUE:
                if (m_spellProto->IsFitToFamilyMask(uint64(0x0000000000010000)))
                    removeState = AURA_STATE_DEADLY_POISON; // Deadly poison aura state
                break;
            case SPELLFAMILY_HUNTER:
                if (m_spellProto->IsFitToFamilyMask(uint64(0x1000000000000000)))
                    removeState = AURA_STATE_FAERIE_FIRE;   // Sting (hunter versions)
        }

        if (m_spellProto->HasAttribute(SPELL_ATTR_SS_PREVENT_INVIS))
            removeState = AURA_STATE_FAERIE_FIRE;   // Faerie Fire

        // Remove state (but need check other auras for it)
        if (removeState)
        {
            bool found = false;
            Unit::SpellAuraHolderMap const& holders = m_target->GetSpellAuraHolderMap();
            for (const auto& holder : holders)
            {
                SpellEntry const* auraSpellInfo = holder.second->GetSpellProto();
                if (auraSpellInfo->IsFitToFamily(SpellFamily(m_spellProto->SpellFamilyName), removeFamilyFlag))
                {
                    found = true;
                    break;
                }
            }

            // this has been last aura
            if (!found)
                m_target->ModifyAuraState(AuraState(removeState), false);
        }

        // reset cooldown state for spells
        if (caster && GetSpellProto()->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE))
        {
            // some spells need to start cooldown at aura fade (like stealth)
            caster->AddCooldown(*GetSpellProto());
        }
    }
}

void SpellAuraHolder::CleanupTriggeredSpells()
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!m_spellProto->EffectApplyAuraName[i])
            continue;

        uint32 tSpellId = m_spellProto->EffectTriggerSpell[i];
        if (!tSpellId)
            continue;

        SpellEntry const* tProto = sSpellTemplate.LookupEntry<SpellEntry>(tSpellId);
        if (!tProto)
            continue;

        if (GetSpellDuration(tProto) != -1)
            continue;

        // needed for spell 43680, maybe others
        // TODO: is there a spell flag, which can solve this in a more sophisticated way?
        if (m_spellProto->EffectApplyAuraName[i] == SPELL_AURA_PERIODIC_TRIGGER_SPELL &&
                GetSpellDuration(m_spellProto) == int32(m_spellProto->EffectAmplitude[i]))
            continue;

        m_target->RemoveAurasDueToSpell(tSpellId);
    }
}

bool SpellAuraHolder::ModStackAmount(int32 num, Unit* newCaster)
{
    uint32 protoStackAmount = m_spellProto->StackAmount;

    // Can`t mod
    if (!protoStackAmount)
        return true;

    // Modify stack but limit it
    int32 stackAmount = m_stackAmount + num;
    if (stackAmount > (int32)protoStackAmount)
        stackAmount = protoStackAmount;
    else if (stackAmount <= 0) // Last aura from stack removed
    {
        m_stackAmount = 0;
        return true; // need remove aura
    }

    // Update stack amount
    SetStackAmount(stackAmount, newCaster);
    return false;
}

void SpellAuraHolder::SetStackAmount(uint32 stackAmount, Unit* newCaster)
{
    Unit* target = GetTarget();
    if (!target)
        return;

    if (stackAmount >= m_stackAmount)
    {
        // Change caster
        Unit* oldCaster = GetCaster();
        if (oldCaster != newCaster)
        {
            m_casterGuid = newCaster->GetObjectGuid();
            // New caster duration sent for owner in RefreshHolder
        }
        // Stack increased refresh duration
        RefreshHolder();
    }
    else
        // Stack decreased only send update
        SendAuraUpdate(false);

    int32 oldStackAmount = m_stackAmount;
    m_stackAmount = stackAmount;

    for (auto aur : m_auras)
    {
        if (aur)
        {
            int32 baseAmount = aur->GetModifier()->m_baseAmount;
            int32 amount = m_stackAmount * baseAmount;
            // Reapply if amount change
            if (!baseAmount || amount != aur->GetModifier()->m_amount)
            {
                aur->SetRemoveMode(AURA_REMOVE_BY_GAINED_STACK);
                if (IsAuraRemoveOnStacking(this->GetSpellProto(), aur->GetEffIndex()))
                    aur->ApplyModifier(false, true);
                aur->GetModifier()->m_amount = amount;
                aur->GetModifier()->m_recentAmount = baseAmount * (stackAmount - oldStackAmount);
                aur->ApplyModifier(true, true);
            }
        }
    }
}

Unit* SpellAuraHolder::GetCaster() const
{
    if (GetCasterGuid() == m_target->GetObjectGuid())
        return m_target;

    return ObjectAccessor::GetUnit(*m_target, m_casterGuid);// player will search at any maps
}

bool SpellAuraHolder::IsWeaponBuffCoexistableWith(SpellAuraHolder const* ref) const
{
    // only item casted spells
    if (!GetCastItemGuid())
        return false;

    // Exclude Debuffs
    if (!IsPositive())
        return false;

    // Exclude Non-generic Buffs [ie: Runeforging] and Executioner-Enchant
    if (GetSpellProto()->SpellFamilyName != SPELLFAMILY_GENERIC || GetId() == 42976)
        return false;

    // Exclude Stackable Buffs [ie: Blood Reserve]
    if (GetSpellProto()->StackAmount)
        return false;

    // only self applied player buffs
    if (m_target->GetTypeId() != TYPEID_PLAYER || m_target->GetObjectGuid() != GetCasterGuid())
        return false;

    Item* castItem = ((Player*)m_target)->GetItemByGuid(GetCastItemGuid());
    if (!castItem)
        return false;

    // Limit to Weapon-Slots
    if (!castItem->IsEquipped() ||
            (castItem->GetSlot() != EQUIPMENT_SLOT_MAINHAND && castItem->GetSlot() != EQUIPMENT_SLOT_OFFHAND))
        return false;

    // from different weapons
    return ref->GetCastItemGuid() && ref->GetCastItemGuid() != GetCastItemGuid();
}

bool SpellAuraHolder::IsNeedVisibleSlot(Unit const* caster) const
{
    bool totemAura = caster && caster->GetTypeId() == TYPEID_UNIT && ((Creature*)caster)->IsTotem();

    if (m_spellProto->procFlags)
        return true;
    if (IsSpellTriggerSpellByAura(m_spellProto))
        return true;
    if (IsSpellHaveAura(m_spellProto, SPELL_AURA_MOD_IGNORE_SHAPESHIFT))
        return true;
    if (IsSpellHaveAura(m_spellProto, SPELL_AURA_IGNORE_UNIT_STATE))
        return true;

    // passive auras (except totem auras) do not get placed in the slots
    return !m_isPassive || totemAura || HasAreaAuraEffect(m_spellProto);
}

void SpellAuraHolder::BuildUpdatePacket(WorldPacket& data) const
{
    data << uint8(GetAuraSlot());
    data << uint32(GetId());

    uint8 auraFlags = GetAuraFlags();
    data << uint8(auraFlags);
    data << uint8(GetAuraLevel());

    uint32 stackCount = m_procCharges ? m_procCharges * m_stackAmount : m_stackAmount;
    data << uint8(stackCount <= 255 ? stackCount : 255);

    if (!(auraFlags & AFLAG_NOT_CASTER))
    {
        data << GetCasterGuid().WriteAsPacked();
    }

    if (auraFlags & AFLAG_DURATION)
    {
        data << uint32(GetAuraMaxDuration());
        data << uint32(GetAuraDuration());
    }
}

void SpellAuraHolder::SendAuraUpdate(bool remove) const
{
    WorldPacket data(SMSG_AURA_UPDATE);
    data << m_target->GetPackGUID();

    if (remove)
    {
        data << uint8(GetAuraSlot());
        data << uint32(0);
    }
    else
        BuildUpdatePacket(data);

    m_target->SendMessageToSet(data, true);
}

void SpellAuraHolder::HandleSpellSpecificBoosts(bool apply)
{
    bool cast_at_remove = false;                            // if spell must be casted at last aura from stack remove
    std::vector<uint32> boostSpells;

    switch (GetSpellProto()->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (GetId())
            {
                case 29865:                                 // Deathbloom (10 man)
                {
                    if (!apply && m_removeMode == AURA_REMOVE_BY_EXPIRE)
                    {
                        cast_at_remove = true;
                        boostSpells.push_back(55594);
                    }
                    else
                        return;
                    break;
                }
                case 32830: // Possess
                    boostSpells.push_back(32831);
                    break;
                case 33896: // Desperate Defense
                    boostSpells.push_back(33897);
                    break;
                case 36797: // Mind Control - Kaelthas
                    boostSpells.push_back(36798);
                    break;
                case 38511: // Persuasion - Vashj
                    boostSpells.push_back(38514);
                    break;
                case 55053:                                 // Deathbloom (25 man)
                {
                    if (!apply && m_removeMode == AURA_REMOVE_BY_EXPIRE)
                    {
                        cast_at_remove = true;
                        boostSpells.push_back(55601);
                    }
                    else
                        return;
                    break;
                }
                case 50720:                                 // Vigilance (warrior spell but not have warrior family)
                    boostSpells.push_back(68066);                       // Damage Reduction
                    break;
                case 57350:                                 // Illusionary Barrier
                {
                    if (!apply && m_target->GetPowerType() == POWER_MANA)
                    {
                        cast_at_remove = true;
                        boostSpells.push_back(60242);                   // Darkmoon Card: Illusion
                    }
                    else
                        return;
                    break;
                }
                case 58914:                                 // Kill Command, pet aura
                {
                    // Removal is needed here because the dummy aura handler is applied / removed at stacks change
                    if (!apply)
                        if (Unit* caster = GetCaster())
                            caster->RemoveAurasDueToSpell(34027);
                    return;
                }
                case 62692:                                 // Aura of Despair
                    boostSpells.push_back(64848);
                    break;
                case 71905:                                 // Soul Fragment
                {
                    if (!apply)
                    {
                        boostSpells.push_back(72521);                   // Shadowmourne Visual Low
                        boostSpells.push_back(72523);                   // Shadowmourne Visual High
                    }
                    else
                        return;
                    break;
                }
                default:
                    return;
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            // Ice Barrier (non stacking from one caster)
            if (m_spellProto->SpellIconID == 32)
            {
                if ((!apply && m_removeMode == AURA_REMOVE_BY_DISPEL) || m_removeMode == AURA_REMOVE_BY_SHIELD_BREAK)
                {
                    Unit::AuraList const& dummyAuras = m_target->GetAurasByType(SPELL_AURA_DUMMY);
                    for (auto dummyAura : dummyAuras)
                    {
                        // Shattered Barrier
                        if (dummyAura->GetSpellProto()->SpellIconID == 2945)
                        {
                            cast_at_remove = true;
                            // first rank have 50% chance
                            if (dummyAura->GetId() != 44745 || roll_chance_i(50))
                                boostSpells.push_back(55080);
                            break;
                        }
                    }
                }
                else
                    return;
                break;
            }

            switch (GetId())
            {
                case 11129:                                 // Combustion (remove triggered aura stack)
                {
                    if (!apply)
                        boostSpells.push_back(28682);
                    else
                        return;
                    break;
                }
                case 28682:                                 // Combustion (remove main aura)
                {
                    if (!apply)
                        boostSpells.push_back(11129);
                    else
                        return;
                    break;
                }
                case 44401:                                 // Missile Barrage (triggered)
                case 48108:                                 // Hot Streak (triggered)
                case 57761:                                 // Fireball! (Brain Freeze triggered)
                {
                    // consumed aura (at proc charges 0)
                    if (!apply && m_removeMode == AURA_REMOVE_BY_DEFAULT)
                    {
                        Unit* caster = GetCaster();
                        // Item - Mage T10 2P Bonus
                        if (!caster || !caster->HasAura(70752))
                            return;

                        cast_at_remove = true;
                        boostSpells.push_back(70753);                   // Pushing the Limit
                    }
                    else
                        return;
                    break;
                }
                case 74396:                                 // Fingers of Frost (remove main aura)
                {
                    if (!apply)
                        boostSpells.push_back(44544);
                    else
                        return;
                    break;
                }
                default:
                    break; // Break here for poly below - 2.4.2+ only player poly regens
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            if (!apply)
            {
                // Remove Blood Frenzy only if target no longer has any Deep Wound or Rend (applying is handled by procs)
                if (GetSpellProto()->Mechanic != MECHANIC_BLEED)
                    return;

                // If target still has one of Warrior's bleeds, do nothing
                Unit::AuraList const& PeriodicDamage = m_target->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
                for (auto i : PeriodicDamage)
                    if (i->GetCasterGuid() == GetCasterGuid() &&
                        i->GetSpellProto()->SpellFamilyName == SPELLFAMILY_WARRIOR &&
                        i->GetSpellProto()->Mechanic == MECHANIC_BLEED)
                        return;

                boostSpells.push_back(30069);                           // Blood Frenzy (Rank 1)
                boostSpells.push_back(30070);                           // Blood Frenzy (Rank 2)
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            // Fear (non stacking)
            if (m_spellProto->SpellFamilyFlags & uint64(0x0000040000000000))
            {
                if (!apply)
                {
                    Unit* caster = GetCaster();
                    if (!caster)
                        return;

                    Unit::AuraList const& dummyAuras = caster->GetAurasByType(SPELL_AURA_DUMMY);
                    for (auto dummyAura : dummyAuras)
                    {
                        SpellEntry const* dummyEntry = dummyAura->GetSpellProto();
                        // Improved Fear
                        if (dummyEntry->SpellFamilyName == SPELLFAMILY_WARLOCK && dummyEntry->SpellIconID == 98)
                        {
                            cast_at_remove = true;
                            switch (dummyAura->GetModifier()->m_amount)
                            {
                                // Rank 1
                                case 0: boostSpells.push_back(60946); break;
                                // Rank 1
                                case 1: boostSpells.push_back(60947); break;
                            }
                            break;
                        }
                    }
                }
                else
                    return;
            }
            // Shadowflame (DoT)
            else if (m_spellProto->IsFitToFamilyMask(uint64(0x0000000000000000), 0x00000002))
            {
                // Glyph of Shadowflame
                if (!apply)
                    boostSpells.push_back(63311);
                else
                {
                    Unit* caster = GetCaster();
                    if (caster && caster->HasAura(63310))
                        boostSpells.push_back(63311);
                    else
                        return;
                }
            }
            else
                return;
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            // Shadow Word: Pain (need visual check fro skip improvement talent) or Vampiric Touch
            if ((m_spellProto->SpellIconID == 234 && m_spellProto->SpellVisual[0]) || m_spellProto->SpellIconID == 2213)
            {
                if (!apply && m_removeMode == AURA_REMOVE_BY_DISPEL)
                {
                    Unit* caster = GetCaster();
                    if (!caster)
                        return;

                    Unit::AuraList const& dummyAuras = caster->GetAurasByType(SPELL_AURA_DUMMY);
                    for (auto dummyAura : dummyAuras)
                    {
                        // Shadow Affinity
                        if (dummyAura->GetSpellProto()->SpellFamilyName == SPELLFAMILY_PRIEST
                                && dummyAura->GetSpellProto()->SpellIconID == 178)
                        {
                            // custom cast code
                            int32 basepoints0 = dummyAura->GetModifier()->m_amount * caster->GetCreateMana() / 100;
                            caster->CastCustomSpell(caster, 64103, &basepoints0, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr);
                            return;
                        }
                    }
                }
                else
                    return;
            }
            // Power Word: Shield
            else if (apply && m_spellProto->SpellFamilyFlags & uint64(0x0000000000000001) && m_spellProto->Mechanic == MECHANIC_SHIELD)
            {
                Unit* caster = GetCaster();
                if (!caster)
                    return;

                // Glyph of Power Word: Shield
                if (Aura* glyph = caster->GetAura(55672, EFFECT_INDEX_0))
                {
                    Aura* shield = GetAuraByEffectIndex(EFFECT_INDEX_0);
                    int32 heal = (glyph->GetModifier()->m_amount * shield->GetModifier()->m_amount) / 100;
                    caster->CastCustomSpell(m_target, 56160, &heal, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, shield);
                }
                return;
            }
            switch (GetId())
            {
                // Abolish Disease (remove 1 more poison effect with Body and Soul)
                case 552:
                {
                    if (apply)
                    {
                        int chance = 0;
                        Unit::AuraList const& dummyAuras = m_target->GetAurasByType(SPELL_AURA_DUMMY);
                        for (auto dummyAura : dummyAuras)
                        {
                            SpellEntry const* dummyEntry = dummyAura->GetSpellProto();
                            // Body and Soul (talent ranks)
                            if (dummyEntry->SpellFamilyName == SPELLFAMILY_PRIEST && dummyEntry->SpellIconID == 2218 &&
                                    dummyEntry->SpellVisual[0] == 0)
                            {
                                chance = dummyAura->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_1);
                                break;
                            }
                        }

                        if (roll_chance_i(chance))
                            boostSpells.push_back(64134);               // Body and Soul (periodic dispel effect)
                    }
                    else
                                boostSpells.push_back(64134);                   // Body and Soul (periodic dispel effect)
                    break;
                }
                // Dispersion mana reg and immunity
                case 47585:
                    boostSpells.push_back(60069);                       // Dispersion
                    boostSpells.push_back(63230);                       // Dispersion
                    break;
                default:
                    break;
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Barkskin
            if (GetId() == 22812 && m_target->HasAura(63057)) // Glyph of Barkskin
                boostSpells.push_back(63058);                           // Glyph - Barkskin 01
            else if (!apply && GetId() == 5229)             // Enrage (Druid Bear)
                boostSpells.push_back(51185);                           // King of the Jungle (Enrage damage aura)
            else
                return;
            break;
        }
        case SPELLFAMILY_ROGUE:
            // Sprint (skip non player casted spells by category)
            if (GetSpellProto()->SpellFamilyFlags & uint64(0x0000000000000040) && GetSpellProto()->Category == 44)
            {
                if (!apply || m_target->HasAura(58039))     // Glyph of Blurred Speed
                    boostSpells.push_back(61922);                       // Sprint (waterwalk)
                else
                    return;
            }
            else
                return;
            break;
        case SPELLFAMILY_HUNTER:
        {
            switch (GetId())
            {
                case 34074:                                 // Aspect of the Viper
                {
                    if (!apply || m_target->HasAura(60144)) // Viper Attack Speed
                        boostSpells.push_back(61609);                   // Vicious Viper
                    else
                        return;
                    break;
                }
                case 19574:                                 // Bestial Wrath - immunity
                case 34471:                                 // The Beast Within - immunity
                {
                    boostSpells.push_back(24395);
                    boostSpells.push_back(24396);
                    boostSpells.push_back(24397);
                    boostSpells.push_back(26592);
                    break;
                }
                case 34027:                                 // Kill Command, owner aura (spellmods)
                {
                    if (apply)
                    {
                        if (m_target->HasAura(35029))       // Focused Fire, rank 1
                            boostSpells.push_back(60110);               // Kill Command, Focused Fire rank 1 bonus
                        else if (m_target->HasAura(35030))  // Focused Fire, rank 2
                            boostSpells.push_back(60113);               // Kill Command, Focused Fire rank 2 bonus
                        else
                            return;
                    }
                    else
                    {
                        boostSpells.push_back(34026);                   // Kill Command, owner casting aura
                        boostSpells.push_back(60110);                   // Kill Command, Focused Fire rank 1 bonus
                        boostSpells.push_back(60113);                   // Kill Command, Focused Fire rank 2 bonus
                        if (Unit* pet = m_target->GetPet())
                            pet->RemoveAurasDueToSpell(58914); // Kill Command, pet aura
                    }
                    break;
                }
                case 35029:                                 // Focused Fire, rank 1
                {
                    if (apply && !m_target->HasAura(34027)) // Kill Command, owner casting aura
                        return;

                    boostSpells.push_back(60110);                       // Kill Command, Focused Fire rank 1 bonus
                    break;
                }
                case 35030:                                 // Focused Fire, rank 2
                {
                    if (apply && !m_target->HasAura(34027)) // Kill Command, owner casting aura
                        return;

                    boostSpells.push_back(60113);                       // Kill Command, Focused Fire rank 2 bonus
                    break;
                }
                default:
                    // Freezing Trap Effect
                    if (m_spellProto->SpellFamilyFlags & uint64(0x0000000000000008))
                    {
                        if (!apply)
                        {
                            Unit* caster = GetCaster();
                            // Glyph of Freezing Trap
                            if (caster && caster->HasAura(56845))
                            {
                                cast_at_remove = true;
                                boostSpells.push_back(61394);
                            }
                            else
                                return;
                        }
                        else
                            return;
                    }
                    // Aspect of the Dragonhawk dodge
                    else if (GetSpellProto()->IsFitToFamilyMask(uint64(0x0000000000000000), 0x00001000))
                    {
                        boostSpells.push_back(61848);

                        // triggered spell have same category as main spell and cooldown
                        if (apply && m_target->GetTypeId() == TYPEID_PLAYER)
                            m_target->RemoveSpellCooldown(61848);
                    }
                    else
                        return;
                    break;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            if (m_spellProto->Id == 31884)                  // Avenging Wrath
            {
                if (!apply)
                    boostSpells.push_back(57318);                       // Sanctified Wrath (triggered)
                else
                {
                    int32 percent = 0;
                    Unit::AuraList const& dummyAuras = m_target->GetAurasByType(SPELL_AURA_DUMMY);
                    for (auto dummyAura : dummyAuras)
                    {
                        if (dummyAura->GetSpellProto()->SpellIconID == 3029)
                        {
                            percent = dummyAura->GetModifier()->m_amount;
                            break;
                        }
                    }

                    // apply in special way
                    if (percent)
                    {
                        // Sanctified Wrath (triggered)
                        // prevent aura deletion, specially in multi-boost case
                        m_target->CastCustomSpell(m_target, 57318, &percent, &percent, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr);
                    }
                    return;
                }
                break;
            }

            // Only process on player casting paladin aura
            // all aura bonuses applied also in aura area effect way to caster
            if (GetCasterGuid() != m_target->GetObjectGuid() || !GetCasterGuid().IsPlayer())
                return;

            if (GetSpellSpecific(m_spellProto->Id) != SPELL_AURA)
                return;

            // Sanctified Retribution and Swift Retribution (they share one aura), but not Retribution Aura (already gets modded)
            if ((GetSpellProto()->SpellFamilyFlags & uint64(0x0000000000000008)) == 0)
                boostSpells.push_back(63531);                           // placeholder for talent spell mods
            // Improved Concentration Aura (auras bonus)
            boostSpells.push_back(63510);                               // placeholder for talent spell mods
            // Improved Devotion Aura (auras bonus)
            boostSpells.push_back(63514);                               // placeholder for talent spell mods
            break;
        }
        case SPELLFAMILY_DEATHKNIGHT:
        {
            // second part of spell apply
            switch (GetId())
            {
                case 49039: boostSpells.push_back(50397); break;        // Lichborne

                case 48263:                                 // Frost Presence
                case 48265:                                 // Unholy Presence
                case 48266:                                 // Blood Presence
                {
                    // else part one per 3 pair
                    if (GetId() == 48263 || GetId() == 48265) // Frost Presence or Unholy Presence
                    {
                        // Improved Blood Presence
                        int32 heal_pct = 0;
                        if (apply)
                        {
                            Unit::AuraList const& bloodAuras = m_target->GetAurasByType(SPELL_AURA_DUMMY);
                            for (auto bloodAura : bloodAuras)
                            {
                                // skip same icon
                                if (bloodAura->GetSpellProto()->SpellFamilyName == SPELLFAMILY_DEATHKNIGHT &&
                                    bloodAura->GetSpellProto()->SpellIconID == 2636)
                                {
                                    heal_pct = bloodAura->GetModifier()->m_amount;
                                    break;
                                }
                            }
                        }

                        if (heal_pct)
                            m_target->CastCustomSpell(m_target, 63611, &heal_pct, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                        else
                            m_target->RemoveAurasDueToSpell(63611);
                    }
                    else
                        boostSpells.push_back(63611);                   // Improved Blood Presence, trigger for heal

                    if (GetId() == 48263 || GetId() == 48266) // Frost Presence or Blood Presence
                    {
                        // Improved Unholy Presence
                        int32 power_pct = 0;
                        if (apply)
                        {
                            Unit::AuraList const& unholyAuras = m_target->GetAurasByType(SPELL_AURA_DUMMY);
                            for (auto unholyAura : unholyAuras)
                            {
                                // skip same icon
                                if (unholyAura->GetSpellProto()->SpellFamilyName == SPELLFAMILY_DEATHKNIGHT &&
                                    unholyAura->GetSpellProto()->SpellIconID == 2633)
                                {
                                    power_pct = unholyAura->GetModifier()->m_amount;
                                    break;
                                }
                            }
                        }
                        if (power_pct || !apply)
                            boostSpells.push_back(49772);               // Unholy Presence, speed part, spell1 used for Improvement presence fit to own presence
                    }
                    else
                                boostSpells.push_back(49772);                   // Unholy Presence move speed

                    if (GetId() == 48265 || GetId() == 48266)   // Unholy Presence or Blood Presence
                    {
                        // Improved Frost Presence
                        int32 stamina_pct = 0;
                        if (apply)
                        {
                            Unit::AuraList const& frostAuras = m_target->GetAurasByType(SPELL_AURA_DUMMY);
                            for (auto frostAura : frostAuras)
                            {
                                // skip same icon
                                if (frostAura->GetSpellProto()->SpellFamilyName == SPELLFAMILY_DEATHKNIGHT &&
                                    frostAura->GetSpellProto()->SpellIconID == 2632)
                                {
                                    stamina_pct = frostAura->GetModifier()->m_amount;
                                    break;
                                }
                            }
                        }

                        if (stamina_pct)
                            m_target->CastCustomSpell(m_target, 61261, &stamina_pct, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                        else
                            m_target->RemoveAurasDueToSpell(61261);
                    }
                    else
                        boostSpells.push_back(61261);                   // Frost Presence, stamina

                    if (GetId() == 48265)                   // Unholy Presence
                    {
                        // Improved Unholy Presence, special case for own presence
                        int32 power_pct = 0;
                        if (apply)
                        {
                            Unit::AuraList const& unholyAuras = m_target->GetAurasByType(SPELL_AURA_DUMMY);
                            for (auto unholyAura : unholyAuras)
                            {
                                // skip same icon
                                if (unholyAura->GetSpellProto()->SpellFamilyName == SPELLFAMILY_DEATHKNIGHT &&
                                    unholyAura->GetSpellProto()->SpellIconID == 2633)
                                {
                                    power_pct = unholyAura->GetModifier()->m_amount;
                                    break;
                                }
                            }
                        }

                        if (power_pct)
                        {
                            int32 bp = 5;
                            m_target->CastCustomSpell(m_target, 63622, &bp, &bp, &bp, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                            m_target->CastCustomSpell(m_target, 65095, &bp, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                        }
                        else
                        {
                            m_target->RemoveAurasDueToSpell(63622);
                            m_target->RemoveAurasDueToSpell(65095);
                        }
                    }
                    break;
                }
            }

            // Improved Blood Presence
            if (GetSpellProto()->SpellIconID == 2636 && m_isPassive)
            {
                // if presence active: Frost Presence or Unholy Presence
                if (apply && (m_target->HasAura(48263) || m_target->HasAura(48265)))
                {
                    Aura* aura = GetAuraByEffectIndex(EFFECT_INDEX_0);
                    if (!aura)
                        return;

                    int32 bp = aura->GetModifier()->m_amount;
                    m_target->CastCustomSpell(m_target, 63611, &bp, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                }
                else
                    m_target->RemoveAurasDueToSpell(63611);
                return;
            }

            // Improved Frost Presence
            if (GetSpellProto()->SpellIconID == 2632 && m_isPassive)
            {
                // if presence active: Unholy Presence or Blood Presence
                if (apply && (m_target->HasAura(48265) || m_target->HasAura(48266)))
                {
                    Aura* aura = GetAuraByEffectIndex(EFFECT_INDEX_0);
                    if (!aura)
                        return;

                    int32 bp0 = aura->GetModifier()->m_amount;
                    int32 bp1 = 0;                          // disable threat mod part for not Frost Presence case
                    m_target->CastCustomSpell(m_target, 61261, &bp0, &bp1, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                }
                else
                    m_target->RemoveAurasDueToSpell(61261);
                return;
            }

            // Improved Unholy Presence
            if (GetSpellProto()->SpellIconID == 2633 && m_isPassive)
            {
                // if presence active: Unholy Presence
                if (apply && m_target->HasAura(48265))
                {
                    int32 bp = 5;
                    m_target->CastCustomSpell(m_target, 63622, &bp, &bp, &bp, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                    m_target->CastCustomSpell(m_target, 65095, &bp, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetCasterGuid());
                }
                else
                {
                    m_target->RemoveAurasDueToSpell(63622);
                    m_target->RemoveAurasDueToSpell(65095);
                }

                // if presence active: Frost Presence or Blood Presence
                if (!apply || m_target->HasAura(48263) || m_target->HasAura(48266))
                    boostSpells.push_back(49772);
                else
                    return;
                break;
            }
            break;
        }
        default:
            return;
    }

    if (GetSpellProto()->Mechanic == MECHANIC_POLYMORPH)
        boostSpells.push_back(12939); // Just so that this doesnt conflict with others

    if (boostSpells.empty())
        return;

    for (uint32 spellId : boostSpells)
    {
        Unit* boostCaster = m_target;
        Unit* boostTarget = nullptr;
        ObjectGuid casterGuid = m_target->GetObjectGuid(); // caster can be nullptr, but guid is still valid for removal
        SpellEntry const* boostEntry = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
        for (uint32 target : boostEntry->EffectImplicitTargetA)
        {
            switch (target)
            {
                case TARGET_UNIT_ENEMY:
                case TARGET_UNIT:
                    if (apply) // optimization
                        boostCaster = GetCaster();
                    else
                        casterGuid = GetCasterGuid();
                    boostTarget = m_target;
                    break;
            }
        }
        if (apply || cast_at_remove)
            boostCaster->CastSpell(boostTarget, boostEntry, TRIGGERED_OLD_TRIGGERED);
        else
            m_target->RemoveAurasByCasterSpell(spellId, casterGuid);
    }
}

SpellAuraHolder::~SpellAuraHolder()
{
    // note: auras in delete list won't be affected since they clear themselves from holder when adding to deletedAuraslist
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        delete m_auras[i];
}

void SpellAuraHolder::Update(uint32 diff)
{
    if (m_skipUpdate)
    {
        m_skipUpdate = false;
        return;
    }

    for (auto aura : m_auras)
        if (aura)
            aura->UpdateAura(diff);

    if (m_duration > 0)
    {
        m_duration -= diff;
        if (m_duration < 0)
            m_duration = 0;

        m_timeCla -= diff;

        if (m_timeCla <= 0)
        {
            if (Unit* caster = GetCaster())
            {
                // This should not be used for health funnel (already processed in PeriodicTick()).
                // TODO:: is the fallowing code can be removed?
                if (GetSpellProto()->SpellVisual[0] != 163)
                {
                    Powers powertype = Powers(GetSpellProto()->powerType);
                    int32 manaPerSecond = GetSpellProto()->manaPerSecond + GetSpellProto()->manaPerSecondPerLevel * caster->getLevel();
                    m_timeCla = 1 * IN_MILLISECONDS;

                    if (manaPerSecond)
                    {
                        if (powertype == POWER_HEALTH)
                            caster->ModifyHealth(-manaPerSecond);
                        else
                            caster->ModifyPower(powertype, -manaPerSecond);
                    }
                }
            }
        }
    }
}

void SpellAuraHolder::RefreshHolder()
{
    SetAuraDuration(GetAuraMaxDuration());
    SendAuraUpdate(false);
}

void SpellAuraHolder::SetAuraMaxDuration(int32 duration)
{
    m_maxDuration = duration;

    // possible overwrite persistent state
    if (!GetSpellProto()->HasAttribute(SPELL_ATTR_EX5_HIDE_DURATION) && duration > 0)
    {
        if (!(IsPassive() && GetSpellProto()->DurationIndex == 0))
            SetPermanent(false);

        SetAuraFlags(GetAuraFlags() | AFLAG_DURATION);
    }
    else
        SetAuraFlags(GetAuraFlags() & ~AFLAG_DURATION);
}

bool SpellAuraHolder::DropAuraCharge()
{
    if (m_procCharges == 0)
        return false;

    --m_procCharges;

    if (GetCasterGuid() != m_target->GetObjectGuid() && IsAreaAura())
        if (Unit* caster = GetCaster())
            caster->RemoveAuraCharge(m_spellProto->Id);

    return m_procCharges == 0;
}

bool SpellAuraHolder::HasMechanic(uint32 mechanic) const
{
    if (mechanic == m_spellProto->Mechanic)
        return true;

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (m_auras[i] && m_spellProto->EffectMechanic[i] == mechanic)
            return true;
    return false;
}

bool SpellAuraHolder::HasMechanicMask(uint32 mechanicMask) const
{
    if (mechanicMask & (1 << (m_spellProto->Mechanic - 1)))
        return true;

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (m_auras[i] && m_spellProto->EffectMechanic[i] && ((1 << (m_spellProto->EffectMechanic[i] - 1)) & mechanicMask))
            return true;
    return false;
}

bool SpellAuraHolder::IsPersistent() const
{
    for (auto aur : m_auras)
        if (aur)
            if (aur->IsPersistent())
                return true;
    return false;
}

bool SpellAuraHolder::IsAreaAura() const
{
    for (auto aur : m_auras)
        if (aur)
            if (aur->IsAreaAura())
                return true;
    return false;
}

bool SpellAuraHolder::IsPositive() const
{
    for (auto aur : m_auras)
        if (aur)
            if (!aur->IsPositive())
                return false;
    return true;
}

bool SpellAuraHolder::IsEmptyHolder() const
{
    for (auto m_aura : m_auras)
        if (m_aura)
            return false;
    return true;
}

void SpellAuraHolder::UnregisterAndCleanupTrackedAuras()
{
    TrackedAuraType trackedType = GetTrackedAuraType();
    if (!trackedType)
        return;

    if (trackedType == TRACK_AURA_TYPE_SINGLE_TARGET)
    {
        if (Unit* caster = GetCaster())
            caster->GetTrackedAuraTargets(trackedType).erase(GetSpellProto());
    }
    else if (trackedType == TRACK_AURA_TYPE_CONTROL_VEHICLE)
    {
        Unit* caster = GetCaster();
        if (caster && IsSpellHaveAura(GetSpellProto(), SPELL_AURA_CONTROL_VEHICLE, GetAuraFlags()))
        {
            caster->GetTrackedAuraTargets(trackedType).erase(GetSpellProto());
        }
        else if (caster)
        {
            Unit::TrackedAuraTargetMap scTarget = caster->GetTrackedAuraTargets(trackedType);
            Unit::TrackedAuraTargetMap::iterator find = scTarget.find(GetSpellProto());
            if (find != scTarget.end())
            {
                ObjectGuid vehicleGuid = find->second;
                scTarget.erase(find);
                if (Unit* vehicle = caster->GetMap()->GetUnit(vehicleGuid))
                    vehicle->RemoveAurasByCasterSpell(GetSpellProto()->Id, caster->GetObjectGuid());
            }
        }
    }

    m_trackedAuraType = TRACK_AURA_TYPE_NOT_TRACKED;
}

void SpellAuraHolder::SetCreationDelayFlag()
{
    m_skipUpdate = true;
}
