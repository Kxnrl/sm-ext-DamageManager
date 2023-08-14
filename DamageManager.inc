#if defined _DamageManager_included
    #endinput
#endif
#define _DamageManager_included

/**
 * @file DamageManager.inc
 * @brief DamageManager extension header.
 */


enum /* filters_t */
{
    DamageFilter_DamageValid,
    DamageFilter_KillerValid,
    DamageFilter_WeaponValid,
    DamageFilter_DamageType,    // if you want to disable this filter, please set to -1
    DamageFilter_SkipSelf,
    DamageFilter_SkipDead,
    DamageFilter_VictimTeam,
    DamageFilter_KillerTeam,
    DAMAGE_FILTER_MAX_COUNT
};

/**
 * Defines a callback function for player damage events.
 *
 * @param victim			The victim's entity index.
 * @param killer			The killer's entity index.
 * @param inflictor			The inflictor's entity index.
 * @param weapon			The weapon's entity index.
 * @param flDamage			The amount of damage inflicted.
 * @param bitsDamageType 	The type of damage inflicted.
 */
typedef PlayerDamageCallback = function void(int victim, int killer, int inflictor, int weapon, float flDamage, int bitsDamageType);

/**
 * Installs a hook to intercept player damage events.
 *
 * @param callback 	The callback function to be called when a player takes damage.
 * @param filters  	An array of damage filters to apply to the damage event.
 * @return 			True if the hook was successfully installed, false otherwise.
 */
native bool DamageManager_InstallHook(PlayerDamageCallback callback, int filters[DAMAGE_FILTER_MAX_COUNT], int filterCount = DAMAGE_FILTER_MAX_COUNT);

/**
 * Removes a previously registered PlayerDamageCallback hook.
 *
 * @param callback 	The callback function to remove.
 * @return 			True if the hook was successfully removed, false otherwise.
 * @error  			The hook is undefined.
 */
native bool DamageManager_RemoveHook(PlayerDamageCallback callback);

/**
 * Takes damage from an attacker to a victim with a specified amount of damage, damage type, weapon, damage force and damage position.
 * @note                 This function will check CBaseEntity::PassesDamageFilter and CCSGameRules::AllowDamage.
 *
 * @param victim         victim index taking damage
 * @param inflictor      Inflictor entity index
 * @param attacker       Attacker entity index
 * @param damage         Amount of damage
 * @param damageType     Bitfield of damage types
 * @param weapon         Weapon index (orangebox and later) or -1 for unspecified
 * @param damageForce    Velocity of damage force
 * @param damagePosition Origin of damage
 * @error                Invalid entity, attacker, inflictor, or weapon entity.
 */
native void DamageManager_TakeDamage(int victim, int inflictor, int attacker, float damage, int damageType = 0, int weapon = -1, const float damageForce[3] = NULL_VECTOR, const float damagePosition[3] = NULL_VECTOR);

/**
 * Make sure damage filter allows the damage.
 *
 * @param entity         entity index
 * @param inflictor      Inflictor entity index
 * @param attacker       Attacker entity index
 * @param damage         Amount of damage
 * @param damageType     Bitfield of damage types
 * @param weapon         Weapon index (orangebox and later) or -1 for unspecified
 * @param damageForce    Velocity of damage force
 * @param damagePosition Origin of damage
 * * @return 			 True if passed damage filter, false otherwise.
 * @error                Invalid entity, attacker, inflictor, or weapon entity.
 */
native bool DamageManager_PassesDamageFilter(int entity, int inflictor, int attacker, float damage, int damageType = 0, int weapon = -1, const float damageForce[3] = NULL_VECTOR, const float damagePosition[3] = NULL_VECTOR);

/**
 * Retrieves the force of the last damage dealt to the player.
 *
 * @param vecForce 	The vector to store the force in.
 * @return 			True if the force was successfully retrieved, false otherwise.
 * @error  			Outside the of callback.
 */
native bool DamageManager_GetDamageForce(float vecForce[3]);

/**
 * Retrieves the position of the last damage dealt to the player.
 *
 * @param vecPosition 	The array to store the position in.
 * @return 				True if the position was successfully retrieved, false otherwise.
 * @error  				Outside of the callback.
 */
native bool DamageManager_GetDamagePosition(float vecPosition[3]);

/**
 * Returns the index of the hitbox that was hit during the last trace.
 *
 * @return The index of the hitbox that was hit during the last trace.
 * @error  Outside of the callback, damage not from bullet.
 */
native int DamageManager_GetCurrentTraceHitBox();

/**
 * Returns the current trace hit group.
 *
 * @return The current trace hit group as an integer.
 * @error  Outside of the callback, damage not from bullet.
 */
native int DamageManager_GetCurrentTraceHitGroup();

/**
 * Returns the last hit group of a client.
 *
 * @param client	The client index to get the last hit group of.
 * @return 			The last hit group of the client.
 */
native int DamageManager_GetLastHitGroup(int client);

/**
 * Returns the health of the client after the last damage taken.
 *
 * @param client	The client index to get the last damage health for.
 * @return 			The health of the client after the last damage taken.
 */
native int DamageManager_GetLastDamageHealth(int client);

/**
 * Returns the last damage dealt to the client's armor.
 *
 * @param client	The client index to get the last damage dealt to their armor.
 * @return			The amount of damage dealt to the client's armor.
 */
native int DamageManager_GetLastDamageArmor(int client);

/**
 * _________________Do not edit below this line!_______________________
 */
public Extension __ext_damage = {
    name = "DamageManager",
    file = "DamageManager.ext",
#if defined AUTOLOAD_EXTENSIONS
    autoload = 1,
#else
    autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
    required = 1,
#else
    required = 0,
#endif
};

#if !defined REQUIRE_EXTENSIONS

public void __ext_damage_SetNTVOptional()
{
    MarkNativeAsOptional("DamageManager_InstallHook");
    MarkNativeAsOptional("DamageManager_RemoveHook");
    MarkNativeAsOptional("DamageManager_TakeDamage");
    MarkNativeAsOptional("DamageManager_PassesDamageFilter");

    MarkNativeAsOptional("DamageManager_GetDamageForce");
    MarkNativeAsOptional("DamageManager_GetDamagePosition");

    MarkNativeAsOptional("DamageManager_GetCurrentTraceHitBox");
    MarkNativeAsOptional("DamageManager_GetCurrentTraceHitGroup");

    MarkNativeAsOptional("DamageManager_GetLastHitGroup");
    MarkNativeAsOptional("DamageManager_GetLastDamageHealth");
    MarkNativeAsOptional("DamageManager_GetLastDamageArmor");
}
#endif