/**
 * What scripts can ask of GameFixes_s2m (IGameFixes). SteamIDs are SteamID64 as
 * decimal text. A call to a fix that is off or failed to load does nothing and
 * answers false.
 */

#if defined _gamefixes_included
 #endinput
#endif
#define _gamefixes_included

methodmap GameFixes
{
	/**
	 * steam_ban: the engine's GC ban and competitive cooldown kick leave this account
	 * alone, like the whitelist in game_fixes.json, until SteamBanDisallowAccount or a
	 * plugin reload. An account the engine wants kicked is held for "hold_seconds"
	 * first, so a script that learns who the player is a moment later is in time.
	 */
	public static native bool SteamBanAllowAccount(const char[] steamId64);
	public static native bool SteamBanDisallowAccount(const char[] steamId64);

	/**
	 * server_list_players: the account is left out of what goes to Steam's server list
	 * from the next push on, until ServerListPlayersShow or a plugin reload. Steam keeps
	 * what it was last given for a player until they disconnect, so hide an account when
	 * it connects, not later.
	 */
	public static native bool ServerListPlayersHide(const char[] steamId64);
	public static native bool ServerListPlayersShow(const char[] steamId64);
}

public SharedPlugin __pl_GameFixes =
{
	name = "GameFixes",
	file = "game_fixes",
#if defined REQUIRE_PLUGIN
	required = 1,
#else
	required = 0,
#endif
};
