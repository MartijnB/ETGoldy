/*
* ET <-> *Lua* interface source file.
*
* This code is taken from ETPub and partially from NQ. All credits go to the teams especially to quad and pheno!
* http://etpub.org
* http://shitstorm.org
*
* Find the Lua code by doing a text seach for "FEATURE_LUA"
*/
#ifdef FEATURE_LUA

#include "g_lua.h"

extern field_t fields[];

// Goal: aiming for compatibility with ETPro lua mods
// http://wolfwiki.anime.net/index.php/Lua_Mod_API

lua_vm_t *lVM[LUA_NUM_VM];

void QDECL G_Lua_Printf(const char *fmt, ...)
{
	va_list argptr;
	char    buff[1024];

	va_start(argptr, fmt);
	Q_vsnprintf(buff, sizeof(buff), fmt, argptr);
	va_end(argptr);

	// cs: removed dedicated server restriction. its nice to see errors on listen server too.
	trap_Printf(buff);
}

void QDECL G_Lua_Printf(const char *fmt, ...) _attribute((format(printf, 1, 2)));

// gentity number = C_gentity_ptr_to_entNum( pointer-value )
// input:  pointer to a gentity (gentity*)
// output: the entity number.
//         if (input==0) return = -1
//         if (input address is out of g_entities[] memory range) return -1;
int C_gentity_ptr_to_entNum(unsigned long addr)
{
	// no NULL address,
	// address must also be in the range of the g_entities array memory space..
	// address must also be pointing to the start of an entity (invalid if it points just somewhere halfway into the entity)
	if (!addr ||
	    (gentity_t *)addr < &g_entities[0] || (gentity_t *)addr > &g_entities[MAX_GENTITIES - 1] ||
	    (addr - (unsigned long)&g_entities[0]) % sizeof(gentity_t) != 0)
	{
		return -1;
	}
	return ((gentity_t *)addr - g_entities);
}

/***************************/
/* Lua et library handlers */
/***************************/

// ET Library Calls
// et.RegisterModname( modname )
static int _et_RegisterModname(lua_State *L)
{
	const char *modname = luaL_checkstring(L, 1);

	if (modname)
	{
		lua_vm_t *vm = G_LuaGetVM(L);
		if (vm)
		{
			Q_strncpyz(vm->mod_name, modname, sizeof(vm->mod_name));
		}
	}
	return 0;
}

// vmnumber = et.FindSelf()
static int _et_FindSelf(lua_State *L)
{
	lua_vm_t *vm = G_LuaGetVM(L);

	if (vm)
	{
		lua_pushinteger(L, vm->id);
	}
	else
	{
		lua_pushnil(L);
	}
	return 1;
}

// modname, signature = et.FindMod( vmnumber )
static int _et_FindMod(lua_State *L)
{
	int      vmnumber = luaL_checkint(L, 1);
	lua_vm_t *vm      = lVM[vmnumber];

	if (vm)
	{
		lua_pushstring(L, vm->mod_name);
		lua_pushstring(L, vm->mod_signature);
	}
	else
	{
		lua_pushnil(L);
		lua_pushnil(L);
	}
	return 2;
}

// success = et.IPCSend( vmnumber, message )
static int _et_IPCSend(lua_State *L)
{
	int        vmnumber = luaL_checkint(L, 1);
	const char *message = luaL_checkstring(L, 2);

	lua_vm_t *sender = G_LuaGetVM(L);
	lua_vm_t *vm     = lVM[vmnumber];

	if (!vm || vm->err)
	{
		lua_pushinteger(L, 0);
		return 1;
	}

	// Find callback
	if (!G_LuaGetNamedFunction(vm, "et_IPCReceive"))
	{
		lua_pushinteger(L, 0);
		return 1;
	}

	// Arguments
	if (sender)
	{
		lua_pushinteger(vm->L, sender->id);
	}
	else
	{
		lua_pushnil(vm->L);
	}
	lua_pushstring(vm->L, message);

	// Call
	if (!G_LuaCall(vm, "et.IPCSend", 2, 0))
	{
		//G_LuaStopVM(vm);
		lua_pushinteger(L, 0);
		return 1;
	}

	// Success
	lua_pushinteger(L, 1);
	return 1;
}

// Printing
// et.G_Print( text )
static int _et_G_Print(lua_State *L)
{
	char text[1024];
	Q_strncpyz(text, luaL_checkstring(L, 1), sizeof(text));
	trap_Printf(text);
	return 0;
}

// et.G_LogPrint( text )
static int _et_G_LogPrint(lua_State *L)
{
	char text[1024];
	Q_strncpyz(text, luaL_checkstring(L, 1), sizeof(text));

	G_Lua_Printf("%s", text);

	// Additional logging
	if (level.logFile)
	{
		char string[1024];
/*
        if ( g_logOptions.integer & LOGOPTS_REALTIME )
        {
            Com_sprintf(string, sizeof(string), "%s %s", G_GetRealTime(), text);
        }
        else*/{
			int min, tens, sec;

			sec  = level.time / 1000;
			min  = sec / 60;
			sec -= min * 60;
			tens = sec / 10;
			sec -= tens * 10;

			Com_sprintf(string, sizeof(string), "%i:%i%i %s", min, tens, sec, text);
		}

		trap_FS_Write(string, strlen(string), level.logFile);
	}
	return 0;
}

// Argument Handling
// args = et.ConcatArgs( index )
static int _et_ConcatArgs(lua_State *L)
{
	int index = luaL_checkint(L, 1);

	lua_pushstring(L, ConcatArgs(index));
	return 1;
}

// argcount = et.trap_Argc()
static int _et_trap_Argc(lua_State *L)
{
	lua_pushinteger(L, trap_Argc());
	return 1;
}

// arg = et.trap_Argv( argnum )
static int _et_trap_Argv(lua_State *L)
{
	char buff[MAX_STRING_CHARS];
	int  argnum = luaL_checkint(L, 1);

	trap_Argv(argnum, buff, sizeof(buff));
	lua_pushstring(L, buff);
	return 1;
}

// Cvars
// cvarvalue = et.trap_Cvar_Get( cvarname )
static int _et_trap_Cvar_Get(lua_State *L)
{
	char       buff[MAX_CVAR_VALUE_STRING];
	const char *cvarname = luaL_checkstring(L, 1);

	trap_Cvar_VariableStringBuffer(cvarname, buff, sizeof(buff));
	lua_pushstring(L, buff);
	return 1;
}

// et.trap_Cvar_Set( cvarname, cvarvalue )
static int _et_trap_Cvar_Set(lua_State *L)
{
	const char *cvarname  = luaL_checkstring(L, 1);
	const char *cvarvalue = luaL_checkstring(L, 2);

	trap_Cvar_Set(cvarname, cvarvalue);
	return 0;
}

// Added the "reason" field to give more freedom about the output
// The old way to call the function still works, because "reason" is an optional arg
// et.MutePlayer( clientnum, duration, reason )
// duration is in seconds.
static int _et_MutePlayer(lua_State *L)
{
	int        clientnum = luaL_checkint(L, 1);
	gentity_t  *ent      = g_entities + clientnum;
	int        duration  = luaL_checkint(L, 2);
	const char *reason   = luaL_optstring(L, 3, NULL);

	if (!ent->client)
	{
		return 0;
	}

	ent->client->sess.muted = qtrue;

	//ClientConfigStringChanged( ent );

	if (duration == -1)
	{
		if (reason == NULL)
		{
			CPx(clientnum, va("print \"^9You've been muted by Lua.\n\""));
			AP(va("chat \"%s^9 has been muted by Lua.\"", ent->client->pers.netname));
		}
		else
		{
			CPx(clientnum, va("print \"^9You've been muted by Lua. %s\n\"", reason));
			AP(va("chat \"%s^9 has been muted by Lua. %s\"", ent->client->pers.netname, reason));
		}
	}
	else
	{
		if (reason == NULL)
		{
			CPx(clientnum, va("print \"^9You've been muted for %d seconds by Lua.\n\"", duration));
			AP(va("chat \"%s^9 has been muted for %d seconds by Lua.\"", ent->client->pers.netname, duration));
		}
		else
		{
			CPx(clientnum, va("print \"^9You've been muted for %d seconds by Lua. %s\n\"", duration, reason));
			AP(va("chat \"%s^9 has been muted for %d seconds by Lua. %s\"", ent->client->pers.netname, duration, reason));
		}
	}
	return 0;
}

// et.UnmutePlayer( clientnum )
// added the output messages.
static int _et_UnmutePlayer(lua_State *L)
{
	int       clientnum = luaL_checkint(L, 1);
	gentity_t *ent      = g_entities + clientnum;

	if (!ent->client)
	{
		return 0;
	}

	ent->client->sess.muted = qfalse;

	// ClientConfigStringChanged( ent );
	CPx(clientnum, "print \"^5You've been auto-unmuted. Lua penalty lifted.\n\"");
	AP(va("chat \"%s^7 has been auto-unmuted. Lua penalty lifted.\"", ent->client->pers.netname));
	return 0;
}

// Config Strings
// configstringvalue = et.trap_GetConfigstring( index )
static int _et_trap_GetConfigstring(lua_State *L)
{
	char buff[MAX_STRING_CHARS];
	int  index = luaL_checkint(L, 1);

	trap_GetConfigstring(index, buff, sizeof(buff));
	lua_pushstring(L, buff);
	return 1;
}

// et.trap_SetConfigstring( index, configstringvalue )
static int _et_trap_SetConfigstring(lua_State *L)
{
	int        index = luaL_checkint(L, 1);
	const char *csv  = luaL_checkstring(L, 2);

	trap_SetConfigstring(index, csv);
	return 0;
}

// Server
// et.trap_SendConsoleCommand( when, command )
static int _et_trap_SendConsoleCommand(lua_State *L)
{
	int        when = luaL_checkint(L, 1);
	const char *cmd = luaL_checkstring(L, 2);

	trap_SendConsoleCommand(when, cmd);
	return 0;
}

// Clients
// et.trap_DropClient( clientnum, reason, ban_time )
static int _et_trap_DropClient(lua_State *L)
{
	int        clientnum = luaL_checkint(L, 1);
	const char *reason   = luaL_checkstring(L, 2);
	int        ban       = trap_Cvar_VariableIntegerValue("g_defaultBanTime");

	ban = luaL_optint(L, 3, ban);
	trap_DropClient(clientnum, reason, ban);
	return 0;
}

// et.trap_SendServerCommand( clientnum, command )
static int _et_trap_SendServerCommand(lua_State *L)
{
	int        clientnum = luaL_checkint(L, 1);
	const char *cmd      = luaL_checkstring(L, 2);

	trap_SendServerCommand(clientnum, cmd);
	return 0;
}

// et.G_Say( clientNum, mode, text )
static int _et_G_Say(lua_State *L)
{
	int        clientnum = luaL_checkint(L, 1);
	int        mode      = luaL_checkint(L, 2);
	const char *text     = luaL_checkstring(L, 3);

	G_Say(g_entities + clientnum, NULL, mode, text);
	return 0;
}

// et.ClientUserinfoChanged( clientNum )
static int _et_ClientUserinfoChanged(lua_State *L)
{
	int clientnum = luaL_checkint(L, 1);

	ClientUserinfoChanged(clientnum);
	return 0;
}

// clientnum = et.ClientNumberFromString( string )
// searches for one partial match with 'string', if one is found the clientnum
// is returned, if there is none or more than one match nil is returned
static int _et_ClientNumberFromString(lua_State *L)
{
	const char *search = luaL_checkstring(L, 1);
	int        pids[MAX_CLIENTS];

	// only send exact matches, otherwise -1
	if (ClientNumbersFromString((char *) search, pids) == 1)
	{
		lua_pushinteger(L, pids[0]);
	}
	else
	{
		lua_pushnil(L);
	}
	return 1;
}

// et.isBitSet(bit,value)
// little helper for accessing bitmask values
// if bit 'bit' is set in 'value', true is returned, else false
static int _et_isBitSet(lua_State *L)
{
	int b = luaL_checkint(L, 1);
	int v = luaL_checkint(L, 2);

	if (v & b)
	{
		lua_pushboolean(L, 1);
	}
	else
	{
		lua_pushboolean(L, 0);
	}
	return 1;
}

// Userinfo
// userinfo = et.trap_GetUserinfo( clientnum )
static int _et_trap_GetUserinfo(lua_State *L)
{
	char buff[MAX_STRING_CHARS];
	int  clientnum = luaL_checkint(L, 1);

	trap_GetUserinfo(clientnum, buff, sizeof(buff));
	lua_pushstring(L, buff);
	return 1;
}

// et.trap_SetUserinfo( clientnum, userinfo )
static int _et_trap_SetUserinfo(lua_State *L)
{
	int        clientnum = luaL_checkint(L, 1);
	const char *userinfo = luaL_checkstring(L, 2);

	trap_SetUserinfo(clientnum, userinfo);
	return 0;
}

// String Utility Functions
// infostring = et.Info_RemoveKey( infostring, key )
static int _et_Info_RemoveKey(lua_State *L)
{
	char       buff[MAX_INFO_STRING];
	const char *key = luaL_checkstring(L, 2);

	Q_strncpyz(buff, luaL_checkstring(L, 1), sizeof(buff));
	Info_RemoveKey(buff, key);
	lua_pushstring(L, buff);
	return 1;
}

// infostring = et.Info_SetValueForKey( infostring, key, value )
static int _et_Info_SetValueForKey(lua_State *L)
{
	char       buff[MAX_INFO_STRING];
	const char *key   = luaL_checkstring(L, 2);
	const char *value = luaL_checkstring(L, 3);

	Q_strncpyz(buff, luaL_checkstring(L, 1), sizeof(buff));
	Info_SetValueForKey(buff, key, value);
	lua_pushstring(L, buff);
	return 1;
}

// keyvalue = et.Info_ValueForKey( infostring, key )
static int _et_Info_ValueForKey(lua_State *L)
{
	const char *infostring = luaL_checkstring(L, 1);
	const char *key        = luaL_checkstring(L, 2);

	lua_pushstring(L, Info_ValueForKey(infostring, key));
	return 1;
}

// cleanstring = et.Q_CleanStr( string )
static int _et_Q_CleanStr(lua_State *L)
{
	char buff[MAX_STRING_CHARS];

	Q_strncpyz(buff, luaL_checkstring(L, 1), sizeof(buff));
	Q_CleanStr(buff);
	lua_pushstring(L, buff);
	return 1;
}

extern char bigTextBuffer[100000];

// ET Filesystem
static int _et_trap_FS_GetFileList(lua_State *L)
{
	const char   *dirname = luaL_checkstring(L, 1);
	const char   *filename_extension = luaL_checkstring(L, 2);

	int newTable, index, i;

	int  filelen;
	char filename[MAX_QPATH]; // was 128
	char *filenameptr = bigTextBuffer;

	int  numfiles = trap_FS_GetFileList(dirname, filename_extension, bigTextBuffer, sizeof(bigTextBuffer));

	lua_createtable(L, numfiles, 0);
    newTable = lua_gettop(L);
    index = 1;

	for (i = 0; i < numfiles; i++, filenameptr += filelen + 1)
	{
		filelen = strlen(filenameptr);
		strcpy(filename, filenameptr);

		lua_pushstring(L, filename);
        lua_rawseti(L, newTable, index++);
	}

	return 1;
}

// fd, len = et.trap_FS_FOpenFile( filename, mode )
static int _et_trap_FS_FOpenFile(lua_State *L)
{
	fileHandle_t fd;
	int          len;
	const char   *filename = luaL_checkstring(L, 1);
	int          mode      = luaL_checkint(L, 2);

	len = trap_FS_FOpenFile(filename, &fd, mode);
	lua_pushinteger(L, fd);
	lua_pushinteger(L, len);
	return 2;
}

// filedata = et.trap_FS_Read( fd, count )
static int _et_trap_FS_Read(lua_State *L)
{
	char         *filedata = "";
	fileHandle_t fd        = luaL_checkint(L, 1);
	int          count     = luaL_checkint(L, 2);

	filedata = malloc(count + 1);
	trap_FS_Read(filedata, count, fd);
	*(filedata + count) = '\0';
	lua_pushstring(L, filedata);
	return 1;
}

// count = et.trap_FS_Write( filedata, count, fd )
static int _et_trap_FS_Write(lua_State *L)
{
	const char *filedata = luaL_checkstring(L, 1);
	int        count     = luaL_checkint(L, 2);

	fileHandle_t fd = luaL_checkint(L, 3);
	lua_pushinteger(L, trap_FS_Write(filedata, count, fd));
	return 1;
}

// et.trap_FS_Rename( oldname, newname )
static int _et_trap_FS_Rename(lua_State *L)
{
	const char *oldname = luaL_checkstring(L, 1);
	const char *newname = luaL_checkstring(L, 2);

	trap_FS_Rename(oldname, newname);
	return 0;
}

// et.trap_FS_FCloseFile( fd )
static int _et_trap_FS_FCloseFile(lua_State *L)
{
	fileHandle_t fd = luaL_checkint(L, 1);
	trap_FS_FCloseFile(fd);
	return 0;
}

// Indexes
// soundindex = et.G_SoundIndex( filename )
static int _et_G_SoundIndex(lua_State *L)
{
	const char *filename = luaL_checkstring(L, 1);

	lua_pushinteger(L, G_SoundIndex(filename));
	return 1;
}

// modelindex = et.G_ModelIndex( filename )
static int _et_G_ModelIndex(lua_State *L)
{
	const char *filename = luaL_checkstring(L, 1);

	lua_pushinteger(L, G_ModelIndex((char *)filename));
	return 1;
}

// Sound
// et.G_globalSound( sound )
static int _et_G_globalSound(lua_State *L)
{
	const char *sound = luaL_checkstring(L, 1);

	G_globalSound((char *)sound);
	return 0;
}

// et.G_Sound( entnum, soundindex )
static int _et_G_Sound(lua_State *L)
{
	int entnum     = luaL_checkint(L, 1);
	int soundindex = luaL_checkint(L, 2);

	G_Sound(g_entities + entnum, soundindex);
	return 0;
}

// et.G_ClientSound( clientnum, soundindex )
static int _et_G_ClientSound(lua_State *L)
{
	int clientnum  = luaL_checkint(L, 1);
	int soundindex = luaL_checkint(L, 2);

	G_ClientSound(g_entities + clientnum, soundindex);
	return 0;
}

// Miscellaneous {{{
// milliseconds = et.trap_Milliseconds()
static int _et_trap_Milliseconds(lua_State *L)
{
	lua_pushinteger(L, trap_Milliseconds());
	return 1;
}

// et.G_Damage( target, inflictor, attacker, damage, dflags, mod )
static int _et_G_Damage(lua_State *L)
{
	int target    = luaL_checkint(L, 1);
	int inflictor = luaL_checkint(L, 2);
	int attacker  = luaL_checkint(L, 3);
	int damage    = luaL_checkint(L, 4);
	int dflags    = luaL_checkint(L, 5);
	int mod       = luaL_checkint(L, 6);

	G_Damage(g_entities + target,
	         g_entities + inflictor,
	         g_entities + attacker,
	         NULL,
	         NULL,
	         damage,
	         dflags,
	         mod);

	return 0;
}

// et.G_AddSkillPoints( ent, skill, points )
static int _et_G_AddSkillPoints(lua_State *L)
{
	gentity_t *ent   = g_entities + luaL_checkint(L, 1);
	int       skill  = luaL_checkint(L, 2);
	float     points = luaL_checknumber(L, 3);

	G_AddSkillPoints(ent, skill, points);
	return 0;
}

// et.G_LoseSkillPoints( ent, skill, points )
static int _et_G_LoseSkillPoints(lua_State *L)
{
	gentity_t *ent   = g_entities + luaL_checkint(L, 1);
	int       skill  = luaL_checkint(L, 2);
	float     points = luaL_checknumber(L, 3);

	G_LoseSkillPoints(ent, skill, points);
	return 0;
}

// Entities
// client entity fields
static const gentity_field_t gclient_fields[] =
{

	_et_gclient_addfield(acc,                            FIELD_FLOAT,       FIELD_FLAG_READONLY),

	_et_gclient_addfield(lasthurt_mod,                   FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(pers.playerStats.suicides,      FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(lastKillTime,                   FIELD_INT,         FIELD_FLAG_READONLY),

	_et_gclient_addfield(ps.eFlags,                      FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(ps.pm_flags,                    FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(ps.pm_time,                     FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(deathAnimTime,                  FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(pers.lastBattleSenseBonusTime,  FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(pers.lastHQMineReportTime,      FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(pers.maxHealth,                 FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(respawnTime,                    FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(saved_persistant,               FIELD_INT_ARRAY,   FIELD_FLAG_READONLY),
	_et_gclient_addfield(PCSpecialPickedUpCount,         FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(ps.weaponstate,                 FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(combatState,                    FIELD_INT,         FIELD_FLAG_READONLY),

	_et_gclient_addfield(inactivityTime,                 FIELD_INT,         0),
	_et_gclient_addfield(inactivityWarning,              FIELD_INT,         0),

	// to be compatible with ETPro:
	_et_gclient_addfieldalias(client.inactivityTime,     inactivityTime,    FIELD_INT,           0),
	_et_gclient_addfieldalias(client.inactivityWarning,  inactivityWarning, FIELD_INT,           0),

	_et_gclient_addfield(pers.connected,                 FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(pers.netname,                   FIELD_STRING,      FIELD_FLAG_NOPTR),
	_et_gclient_addfield(pers.localClient,               FIELD_INT,         0),
	_et_gclient_addfield(pers.initialSpawn,              FIELD_INT,         0),
	_et_gclient_addfield(pers.enterTime,                 FIELD_INT,         0),
	_et_gclient_addfield(pers.connectTime,               FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(pers.teamState.state,           FIELD_INT,         0),
	_et_gclient_addfield(pers.voteCount,                 FIELD_INT,         0),
	_et_gclient_addfield(pers.complaints,                FIELD_INT,         0),
	_et_gclient_addfield(pers.complaintClient,           FIELD_INT,         0),
	_et_gclient_addfield(pers.complaintEndTime,          FIELD_INT,         0),
	_et_gclient_addfield(pers.lastReinforceTime,         FIELD_INT,         0),
	_et_gclient_addfield(pers.applicationClient,         FIELD_INT,         0),
	_et_gclient_addfield(pers.applicationEndTime,        FIELD_INT,         0),
	_et_gclient_addfield(pers.invitationClient,          FIELD_INT,         0),
	_et_gclient_addfield(pers.invitationEndTime,         FIELD_INT,         0),
	_et_gclient_addfield(pers.propositionClient,         FIELD_INT,         0),
	_et_gclient_addfield(pers.propositionClient2,        FIELD_INT,         0),
	_et_gclient_addfield(pers.propositionEndTime,        FIELD_INT,         0),
	_et_gclient_addfield(pers.autofireteamEndTime,       FIELD_INT,         0),
	_et_gclient_addfield(pers.autofireteamCreateEndTime, FIELD_INT,         0),
	_et_gclient_addfield(pers.autofireteamJoinEndTime,   FIELD_INT,         0),
	_et_gclient_addfield(pers.lastSpawnTime,             FIELD_INT,         FIELD_FLAG_READONLY),
	_et_gclient_addfield(pers.ready,                     FIELD_INT,         0),
	_et_gclient_addfield(pers.playerStats.hitRegions,    FIELD_INT_ARRAY,   FIELD_FLAG_READONLY),

	_et_gclient_addfield(ps.stats,                       FIELD_INT_ARRAY,   0),
	_et_gclient_addfield(ps.persistant,                  FIELD_INT_ARRAY,   0),
	_et_gclient_addfield(ps.ping,                        FIELD_INT,         0),
	_et_gclient_addfield(ps.powerups,                    FIELD_INT_ARRAY,   0),
	_et_gclient_addfield(ps.origin,                      FIELD_VEC3,        0),
	_et_gclient_addfield(ps.ammo,                        FIELD_INT_ARRAY,   0),
	_et_gclient_addfield(ps.ammoclip,                    FIELD_INT_ARRAY,   0),

	// same order as in g_local.h
	_et_gclient_addfield(sess.sessionTeam,               FIELD_INT,         0),
	_et_gclient_addfield(sess.spectatorTime,             FIELD_INT,         0),
	_et_gclient_addfield(sess.spectatorState,            FIELD_INT,         0),
	_et_gclient_addfield(sess.spectatorClient,           FIELD_INT,         0),
	_et_gclient_addfield(sess.playerType,                FIELD_INT,         0),
	_et_gclient_addfield(sess.playerWeapon,              FIELD_INT,         0),
	_et_gclient_addfield(sess.playerWeapon2,             FIELD_INT,         0),
	_et_gclient_addfield(sess.spawnObjectiveIndex,       FIELD_INT,         0),
	_et_gclient_addfield(sess.latchPlayerType,           FIELD_INT,         0),
	_et_gclient_addfield(sess.latchPlayerWeapon,         FIELD_INT,         0),
	_et_gclient_addfield(sess.latchPlayerWeapon2,        FIELD_INT,         0),
	_et_gclient_addfield(sess.ignoreClients,             FIELD_INT_ARRAY,   0),

	_et_gclient_addfield(sess.muted,                     FIELD_INT,         0),

	_et_gclient_addfield(sess.skillpoints,               FIELD_FLOAT_ARRAY, FIELD_FLAG_READONLY),
	_et_gclient_addfield(sess.startskillpoints,          FIELD_FLOAT_ARRAY, FIELD_FLAG_READONLY),
	_et_gclient_addfield(sess.startxptotal,              FIELD_FLOAT,       FIELD_FLAG_READONLY),
	_et_gclient_addfield(sess.skill,                     FIELD_INT_ARRAY,   0),
	_et_gclient_addfield(sess.rank,                      FIELD_INT,         0),
	_et_gclient_addfield(sess.medals,                    FIELD_INT_ARRAY,   0),
	_et_gclient_addfield(sess.coach_team,                FIELD_INT,         0),
	_et_gclient_addfield(sess.damage_given,              FIELD_INT,         0),
	_et_gclient_addfield(sess.damage_received,           FIELD_INT,         0),
	_et_gclient_addfield(sess.deaths,                    FIELD_INT,         0),
	_et_gclient_addfield(sess.game_points,               FIELD_INT,         0),
	_et_gclient_addfield(sess.kills,                     FIELD_INT,         0),
	_et_gclient_addfield(sess.referee,                   FIELD_INT,         0),
	_et_gclient_addfield(sess.rounds,                    FIELD_INT,         0),
	_et_gclient_addfield(sess.spec_invite,               FIELD_INT,         0),
	_et_gclient_addfield(sess.spec_team,                 FIELD_INT,         0),
	_et_gclient_addfield(sess.suicides,                  FIELD_INT,         0),
	_et_gclient_addfield(sess.team_damage,               FIELD_INT,         0),
	_et_gclient_addfield(sess.team_kills,                FIELD_INT,         0),
	_et_gclient_addfield(sess.uci,                       FIELD_INT,         0),

	_et_gclient_addfield(sess.aWeaponStats,              FIELD_WEAPONSTAT,  0),
	// TODO
	//_et_gclient_addfieldalias(aWeaponStats, sess.aWeaponStats, FIELD_WEAPONSTAT_EXT, FIELD_FLAG_READONLY),


	// origin: use ps.origin instead of r.currentOrigin
	// for client entities
	_et_gclient_addfieldalias(origin,                    ps.origin,         FIELD_VEC3,          0),


	// missing sess.semiadmin
	// missing sess.gibs
	// _et_gclient_addfield(sess.team_damage_given, FIELD_INT, 0),
	// _et_gclient_addfield(sess.team_damage_received, FIELD_INT, 0),
	// _et_gclient_addfieldalias(sess.team_damage, sess.team_damage_given, FIELD_INT, 0),
	// _et_gclient_addfieldalias(sess.team_received, sess.team_damage_received, FIELD_INT, 0),

	{ NULL },
};

// entity fields
// R/W access see see http://wolfwiki.anime.net/index.php/Fieldname
static const gentity_field_t gentity_fields[] =
{
	_et_gentity_addfield(activator,           FIELD_ENTITY,     FIELD_FLAG_READONLY),
	_et_gentity_addfield(chain,               FIELD_ENTITY,     0),
	_et_gentity_addfield(classname,           FIELD_STRING,     0),
	_et_gentity_addfield(clipmask,            FIELD_INT,        0),
	_et_gentity_addfield(closespeed,          FIELD_FLOAT,      0),
	_et_gentity_addfield(count,               FIELD_INT,        0),
	_et_gentity_addfield(count2,              FIELD_INT,        0),
	_et_gentity_addfield(damage,              FIELD_INT,        0),
	_et_gentity_addfield(deathType,           FIELD_INT,        0),
	_et_gentity_addfield(delay,               FIELD_FLOAT,      0),
	_et_gentity_addfield(dl_atten,            FIELD_INT,        0),
	_et_gentity_addfield(dl_color,            FIELD_VEC3,       0),
	_et_gentity_addfield(dl_shader,           FIELD_STRING,     FIELD_FLAG_READONLY),
	_et_gentity_addfield(dl_stylestring,      FIELD_STRING,     FIELD_FLAG_READONLY),
	_et_gentity_addfield(duration,            FIELD_FLOAT,      0),
	_et_gentity_addfield(end_size,            FIELD_INT,        0),
	_et_gentity_addfield(entstate,			  FIELD_INT,		0),
	_et_gentity_addfield(enemy,               FIELD_ENTITY,     0),
	_et_gentity_addfield(flags,               FIELD_INT,        FIELD_FLAG_READONLY),
	_et_gentity_addfield(harc,                FIELD_FLOAT,      0),
	_et_gentity_addfield(health,              FIELD_INT,        0),
	_et_gentity_addfield(inuse,               FIELD_INT,        0),
	_et_gentity_addfield(isProp,              FIELD_INT,        FIELD_FLAG_READONLY),
	_et_gentity_addfield(item,                FIELD_STRING,     FIELD_FLAG_READONLY),
	_et_gentity_addfield(key,                 FIELD_INT,        0),
	_et_gentity_addfield(message,             FIELD_STRING,     0),
	_et_gentity_addfield(methodOfDeath,       FIELD_INT,        0),
	_et_gentity_addfield(mg42BaseEnt,         FIELD_INT,        0),
	_et_gentity_addfield(missionLevel,        FIELD_INT,        0),
	_et_gentity_addfield(model,               FIELD_STRING,     FIELD_FLAG_READONLY),
	_et_gentity_addfield(model2,              FIELD_STRING,     FIELD_FLAG_READONLY),
	_et_gentity_addfield(nextTrain,           FIELD_ENTITY,     0),
	_et_gentity_addfield(noise_index,         FIELD_INT,        0),
	_et_gentity_addfield(prevTrain,           FIELD_ENTITY,     0),
	_et_gentity_addfield(props_frame_state,   FIELD_INT,        FIELD_FLAG_READONLY),

	_et_gentity_addfield(r.absmax,            FIELD_VEC3,       FIELD_FLAG_READONLY),
	_et_gentity_addfield(r.absmin,            FIELD_VEC3,       FIELD_FLAG_READONLY),
	_et_gentity_addfield(r.bmodel,            FIELD_INT,        FIELD_FLAG_READONLY),
	_et_gentity_addfield(r.contents,          FIELD_INT,        0),
	_et_gentity_addfield(r.currentAngles,     FIELD_VEC3,       0),
	_et_gentity_addfield(r.currentOrigin,     FIELD_VEC3,       0),
	_et_gentity_addfield(r.eventTime,         FIELD_INT,        0),
	// _et_gentity_addfield(r.linkcount, FIELD_INT, FIELD_FLAG_READONLY), // IRATA: not used, kept for ET compatibility, so we don't provide it
	_et_gentity_addfield(r.linked,            FIELD_INT,        FIELD_FLAG_READONLY),
	_et_gentity_addfield(r.maxs,              FIELD_VEC3,       0),
	_et_gentity_addfield(r.mins,              FIELD_VEC3,       0),
	_et_gentity_addfield(r.ownerNum,          FIELD_INT,        0),
	_et_gentity_addfield(r.singleClient,      FIELD_INT,        0),
	_et_gentity_addfield(r.svFlags,           FIELD_INT,        0),
	_et_gentity_addfield(r.worldflags,        FIELD_INT,        FIELD_FLAG_READONLY),

	_et_gentity_addfield(radius,              FIELD_INT,        0),
	_et_gentity_addfield(random,              FIELD_FLOAT,      0),
	_et_gentity_addfield(rotate,              FIELD_VEC3,       0),

	_et_gentity_addfield(s.angles,            FIELD_VEC3,       0),
	_et_gentity_addfield(s.angles2,           FIELD_VEC3,       0),
	_et_gentity_addfield(s.apos,              FIELD_TRAJECTORY, 0),
	_et_gentity_addfield(s.clientNum,         FIELD_INT,        0),
	_et_gentity_addfield(s.constantLight,     FIELD_INT,        0),
	_et_gentity_addfield(s.density,           FIELD_INT,        0),
	_et_gentity_addfield(s.dl_intensity,      FIELD_INT,        0),
	_et_gentity_addfield(s.dmgFlags,          FIELD_INT,        0),
	_et_gentity_addfield(s.eFlags,            FIELD_INT,        0),
	_et_gentity_addfield(s.eType,             FIELD_INT,        0),
	_et_gentity_addfield(s.effect1Time,       FIELD_INT,        0),
	_et_gentity_addfield(s.effect2Time,       FIELD_INT,        0),
	_et_gentity_addfield(s.effect3Time,       FIELD_INT,        0),
	_et_gentity_addfield(s.frame,             FIELD_INT,        0),
	_et_gentity_addfield(s.groundEntityNum,   FIELD_INT,        FIELD_FLAG_READONLY),
	_et_gentity_addfield(s.loopSound,         FIELD_INT,        0),
	_et_gentity_addfield(s.modelindex,        FIELD_INT,        0),
	_et_gentity_addfield(s.modelindex2,       FIELD_INT,        0),
	_et_gentity_addfield(s.number,            FIELD_INT,        FIELD_FLAG_READONLY),
	_et_gentity_addfield(s.onFireEnd,         FIELD_INT,        0),
	_et_gentity_addfield(s.onFireStart,       FIELD_INT,        0),
	_et_gentity_addfield(s.origin,            FIELD_VEC3,       0),
	_et_gentity_addfield(s.origin2,           FIELD_VEC3,       0),
	_et_gentity_addfield(s.pos,               FIELD_TRAJECTORY, 0),
	_et_gentity_addfield(s.powerups,          FIELD_INT,        FIELD_FLAG_READONLY),
	_et_gentity_addfield(s.solid,             FIELD_INT,        0),
	_et_gentity_addfield(s.teamNum,           FIELD_INT,        0),
	_et_gentity_addfield(s.time,              FIELD_INT,        0),
	_et_gentity_addfield(s.time2,             FIELD_INT,        0),
	_et_gentity_addfield(s.weapon,            FIELD_INT,        FIELD_FLAG_READONLY),
	_et_gentity_addfield(s.eventParm,         FIELD_INT,        0),

	_et_gentity_addfield(scriptName,          FIELD_STRING,     FIELD_FLAG_READONLY),
	_et_gentity_addfield(spawnflags,          FIELD_INT,        FIELD_FLAG_READONLY),
	_et_gentity_addfield(spawnitem,           FIELD_STRING,     FIELD_FLAG_READONLY),
	_et_gentity_addfield(speed,               FIELD_INT,        0),
	_et_gentity_addfield(splashDamage,        FIELD_INT,        0),
	_et_gentity_addfield(splashMethodOfDeath, FIELD_INT,        0),
	_et_gentity_addfield(splashRadius,        FIELD_INT,        0),
	_et_gentity_addfield(start_size,          FIELD_INT,        0),
	_et_gentity_addfield(tagName,             FIELD_STRING,     FIELD_FLAG_NOPTR + FIELD_FLAG_READONLY),
	_et_gentity_addfield(tagParent,           FIELD_ENTITY,     0),
	_et_gentity_addfield(takedamage,          FIELD_INT,        0),
	_et_gentity_addfield(tankLink,            FIELD_ENTITY,     0),
	_et_gentity_addfield(target,              FIELD_STRING,     0),
	_et_gentity_addfield(TargetAngles,        FIELD_VEC3,       0),
	_et_gentity_addfield(TargetFlag,          FIELD_INT,        FIELD_FLAG_READONLY),
	_et_gentity_addfield(targetname,          FIELD_STRING,     FIELD_FLAG_READONLY),
	_et_gentity_addfield(teamchain,           FIELD_ENTITY,     0),
	_et_gentity_addfield(teammaster,          FIELD_ENTITY,     0),
	_et_gentity_addfield(track,               FIELD_STRING,     FIELD_FLAG_READONLY),
	_et_gentity_addfield(varc,                FIELD_FLOAT,      0),
	_et_gentity_addfield(wait,                FIELD_FLOAT,      0),
	_et_gentity_addfield(waterlevel,          FIELD_INT,        FIELD_FLAG_READONLY),
	_et_gentity_addfield(watertype,           FIELD_INT,        FIELD_FLAG_READONLY),

	// To be compatible with ETPro:
	// origin: use r.currentOrigin instead of ps.origin
	//         for non client entities
	_et_gentity_addfieldalias(origin,         r.currentOrigin,  FIELD_VEC3,                             0),
	{ NULL },
};

// gentity fields helper functions
static gentity_field_t *_et_gentity_getfield(gentity_t *ent, char *fieldname)
{
	int i;

	// search through client fields first
	if (ent->client)
	{
		for (i = 0; gclient_fields[i].name; i++)
		{
			if (Q_stricmp(fieldname, gclient_fields[i].name) == 0)
			{
				return (gentity_field_t *)&gclient_fields[i];
			}
		}
	}

	for (i = 0; gentity_fields[i].name; i++)
	{
		if (Q_stricmp(fieldname, gentity_fields[i].name) == 0)
		{
			return (gentity_field_t *)&gentity_fields[i];
		}
	}

	return 0;
}

static void _et_gentity_getvec3(lua_State *L, vec3_t vec3)
{
	lua_newtable(L);
	lua_pushnumber(L, vec3[0]);
	lua_rawseti(L, -2, 1);
	lua_pushnumber(L, vec3[1]);
	lua_rawseti(L, -2, 2);
	lua_pushnumber(L, vec3[2]);
	lua_rawseti(L, -2, 3);
}

static void _et_gentity_setvec3(lua_State *L, vec3_t *vec3)
{
	lua_pushnumber(L, 1);
	lua_gettable(L, -2);
	(*vec3)[0] = (float)lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_pushnumber(L, 2);
	lua_gettable(L, -2);
	(*vec3)[1] = (float)lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_pushnumber(L, 3);
	lua_gettable(L, -2);
	(*vec3)[2] = (float)lua_tonumber(L, -1);
	lua_pop(L, 1);
}

static void _et_gentity_gettrajectory(lua_State *L, trajectory_t *traj)
{
	int index;

	lua_newtable(L);
	index = lua_gettop(L);
	lua_pushstring(L, "trType");
	lua_pushinteger(L, traj->trType);
	lua_settable(L, -3);
	lua_pushstring(L, "trTime");
	lua_pushinteger(L, traj->trTime);
	lua_settable(L, -3);
	lua_pushstring(L, "trDuration");
	lua_pushinteger(L, traj->trDuration);
	lua_settable(L, -3);
	lua_settop(L, index);
	lua_pushstring(L, "trBase");
	_et_gentity_getvec3(L, traj->trBase);
	lua_settable(L, -3);
	lua_settop(L, index);
	lua_pushstring(L, "trDelta");
	_et_gentity_getvec3(L, traj->trDelta);
	lua_settable(L, -3);
}

static void _et_gentity_settrajectory(lua_State *L, trajectory_t *traj)
{
	lua_pushstring(L, "trType");
	lua_gettable(L, -2);
	traj->trType = (trType_t)lua_tointeger(L, -1);
	lua_pop(L, 1);
	lua_pushstring(L, "trTime");
	lua_gettable(L, -2);
	traj->trTime = lua_tointeger(L, -1);
	lua_pop(L, 1);
	lua_pushstring(L, "trDuration");
	lua_gettable(L, -2);
	traj->trDuration = lua_tointeger(L, -1);
	lua_pop(L, 1);
	lua_pushstring(L, "trBase");
	lua_gettable(L, -2);
	_et_gentity_setvec3(L, (vec3_t *)traj->trBase);
	lua_pop(L, 1);
	lua_pushstring(L, "trDelta");
	lua_gettable(L, -2);
	_et_gentity_setvec3(L, (vec3_t *)traj->trDelta);
	lua_pop(L, 1);
}

static void _et_gentity_getweaponstat(lua_State *L, weapon_stat_t *ws)
{
	lua_newtable(L);
	lua_pushinteger(L, 1);
	lua_pushinteger(L, ws->atts);
	lua_settable(L, -3);
	lua_pushinteger(L, 2);
	lua_pushinteger(L, ws->deaths);
	lua_settable(L, -3);
	lua_pushinteger(L, 3);
	lua_pushinteger(L, ws->headshots);
	lua_settable(L, -3);
	lua_pushinteger(L, 4);
	lua_pushinteger(L, ws->hits);
	lua_settable(L, -3);
	lua_pushinteger(L, 5);
	lua_pushinteger(L, ws->kills);
	lua_settable(L, -3);
}

// entnum = et.G_Spawn()
static int _et_G_Spawn(lua_State *L)
{
	gentity_t *entnum = G_Spawn();

	lua_pushinteger(L, entnum - g_entities);
	return 1;
}

// entnum = et.G_TempEntity( origin, event )
static int _et_G_TempEntity(lua_State *L)
{
	vec3_t origin;
	int    event = luaL_checkint(L, 2);

	lua_pop(L, 1);
	_et_gentity_setvec3(L, &origin);
	lua_pushinteger(L, G_TempEntity(origin, event) - g_entities);
	return 1;
}

// et.G_FreeEntity( entnum )
static int _et_G_FreeEntity(lua_State *L)
{
	int entnum = luaL_checkint(L, 1);

	G_FreeEntity(g_entities + entnum);
	// a succesful LUA function has to return 1
	return 1;
}

// et.G_EntitiesFree()
static int _et_G_EntitiesFree(lua_State *L)
{
	lua_pushinteger(L, G_EntitiesFree());
	return 1;
}

// entnum = et.G_SpawnGEntityFromSV({key="value"})
static int _et_G_SpawnGEntityFromSV(lua_State *L)
{
	const char* key;
	const char* value;

	gentity_t *new_ent;

	level.numSpawnVars     = 0;
	level.numSpawnVarChars = 0;
	
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_pushnil(L);

	while(lua_next(L, 1) != 0)
	{
		/* uses 'key' (at index -2) and 'value' (at index -1) */
		//G_Printf("%s - %s\n", lua_typename(L, lua_type(L, -2)),	lua_typename(L, lua_type(L, -1)));

		key = luaL_checkstring(L, -2);

		if (key == NULL)
		{
			luaL_error(L, "Invalid search key!");
			return 0;
		}

		value = (char*)lua_tostring(L, -1);

		if (value == NULL)
		{
			luaL_error(L, "Invalid search key!");
			return 0;
		}

		if (level.numSpawnVars == MAX_SPAWN_VARS)
		{
			G_Error("G_ScriptAction_Create(): MAX_SPAWN_VARS");
		}

		level.spawnVars[level.numSpawnVars][0] = G_AddSpawnVarToken(key);
		level.spawnVars[level.numSpawnVars][1] = G_AddSpawnVarToken(value);

		level.numSpawnVars++;

		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(L, 1);
	}

	lua_pop(L, 1);

	level.spawning = qtrue;

	new_ent = G_SpawnGEntityFromSpawnVars();

	level.spawning = qfalse;

	trap_LinkEntity(new_ent);

	lua_pushinteger(L, new_ent->s.number);
	return 1;
}

static void _load_fields_for_entity(lua_State *L, gentity_t* ent)
{
	int i, innertable, innerinnertable;
	unsigned long   addr;
	const gentity_field_t* field;

	if (!lua_checkstack(L, 1))
	{
		G_Error("%s\n", "Couldn't allocate memory to extend LUA stack!");
	}

	lua_newtable(L);
	innertable = lua_gettop(L);

	for (i = 0; i < (((0.0f + sizeof(gentity_fields)) / sizeof(gentity_fields[0])) - 1); i++)
	{
		field = &gentity_fields[i];

		if (!lua_checkstack(L, 2))
		{
			G_Error("%s\n", "Couldn't allocate memory to extend LUA stack!");
		}

		addr = (field->flags & FIELD_FLAG_GENTITY) ? (unsigned long)ent : (unsigned long)ent->client;

		// for NULL entities, return nil (prevents server crashes!)
		if (!addr)
		{
			lua_pushnil(L);
			lua_setfield(L, innertable, field->name);

			continue;
		}

		addr += (unsigned long)field->mapping;

		switch (field->type)
		{
		case FIELD_ENTITY:
		case FIELD_TRAJECTORY:
			lua_pushnil(L);
			break;

		case FIELD_INT:
			lua_pushnumber(L, *(int *)addr);
			break;

		case FIELD_FLOAT:
			lua_pushnumber(L, *(float *)addr);
			break;

		case FIELD_STRING:
			if (field->flags & FIELD_FLAG_NOPTR)
			{
				lua_pushstring(L, (char *) ((byte *)addr));
			}
			else
			{
				lua_pushstring(L, *(char **) ((byte *)addr));
			}

			break;

		case FIELD_VEC3:
			if (!lua_checkstack(L, 4))
			{
				G_Error("%s\n", "Couldn't allocate memory to extend LUA stack!");
			}

			lua_newtable(L);
			innerinnertable = lua_gettop(L);
			lua_pushnumber(L, (*(vec3_t *)addr)[0]);
			lua_rawseti(L, innerinnertable, 1);
			lua_pushnumber(L, (*(vec3_t *)addr)[1]);
			lua_rawseti(L, innerinnertable, 2);
			lua_pushnumber(L, (*(vec3_t *)addr)[2]);
			lua_rawseti(L, innerinnertable, 3);
			break;

		default:
			lua_pushnil(L);

			G_Printf("Entity type mapping missing: %s!\n", field->name);
			break;
		}

		lua_setfield(L, innertable, field->name);
	}
}

// ent count = et.G_CountEntities()
static int _et_G_CountEntities(lua_State *L)
{
	lua_pushinteger(L, G_CountEntities());
	return 1;
}

// {key="value"} = et.G_Entity(entnum)
static int _et_G_Entity(lua_State *L)
{
	int entnum = luaL_checkint(L, 1);
	gentity_t* ent;

	if (entnum >= MAX_GENTITIES)
	{
		lua_pushnil(L);
		return 1;
	}

	ent = &g_entities[entnum];

	if (!ent->inuse)
	{
		lua_pushnil(L);
		return 1;
	}

	_load_fields_for_entity(L, ent);
	
	return 1;
}

// {{key="value"}} = et.G_SearchEntities({}) or {{key="value"}} = et.G_SearchEntities({filterKey="filterValue"})
static int _et_G_SearchEntities(lua_State *L) 
{
	int i, returntable;
	char* tmpstr;
	char* tmpstr2;
	vec3_t tmpvec3;
	const char* fieldname;
	qboolean matchedentities[MAX_GENTITIES];
	gentity_t* ent;
	gentity_field_t* ent_field;
	unsigned long   addr;
	
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_pushnil(L);

	if (lua_next(L, 1) != 0) 
	{
		memset(matchedentities, qtrue, sizeof(matchedentities));

		do
		{
			/* uses 'key' (at index -2) and 'value' (at index -1) */
			//G_Printf("%s - %s\n", lua_typename(L, lua_type(L, -2)),	lua_typename(L, lua_type(L, -1)));

			fieldname = luaL_checkstring(L, -2);

			if (fieldname == NULL)
			{
				luaL_error(L, "Invalid search key!");
				return 0;
			}

			for (i = 0; i < MAX_GENTITIES; i++)
			{
				if (matchedentities[i]) 
				{
					ent = &g_entities[i];

					if (ent->inuse)
					{
						ent_field = _et_gentity_getfield(ent, (char*)fieldname);

						// break on invalid gentity field
						if (!ent_field)
						{
							luaL_error(L, "tried to get invalid gentity field \"%s\"", fieldname);
							return 0;
						}

						addr = (ent_field->flags & FIELD_FLAG_GENTITY) ? (unsigned long)ent : (unsigned long)ent->client;

						// for NULL entities, return nil (prevents server crashes!)
						if (!addr)
						{
							matchedentities[i] = qfalse;
							continue;
						}

						addr += (unsigned long)ent_field->mapping;

						switch (ent_field->type)
						{
						case FIELD_INT:
							if (lua_type(L, -1) == LUA_TNUMBER) 
							{
								if (lua_tonumber(L, -1) != (*(int *)addr))
								{
									matchedentities[i] = qfalse;
								}
							}
							else if (lua_type(L, -1) == LUA_TBOOLEAN) 
							{
								if ((lua_toboolean(L, -1) ? qtrue : qfalse) != (*(int *)addr))
								{
									matchedentities[i] = qfalse;
								}
							}
							else
							{
								luaL_error(L, "Expected integer but found %s for field \"%s\"", lua_typename(L, lua_type(L, -1)), fieldname);
								return 0;
							}
							break;

						case FIELD_FLOAT:
							if (lua_type(L, -1) == LUA_TNUMBER) 
							{
								if (lua_tonumber(L, -1) != (*(float *)addr))
								{
									matchedentities[i] = qfalse;
								}
							}
							else
							{
								luaL_error(L, "Expected float but found %s for field \"%s\"", lua_typename(L, lua_type(L, -1)), fieldname);
								return 0;
							}
							break;

						case FIELD_STRING:
							if (lua_type(L, -1) == LUA_TSTRING) 
							{
								tmpstr = (ent_field->flags & FIELD_FLAG_NOPTR) ? (char *) ((byte *)addr) : *(char **) ((byte *)addr);
								tmpstr2 = (char*)lua_tostring(L, -1);

								if (Q_stricmp(tmpstr, tmpstr2) != 0)
								{
									matchedentities[i] = qfalse;
								}
							}
							else
							{
								luaL_error(L, "Expected string but found %s for field \"%s\"", lua_typename(L, lua_type(L, -1)), fieldname);
								return 0;
							}
							break;

						case FIELD_VEC3:
							if (lua_type(L, -1) == LUA_TTABLE) 
							{
								_et_gentity_setvec3(L, &tmpvec3);

								if (tmpvec3[0] != (*(vec3_t *)addr)[0] || tmpvec3[1] != (*(vec3_t *)addr)[1] || tmpvec3[2] != (*(vec3_t *)addr)[2])
								{
									matchedentities[i] = qfalse;
								}
							}
							else
							{
								luaL_error(L, "Expected table but found %s for field \"%s\"", lua_typename(L, lua_type(L, -1)), fieldname);
								return 0;
							}
							break;

						default:
							matchedentities[i] = qfalse;

							G_Printf("Entity type mapping missing: %s!\n", ent_field->name);
							break;
						}
					}
					else
					{
						matchedentities[i] = qfalse;
					}

				}
			}

			/* removes 'value'; keeps 'key' for next iteration */
			lua_pop(L, 1);
		}
		while (lua_next(L, 1) != 0);

		lua_pop(L, 1);

		//return the matched entities
		lua_newtable(L);
		returntable = lua_gettop(L);

		for (i = 0; i < MAX_GENTITIES; i++)
		{
			if (matchedentities[i]) 
			{
				ent = &g_entities[i];

				if (ent->inuse)
				{
					_load_fields_for_entity(L, ent);

					lua_setfield(L, returntable, va("%i", i));
					//lua_rawseti(L, returntable, i); 
				}
			}
		}
	}
	else //return all entities
	{
		lua_newtable(L);
		returntable = lua_gettop(L);

		for (i = 0; i < MAX_GENTITIES; i++)
		{
			ent = &g_entities[i];

			if (ent->inuse)
			{
				_load_fields_for_entity(L, ent);

				lua_setfield(L, returntable, va("%i", i));
				//lua_rawseti(L, returntable, i); 
			}
		}
	}

	return 1;
}

// add G_GetSpawnVar
// spawnval = et.G_GetSpawnVar( entnum, key )
// This function works with fields ( g_spawn.c @ 72 )
//
// Description:
//   The mapper, using his map-editor, assigns spawnvars.
//   Spawnvars, and their values, are represented in code as members of gentity_t.
//   Spawnvar names can be different from the corresponding gentity_t membernames.
//   For example the spawnvar "shortname" is used with trigger_objective_info entities in the map-editor,
//    while in code the gentity_t membername is "message"..
//   This function _et_G_GetSpawnVar() returns the value of a gentity_t member,
//    where the argument is a spawnvar name.
//   (the array called "fields" in g_spawn.c is a mapping of spawnvars<->members)
static int _et_G_GetSpawnVar(lua_State *L)
{
	gentity_t   *ent;
	int         entnum = luaL_checkint(L, 1);
	const char  *key   = luaL_checkstring(L, 2);
	int         index  = GetFieldIndex((char *)key);
	fieldtype_t type   = GetFieldType((char *)key);
	int         ofs;

	// break on invalid gentity field
	if (index == -1)
	{
		luaL_error(L, "field \"%s\" index is -1", key);
		return 0;
	}

	if (entnum < 0 || entnum >= MAX_GENTITIES)
	{
		luaL_error(L, "entnum \"%d\" is out of range", entnum);
		return 0;
	}

	ent = &g_entities[entnum];

	// If the entity is not in use, return nil
	if (!ent->inuse)
	{
		lua_pushnil(L);
		return 1;
	}

	ofs = fields[index].ofs;

	switch (type)
	{
	case F_INT:
		lua_pushinteger(L, *(int *) ((byte *)ent + ofs));
		return 1;
	case F_FLOAT:
		lua_pushnumber(L, *(float *) ((byte *)ent + ofs));
		return 1;
	case F_LSTRING:
	case F_GSTRING:
		if (fields[index].flags & FIELD_FLAG_NOPTR)
		{
			lua_pushstring(L, (char *) ((byte *)ent + ofs));
		}
		else
		{
			lua_pushstring(L, *(char **) ((byte *)ent + ofs));
		}
		return 1;
	case F_VECTOR:
	case F_ANGLEHACK:
		_et_gentity_getvec3(L, *(vec3_t *)((byte *)ent + ofs));
		return 1;
	case F_ENTITY:
	{
		// core: return the entity-number  of the entity that the pointer is pointing at.
		int entNum = C_gentity_ptr_to_entNum(*(int *)((byte *)ent + ofs));

		if (entNum < 0)
		{
			lua_pushnil(L);
		}
		else
		{
			lua_pushinteger(L, entNum);
		}
	}
		return 1;
	case F_ITEM:
	case F_CLIENT:
	case F_IGNORE:
	default:
		lua_pushnil(L);
		return 1;
	}
	return 0;
}

// add G_SetSpawnVar
// et.G_SetSpawnVar( entnum, key, value )
// This function works with fields ( g_spawn.c @ 72 )
static int _et_G_SetSpawnVar(lua_State *L)
{
	gentity_t   *ent;
	int         entnum = luaL_checkint(L, 1);
	const char  *key   = luaL_checkstring(L, 2);
	int         index  = GetFieldIndex((char *)key);
	fieldtype_t type   = GetFieldType((char *)key);
	int         ofs;
	const char  *buffer;

	// break on invalid gentity field
	if (index == -1)
	{
		luaL_error(L, "field \"%s\" index is -1", key);
		return 0;
	}

	if (entnum < 0 || entnum >= MAX_GENTITIES)
	{
		luaL_error(L, "entnum \"%d\" is out of range", entnum);
		return 0;
	}

	ent = &g_entities[entnum];

	// If the entity is not in use, return nil
	if (!ent->inuse)
	{
		lua_pushnil(L);
		return 1;
	}

	ofs = fields[index].ofs;

	switch (type)
	{
	case F_INT:
		*(int *) ((byte *)ent + ofs) = luaL_checkint(L, 3);
		return 1;
	case F_FLOAT:
		*(float *) ((byte *)ent + ofs) = (float)luaL_checknumber(L, 3);
		return 1;
	case F_LSTRING:
	case F_GSTRING:
		buffer = luaL_checkstring(L, 3);
		if (fields[index].flags & FIELD_FLAG_NOPTR)
		{
			Q_strncpyz((char *)((byte *)ent + ofs), buffer, MAX_STRING_CHARS);
		}
		else
		{
			//free(*(char **)((byte *)ent + ofs));
			*(char **)((byte *)ent + ofs) = G_Alloc(strlen(buffer) + 1);
			Q_strncpyz(*(char **)((byte *)ent + ofs), buffer, strlen(buffer) + 1);
		}
		return 1;
	case F_VECTOR:
	case F_ANGLEHACK:
		_et_gentity_setvec3(L, (vec3_t *)((byte *)ent + ofs));
		return 1;
	case F_ENTITY:
		// pointer-fields are read-only..
		//*(gentity_t **)((byte *)ent + ofs) = g_entities + luaL_checkint(L, 3);
		return 0;
	case F_ITEM:
	case F_CLIENT:
	case F_IGNORE:
	default:
		lua_pushnil(L);
		return 1;
	}

	return 0;
}

// TODO:
// integer entnum = et.G_SpawnGEntityFromSpawnVars( string spawnvar, string spawnvalue, ... )

// et.trap_LinkEntity( entnum )
static int _et_trap_LinkEntity(lua_State *L)
{
	int entnum = luaL_checkint(L, 1);

	trap_LinkEntity(g_entities + entnum);
	return 0;
}

// et.trap_UnlinkEntity( entnum )
static int _et_trap_UnlinkEntity(lua_State *L)
{
	int entnum = luaL_checkint(L, 1);

	trap_UnlinkEntity(g_entities + entnum);
	return 0;
}

// (variable) = et.gentity_get( entnum, fieldname, arrayindex )
static int _et_gentity_get(lua_State *L)
{
	gentity_t       *ent       = g_entities + luaL_checkint(L, 1);
	const char      *fieldname = luaL_checkstring(L, 2);
	gentity_field_t *field     = _et_gentity_getfield(ent, (char *)fieldname);
	unsigned long   addr;

	// break on invalid gentity field
	if (!field)
	{
		luaL_error(L, "tried to get invalid gentity field \"%s\"", fieldname);
		return 0;
	}

	if (field->flags & FIELD_FLAG_GENTITY)
	{
		addr = (unsigned long)ent;
	}
	else
	{
		addr = (unsigned long)ent->client;
	}

	// for NULL entities, return nil (prevents server crashes!)
	if (!addr)
	{
		lua_pushnil(L);
		return 1;
	}

	addr += (unsigned long)field->mapping;

	switch (field->type)
	{
	case FIELD_INT:
		lua_pushinteger(L, *(int *)addr);
		return 1;
	case FIELD_STRING:
		if (field->flags & FIELD_FLAG_NOPTR)
		{
			lua_pushstring(L, (char *)addr);
		}
		else
		{
			lua_pushstring(L, *(char **)addr);
		}
		return 1;
	case FIELD_FLOAT:
		lua_pushnumber(L, *(float *)addr);
		return 1;
	case FIELD_ENTITY:
	{
		// core: return the entity-number  of the entity that the pointer is pointing at.
		int entNum = C_gentity_ptr_to_entNum(*(int *)addr);

		if (entNum < 0)
		{
			lua_pushnil(L);
		}
		else
		{
			lua_pushinteger(L, entNum);
		}
	}
		return 1;
	case FIELD_VEC3:
		_et_gentity_getvec3(L, *(vec3_t *)addr);
		return 1;
	case FIELD_INT_ARRAY:
		lua_pushinteger(L, (*(int *)(addr + (sizeof(int) * luaL_optint(L, 3, 0)))));
		return 1;
	case FIELD_TRAJECTORY:
		_et_gentity_gettrajectory(L, (trajectory_t *)addr);
		return 1;
	case FIELD_FLOAT_ARRAY:
		lua_pushnumber(L, (*(float *)(addr + (sizeof(int) * luaL_optint(L, 3, 0)))));
		return 1;
	case FIELD_WEAPONSTAT:
		_et_gentity_getweaponstat(L, (weapon_stat_t *)(addr + (sizeof(weapon_stat_t) * luaL_optint(L, 3, 0))));
		return 1;

	}
	return 0;
}

// et.gentity_set( entnum, fieldname, arrayindex, (value) )
static int _et_gentity_set(lua_State *L)
{
	gentity_t       *ent       = g_entities + luaL_checkint(L, 1);
	const char      *fieldname = luaL_checkstring(L, 2);
	gentity_field_t *field     = _et_gentity_getfield(ent, (char *)fieldname);
	unsigned long   addr;
	const char      *buffer;

	// break on invalid gentity field
	if (!field)
	{
		luaL_error(L, "tried to set invalid gentity field \"%s\"", fieldname);
		return 0;
	}

	// break on read-only gentity field
	if (field->flags & FIELD_FLAG_READONLY)
	{
		luaL_error(L, "tried to set read-only gentity field \"%s\"", fieldname);
		return 0;
	}

	if (field->flags & FIELD_FLAG_GENTITY)
	{
		addr = (unsigned long)ent;
	}
	else
	{
		addr = (unsigned long)ent->client;
	}

	// for NULL entities, return nil (prevents server crashes!)
	if (!addr)
	{
		lua_pushnil(L);
		return 1;
	}

	addr += (unsigned long)field->mapping;

	switch (field->type)
	{
	case FIELD_INT:
		*(int *)addr = luaL_checkint(L, 3);
		break;
	case FIELD_STRING:
		buffer = luaL_checkstring(L, 3);
		if (field->flags & FIELD_FLAG_NOPTR)
		{
			Q_strncpyz((char *)addr, buffer, strlen((char *)addr));
		}
		else
		{
			//free(*(char **)addr);
			*(char **)addr = G_Alloc(strlen(buffer) + 1);
			Q_strncpyz(*(char **)addr, buffer, strlen(buffer) + 1);
		}
		break;
	case FIELD_FLOAT:
		*(float *)addr = (float)luaL_checknumber(L, 3);
		break;
	case FIELD_ENTITY:
		// pointer-fields are read-only..
		break;
	case FIELD_VEC3:
		_et_gentity_setvec3(L, (vec3_t *)addr);
		break;
	case FIELD_INT_ARRAY:
		*(int *)(addr + (sizeof(int) * luaL_checkint(L, 3))) = luaL_checkint(L, 4);
		break;
	case FIELD_TRAJECTORY:
		_et_gentity_settrajectory(L, (trajectory_t *)addr);
		break;
	case FIELD_FLOAT_ARRAY:
		*(float *)(addr + (sizeof(int) * luaL_checkint(L, 3))) = luaL_checknumber(L, 4);
		return 1;
	}
	return 0;
}

// et.G_AddEvent( ent, event, eventparm )
static int _et_G_AddEvent(lua_State *L)
{
	int ent       = luaL_checkint(L, 1);
	int event     = luaL_checkint(L, 2);
	int eventparm = luaL_checkint(L, 3);
	G_AddEvent(g_entities + ent, event, eventparm);
	return 0;
}

/*
 * et.G_XP_Set ( clientNum , xp, skill, add )
 */
static int _et_G_XP_Set(lua_State *L)
{
	gentity_t *ent      = NULL;
	int       clientNum = luaL_checkint(L, 1);
	int       xp        = luaL_checkint(L, 2);
	int       skill     = luaL_checkint(L, 3);
	int       add       = luaL_checkint(L, 4); // 'add' just checks to be 0 or not to be 0

	ent = &g_entities[clientNum];

	// Did comment the following lines to set XP via Lua on client connect()
	// - If used on connect() a moment later the rest of the entity data is set, and the entity data is valid
	// - If a client is not 'inuse' and this function is called the client is not in game for real
	//   and the data should be overwritten again, when the next player uses this client num/slot

	// Check if the entity is valid
	//if ( !ent->inuse ) {
	//	luaL_error(L, "clientNum \"%d\" is not an used entity", clientNum);
	//	return 0;
	//}

	// Check if the entity is a client
	if (!ent->client)
	{
		luaL_error(L, "clientNum \"%d\" is not a client entity", clientNum);
		return 0;
	}

	// Check if the skill is in the range
	if (skill < 0 || skill > SK_NUM_SKILLS - 1)
	{
		luaL_error(L, "\"skill\" must be a number from 0 to 6 both included");
		return 0;
	}

	// Check if the xp value is negative
	if (xp < 0)
	{
		luaL_error(L, "negative xp values are not allowed");
		return 0;
	}

	// special case for 0 adds
	if (add == 0)
	{
		float oldxp = ent->client->sess.skillpoints[skill];

		ent->client->sess.skillpoints[skill] = (float)xp;
		// ent->client->sess.mapstartSkillpoints[skill] = (float)xp;
		ent->client->sess.startxptotal -= oldxp;
		ent->client->sess.startxptotal += (float)xp;
	}
	else
	{
		ent->client->sess.skillpoints[skill] += (float)xp;
		// ent->client->sess.mapstartSkillpoints[skill] += (float)xp;
		ent->client->sess.startxptotal += (float)xp;
	}

	ent->client->ps.stats[STAT_XP] = (int)ent->client->sess.startxptotal;

	G_CalcRank(ent->client);
	BG_PlayerStateToEntityState(&ent->client->ps, &ent->s, level.time, qtrue);

	return 1;
}

/*
 * Reset XP
 * et.ResetXP ( clientNum )
 */
static int _et_G_ResetXP(lua_State *L)
{
	int       entnum = luaL_optint(L, 1, -1);
	gentity_t *ent   = NULL;

	if (entnum > -1)
	{
		ent = g_entities + entnum;
	}
	G_ResetXP(ent);
	return 1;
}

// et library initialisation array
static const luaL_Reg etlib[] =
{
	// ET Library Calls
	{ "RegisterModname",         _et_RegisterModname         },
	{ "FindSelf",                _et_FindSelf                },
	{ "FindMod",                 _et_FindMod                 },
	{ "IPCSend",                 _et_IPCSend                 },
	// Printing
	{ "G_Print",                 _et_G_Print                 },
	{ "G_LogPrint",              _et_G_LogPrint              },
	// Argument Handling
	{ "ConcatArgs",              _et_ConcatArgs              },
	{ "trap_Argc",               _et_trap_Argc               },
	{ "trap_Argv",               _et_trap_Argv               },
	// Cvars
	{ "trap_Cvar_Get",           _et_trap_Cvar_Get           },
	{ "trap_Cvar_Set",           _et_trap_Cvar_Set           },
	// Muting
	{ "MutePlayer",              _et_MutePlayer              },
	{ "UnmutePlayer",            _et_UnmutePlayer            },
	// Config Strings
	{ "trap_GetConfigstring",    _et_trap_GetConfigstring    },
	{ "trap_SetConfigstring",    _et_trap_SetConfigstring    },
	// Server
	{ "trap_SendConsoleCommand", _et_trap_SendConsoleCommand },
	// Clients
	{ "trap_DropClient",         _et_trap_DropClient         },
	{ "trap_SendServerCommand",  _et_trap_SendServerCommand  },
	//	{"trap_SendMessage",			_et_trap_SendMessage},
	//	{"trap_MessageStatus",			_et_trap_MessageStatus},
	{ "G_Say",                   _et_G_Say                   },
	{ "ClientUserinfoChanged",   _et_ClientUserinfoChanged   },
	{ "ClientNumberFromString",  _et_ClientNumberFromString  },
	{ "isBitSet",                _et_isBitSet                },
	// Userinfo
	{ "trap_GetUserinfo",        _et_trap_GetUserinfo        },
	{ "trap_SetUserinfo",        _et_trap_SetUserinfo        },
	// String Utility Functions
	{ "Info_RemoveKey",          _et_Info_RemoveKey          },
	{ "Info_SetValueForKey",     _et_Info_SetValueForKey     },
	{ "Info_ValueForKey",        _et_Info_ValueForKey        },
	{ "Q_CleanStr",              _et_Q_CleanStr              },
	// ET Filesystem
	{ "trap_FS_GetFileList",     _et_trap_FS_GetFileList     },
	{ "trap_FS_FOpenFile",       _et_trap_FS_FOpenFile       },
	{ "trap_FS_Read",            _et_trap_FS_Read            },
	{ "trap_FS_Write",           _et_trap_FS_Write           },
	{ "trap_FS_Rename",          _et_trap_FS_Rename          },
	{ "trap_FS_FCloseFile",      _et_trap_FS_FCloseFile      },
	// Indexes
	{ "G_SoundIndex",            _et_G_SoundIndex            },
	{ "G_ModelIndex",            _et_G_ModelIndex            },
	// Sound
	{ "G_globalSound",           _et_G_globalSound           },
	{ "G_Sound",                 _et_G_Sound                 },
	{ "G_ClientSound",           _et_G_ClientSound           },
	// Miscellaneous
	{ "trap_Milliseconds",       _et_trap_Milliseconds       },
	{ "G_Damage",                _et_G_Damage                },
	{ "G_AddSkillPoints",        _et_G_AddSkillPoints        },
	{ "G_LoseSkillPoints",       _et_G_LoseSkillPoints       },
	// Entities
	{ "G_Spawn",                 _et_G_Spawn                 },
	{ "G_SpawnGEntityFromSV",    _et_G_SpawnGEntityFromSV    },
	{ "G_TempEntity",            _et_G_TempEntity            },
	{ "G_FreeEntity",            _et_G_FreeEntity            },

	{ "G_Entity",				 _et_G_Entity                },
	{ "G_CountEntities",		 _et_G_CountEntities		 },
	{ "G_SearchEntities",		 _et_G_SearchEntities		 },

	{ "G_GetSpawnVar",           _et_G_GetSpawnVar           },

	{ "G_SetSpawnVar",           _et_G_SetSpawnVar           },
	{ "G_EntitiesFree",          _et_G_EntitiesFree          },
	{ "trap_LinkEntity",         _et_trap_LinkEntity         },
	{ "trap_UnlinkEntity",       _et_trap_UnlinkEntity       },
	{ "gentity_get",             _et_gentity_get             },
	{ "gentity_set",             _et_gentity_set             },
	{ "G_AddEvent",              _et_G_AddEvent              },

	// XP functions
	{ "G_XP_Set",                _et_G_XP_Set                },
	{ "G_ResetXP",               _et_G_ResetXP               },

	{ NULL },
};

/*************/
/* Lua API   */
/*************/

/*
 * G_LuaInit()
 * Initialises the Lua API interface
 */
qboolean G_LuaInit(void)
{
	char         allowedModules[MAX_CVAR_VALUE_STRING];
	int          i, num_vm = 0, len, flen = 0;
	char         buff[MAX_CVAR_VALUE_STRING], *crt, *code, *signature;
	fileHandle_t f;
	lua_vm_t     *vm;

	if (!lua_modules.string[0])
	{
		return qtrue;
	}

	Q_strncpyz(allowedModules, Q_strupr(lua_allowedModules.string), sizeof(allowedModules));

	Q_strncpyz(buff, lua_modules.string, sizeof(buff));
	len = strlen(buff);
	crt = buff;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		lVM[i] = NULL;
	}

	for (i = 0; i <= len; i++)
	{
		if (buff[i] == ' ' || buff[i] == '\0' || buff[i] == ',' || buff[i] == ';')
		{
			buff[i] = '\0';

			// try to open lua file
			flen = trap_FS_FOpenFile(crt, &f, FS_READ);
			if (flen < 0)
			{
				G_Lua_Printf("Lua API: can not open file %s\n", crt);
			}
			else if (flen > LUA_MAX_FSIZE)
			{
				// Let's not load arbitrarily big files to memory.
				// If your lua file exceeds the limit, let me know.
				G_Lua_Printf("Lua API: ignoring file %s (too big)\n", crt);
				trap_FS_FCloseFile(f);
			}
			else
			{
				code = malloc(flen + 1);
				trap_FS_Read(code, flen, f);
				*(code + flen) = '\0';
				trap_FS_FCloseFile(f);
				signature = G_SHA1(code);

				if (Q_stricmp(lua_allowedModules.string, "") && !strstr(allowedModules, signature))
				{
					// don't load disallowed lua modules into vm
					free(code); // fixed memory leaking in Lua API - thx ETPub/goesa
					G_Lua_Printf("Lua API: Lua module [%s] [%s] disallowed by ACL\n", crt, signature);
				}
				else
				{
					// Init lua_vm_t struct
					vm     = (lua_vm_t *) malloc(sizeof(lua_vm_t));
					vm->id = -1;
					Q_strncpyz(vm->file_name, crt, sizeof(vm->file_name));
					Q_strncpyz(vm->mod_name, "", sizeof(vm->mod_name));
					Q_strncpyz(vm->mod_signature, signature, sizeof(vm->mod_signature));
					vm->code      = code;
					vm->code_size = flen;
					vm->err       = 0;

					// Start lua virtual machine
					if (G_LuaStartVM(vm) == qfalse)
					{
						G_LuaStopVM(vm);
						vm = NULL;
					}
					else
					{
						vm->id      = num_vm;
						lVM[num_vm] = vm;
						num_vm++;
					}
				}
			}

			// prepare for next iteration
			if (i + 1 < len)
			{
				crt = buff + i + 1;
			}
			else
			{
				crt = NULL;
			}
			if (num_vm >= LUA_NUM_VM)
			{
				G_Lua_Printf("Lua API: too many lua files specified, only the first %d have been loaded\n", LUA_NUM_VM);
				break;
			}
		}
	}
	return qtrue;
}

/*
 * G_LuaCall( func, vm, nargs, nresults )
 * Calls a function already on the stack.
 */
qboolean G_LuaCall(lua_vm_t *vm, char *func, int nargs, int nresults)
{
	switch (lua_pcall(vm->L, nargs, nresults, 0))
	{
	case LUA_ERRRUN:
		// made output more ETPro compatible
		G_Lua_Printf("Lua API: %s error running lua script: %s\n", func, lua_tostring(vm->L, -1));
		lua_pop(vm->L, 1);
		vm->err++;
		return qfalse;
	case LUA_ERRMEM:
		G_Lua_Printf("Lua API: memory allocation error #2 ( %s )\n", vm->file_name);
		vm->err++;
		return qfalse;
	case LUA_ERRERR:
		G_Lua_Printf("Lua API: traceback error ( %s )\n", vm->file_name);
		vm->err++;
		return qfalse;
	default:
		return qtrue;
	}
	return qtrue;
}

/*
 * G_LuaGetNamedFunction( vm, name )
 * Finds a function by name and puts it onto the stack.
 * If the function does not exist, returns qfalse.
 */
qboolean G_LuaGetNamedFunction(lua_vm_t *vm, char *name)
{
	if (vm->L)
	{
		lua_getglobal(vm->L, name);
		if (lua_isfunction(vm->L, -1))
		{
			return qtrue;
		}
		else
		{
			lua_pop(vm->L, 1);
			return qfalse;
		}
	}
	return qfalse;
}

#ifdef LUA_5_2
// Lua 5.1 -> 5.2 compatible hack :)
// https://github.com/icgood/luafilesystem/commit/f634765b26c52d03aceed88c2130130ab43f6fa9
static void luaL_register(lua_State *L, const char *libname, const luaL_Reg *l)
{
	luaL_newlib(L, l);
	lua_pushvalue(L, -1);
	lua_setglobal(L, libname);
}
#endif

/*
 * G_LuaStartVM( vm )
 * Starts one individual virtual machine.
 */
qboolean G_LuaStartVM(lua_vm_t *vm)
{
	int        res = 0;
	char       basepath[MAX_QPATH];
	char       homepath[MAX_QPATH];
	char       gamepath[MAX_QPATH];
	const char *luaPath, *luaCPath;

	// Open a new lua state
	vm->L = luaL_newstate();
	if (!vm->L)
	{
		G_Lua_Printf("%s API: Lua failed to initialise.\n", LUA_VERSION);
		return qfalse;
	}

	// Initialise the lua state
	luaL_openlibs(vm->L);

	// set LUA_PATH and LUA_CPATH
	trap_Cvar_VariableStringBuffer("fs_basepath", basepath, sizeof(basepath));
	trap_Cvar_VariableStringBuffer("fs_homepath", homepath, sizeof(homepath));
	trap_Cvar_VariableStringBuffer("fs_game", gamepath, sizeof(gamepath));

	luaPath = va("%s%s%s%s?.lua;%s%s%s%slualibs%s?.lua",
	             homepath, LUA_DIRSEP, gamepath, LUA_DIRSEP,
	             homepath, LUA_DIRSEP, gamepath, LUA_DIRSEP, LUA_DIRSEP);

	luaCPath = va("%s%s%s%slualibs%s?.%s",
	              homepath, LUA_DIRSEP, gamepath, LUA_DIRSEP, LUA_DIRSEP, EXTENSION);

	// add fs_basepath if different from fs_homepath
	if (Q_stricmp(basepath, homepath))
	{
		luaPath = va("%s%s%s%s?.lua;%s%s%s%slualibs%s?.lua;%s",
		             basepath, LUA_DIRSEP, gamepath, LUA_DIRSEP,
		             basepath, LUA_DIRSEP, gamepath, LUA_DIRSEP, LUA_DIRSEP, luaPath);

		luaCPath = va("%s%s%s%slualibs%s?.%s;%s",
		              basepath, LUA_DIRSEP, gamepath, LUA_DIRSEP, LUA_DIRSEP, EXTENSION,
		              luaCPath);
	}

	lua_getglobal(vm->L, LUA_LOADLIBNAME);
	if (lua_istable(vm->L, -1))
	{
		lua_pushstring(vm->L, luaPath);
		lua_setfield(vm->L, -2, "path");
		lua_pushstring(vm->L, luaCPath);
		lua_setfield(vm->L, -2, "cpath");
	}
	lua_pop(vm->L, 1);

	// register globals
	lua_registerglobal(vm->L, "LUA_PATH", luaPath);
	lua_registerglobal(vm->L, "LUA_CPATH", luaCPath);

	lua_registerglobal(vm->L, "LUA_DIRSEP", LUA_DIRSEP);

	// register predefined constants
	lua_newtable(vm->L);
	lua_regconstinteger(vm->L, CS_PLAYERS);
	lua_regconstinteger(vm->L, EXEC_NOW);
	lua_regconstinteger(vm->L, EXEC_INSERT);
	lua_regconstinteger(vm->L, EXEC_APPEND);
	lua_regconstinteger(vm->L, FS_READ);
	lua_regconstinteger(vm->L, FS_WRITE);
	lua_regconstinteger(vm->L, FS_APPEND);
	lua_regconstinteger(vm->L, FS_APPEND_SYNC);
	lua_regconstinteger(vm->L, SAY_ALL);
	lua_regconstinteger(vm->L, SAY_TEAM);
	lua_regconstinteger(vm->L, SAY_BUDDY);
	lua_regconstinteger(vm->L, SAY_TEAMNL);
	lua_regconststring(vm->L, HOSTARCH);
	lua_setglobal(vm->L, "et");

	// register functions
	luaL_register(vm->L, "et", etlib); // void luaL_register (lua_State *L, const char *libname,  const luaL_Reg *l);

	// Load the code
	G_Lua_Printf("%s API: Loading %s\n", LUA_VERSION, vm->file_name);

	res = luaL_loadbuffer(vm->L, vm->code, vm->code_size, vm->file_name);
	if (res == LUA_ERRSYNTAX)
	{
		G_Lua_Printf("%s API: syntax error during pre-compilation: %s\n", LUA_VERSION, lua_tostring(vm->L, -1));
		lua_pop(vm->L, 1);
		vm->err++;
		return qfalse;
	}
	else if (res == LUA_ERRMEM)
	{
		G_Lua_Printf("%s API: memory allocation error #1 ( %s )\n", LUA_VERSION, vm->file_name);
		vm->err++;
		return qfalse;
	}

	// Execute the code
	if (!G_LuaCall(vm, "G_LuaStartVM", 0, 0))
	{
		return qfalse;
	}

	return qtrue;
}

/*
 * G_LuaStopVM( vm )
 * Stops one virtual machine, and calls its et_Quit callback.
 */
void G_LuaStopVM(lua_vm_t *vm)
{
	if (vm == NULL)
	{
		return;
	}
	if (vm->code != NULL)
	{
		free(vm->code);
		vm->code = NULL;
	}
	if (vm->L)
	{
		if (G_LuaGetNamedFunction(vm, "et_Quit"))
		{
			G_LuaCall(vm, "et_Quit", 0, 0);
		}
		lua_close(vm->L);
		vm->L = NULL;
	}
	if (vm->id >= 0)
	{
		if (lVM[vm->id] == vm)
		{
			lVM[vm->id] = NULL;
		}
		if (!vm->err)
		{
			G_Lua_Printf("%s API: Lua module [%s] [%s] unloaded.\n", LUA_VERSION, vm->file_name, vm->mod_signature);
		}
	}
	free(vm);
}

/*
 * G_LuaShutdown()
 * Shuts down everything related to Lua API.
 */
void G_LuaShutdown(void)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			G_LuaStopVM(vm);
		}
	}
}

/*
 * G_LuaStatus( ent )
 * Prints information on the Lua virtual machines.
 */
void G_LuaStatus(gentity_t *ent)
{
	int i, cnt = 0;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		if (lVM[i])
		{
			cnt++;
		}
	}

	if (cnt == 0)
	{
		G_refPrintf(ent, "%s API: no scripts loaded.", LUA_VERSION);
		return;
	}
	else if (cnt == 1)
	{
		G_refPrintf(ent, "%s API: showing lua information ( 1 module loaded )", LUA_VERSION);
	}
	else
	{
		G_refPrintf(ent, "%s API: showing lua information ( %d modules loaded )", LUA_VERSION, cnt);
	}
	G_refPrintf(ent, "%-2s %-24s %-40s %-24s", "VM", "Modname", "Signature", "Filename");
	G_refPrintf(ent, "-- ------------------------ ---------------------------------------- ------------------------");
	for (i = 0; i < LUA_NUM_VM; i++)
	{
		if (lVM[i])
		{
			G_refPrintf(ent, "%2d %-24s %-40s %-24s", lVM[i]->id, lVM[i]->mod_name, lVM[i]->mod_signature, lVM[i]->file_name);
		}
	}
	G_refPrintf(ent, "-- ------------------------ ---------------------------------------- ------------------------");
}

/*
 * G_LuaGetVM
 * Retrieves the VM for a given lua_State
 */
lua_vm_t *G_LuaGetVM(lua_State *L)
{
	int i;

	for (i = 0; i < LUA_NUM_VM; i++)
		if (lVM[i] && lVM[i]->L == L)
		{
			return lVM[i];
		}
	return NULL;
}

/*****************************/
/* Lua API hooks / callbacks */
/*****************************/

/*
 * G_LuaHook_InitGame
 * et_InitGame( levelTime, randomSeed, restart ) callback
 */
void G_LuaHook_InitGame(int levelTime, int randomSeed, int restart)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_InitGame"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, levelTime);
			lua_pushinteger(vm->L, randomSeed);
			lua_pushinteger(vm->L, restart);
			// Call
			if (!G_LuaCall(vm, "et_InitGame", 3, 0))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
		}
	}
}

/*
 * G_LuaHook_ShutdownGame
 * et_ShutdownGame( restart )  callback
 */
void G_LuaHook_ShutdownGame(int restart)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_ShutdownGame"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, restart);
			// Call
			if (!G_LuaCall(vm, "et_ShutdownGame", 1, 0))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
		}
	}
}

/*
 * G_LuaHook_RunFrame
 * et_RunFrame( levelTime )  callback
 */
void G_LuaHook_RunFrame(int levelTime)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_RunFrame"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, levelTime);
			// Call
			if (!G_LuaCall(vm, "et_RunFrame", 1, 0))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
		}
	}
}

/*
 * G_LuaHook_ClientConnect
 * rejectreason = et_ClientConnect( clientNum, firstTime, isBot ) callback
 */
qboolean G_LuaHook_ClientConnect(int clientNum, qboolean firstTime, qboolean isBot, char *reason)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_ClientConnect"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, clientNum);
			lua_pushinteger(vm->L, (int)firstTime);
			lua_pushinteger(vm->L, (int)isBot);
			// Call
			if (!G_LuaCall(vm, "et_ClientConnect", 3, 1))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
			if (lua_isstring(vm->L, -1))
			{
				Q_strncpyz(reason, lua_tostring(vm->L, -1), MAX_STRING_CHARS);
				return qtrue;
			}
		}
	}
	return qfalse;
}

/*
 * G_LuaHook_ClientDisconnect
 * et_ClientDisconnect( clientNum ) callback
 */
void G_LuaHook_ClientDisconnect(int clientNum)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_ClientDisconnect"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, clientNum);
			// Call
			if (!G_LuaCall(vm, "et_ClientDisconnect", 1, 0))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
		}
	}
}

/*
 * G_LuaHook_ClientBegin
 * et_ClientBegin( clientNum ) callback
 */
void G_LuaHook_ClientBegin(int clientNum)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_ClientBegin"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, clientNum);
			// Call
			if (!G_LuaCall(vm, "et_ClientBegin", 1, 0))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
		}
	}
}

/*
 * G_LuaHook_ClientUserinfoChanged(int clientNum);
 * et_ClientUserinfoChanged( clientNum ) callback
 */
void G_LuaHook_ClientUserinfoChanged(int clientNum)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_ClientUserinfoChanged"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, clientNum);
			// Call
			if (!G_LuaCall(vm, "et_ClientUserinfoChanged", 1, 0))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
		}
	}
}

/*
 * G_LuaHook_ClientSpawn
 * et_ClientSpawn( clientNum, revived, teamChange, restoreHealth ) callback
 */
void G_LuaHook_ClientSpawn(int clientNum, qboolean revived, qboolean teamChange, qboolean restoreHealth)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_ClientSpawn"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, clientNum);
			lua_pushinteger(vm->L, (int)revived);
			lua_pushinteger(vm->L, (int)teamChange);
			lua_pushinteger(vm->L, (int)restoreHealth);
			// Call
			if (!G_LuaCall(vm, "et_ClientSpawn", 4, 0))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
		}
	}
}

/*
 * G_LuaHook_ClientCommand
 * intercepted = et_ClientCommand( clientNum, command ) callback
 */
qboolean G_LuaHook_ClientCommand(int clientNum, char *command)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_ClientCommand"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, clientNum);
			lua_pushstring(vm->L, command);
			// Call
			if (!G_LuaCall(vm, "et_ClientCommand", 2, 1))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
			if (lua_isnumber(vm->L, -1))
			{
				if (lua_tointeger(vm->L, -1) == 1)
				{
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
 * G_LuaHook_ConsoleCommand
 * intercepted = et_ConsoleCommand( command ) callback
 */
qboolean G_LuaHook_ConsoleCommand(char *command)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_ConsoleCommand"))
			{
				continue;
			}
			// Arguments
			lua_pushstring(vm->L, command);
			// Call
			if (!G_LuaCall(vm, "et_ConsoleCommand", 1, 1))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
			if (lua_isnumber(vm->L, -1))
			{
				if (lua_tointeger(vm->L, -1) == 1)
				{
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
 * G_LuaHook_UpgradeSkill
 * result = et_UpgradeSkill( cno, skill ) callback
 */
qboolean G_LuaHook_UpgradeSkill(int cno, skillType_t skill)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_UpgradeSkill"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, cno);
			lua_pushinteger(vm->L, (int)skill);
			// Call
			if (!G_LuaCall(vm, "et_UpgradeSkill", 2, 1))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
			if (lua_isnumber(vm->L, -1))
			{
				if (lua_tointeger(vm->L, -1) == -1)
				{
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
 * G_LuaHook_SetPlayerSkill
 * et_SetPlayerSkill( cno, skill ) callback
 */
qboolean G_LuaHook_SetPlayerSkill(int cno, skillType_t skill)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_SetPlayerSkill"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, cno);
			lua_pushinteger(vm->L, (int)skill);
			// Call
			if (!G_LuaCall(vm, "et_SetPlayerSkill", 2, 1))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
			if (lua_isnumber(vm->L, -1))
			{
				if (lua_tointeger(vm->L, -1) == -1)
				{
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
 * G_LuaHook_Print
 * et_Print( text ) callback
 */
void G_LuaHook_Print(char *text)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_Print"))
			{
				continue;
			}
			// Arguments
			lua_pushstring(vm->L, text);
			// Call
			if (!G_LuaCall(vm, "et_Print", 1, 0))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
		}
	}
}

/*
 * G_LuaHook_Obituary
 * (customObit) = et_Obituary( victim, killer, meansOfDeath ) callback
 *
 * Different to ETPub which supports custom obituaries
 * this is 'ETPro like' implementation
 */
// qboolean G_LuaHook_Obituary(int victim, int killer, int meansOfDeath, char *customObit)
qboolean G_LuaHook_Obituary(int victim, int killer, int meansOfDeath)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_Obituary"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, victim);

			lua_pushinteger(vm->L, killer);
			lua_pushinteger(vm->L, meansOfDeath);
			// Call
			if (!G_LuaCall(vm, "et_Obituary", 3, 1))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
			if (lua_isstring(vm->L, -1))
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}

// G_LuaHook_Damage
// et_Damage( target, attacker, damage, dflags, mod)
qboolean G_LuaHook_Damage(int target, int attacker, int damage, int dflags, int mod)
{
	int      i;
	lua_vm_t *vm;

	for (i = 0; i < LUA_NUM_VM; i++)
	{
		vm = lVM[i];
		if (vm)
		{
			if (vm->id < 0 /*|| vm->err*/)
			{
				continue;
			}
			if (!G_LuaGetNamedFunction(vm, "et_Damage"))
			{
				continue;
			}
			// Arguments
			lua_pushinteger(vm->L, target);
			lua_pushinteger(vm->L, attacker);
			lua_pushinteger(vm->L, damage);
			lua_pushinteger(vm->L, dflags);
			lua_pushinteger(vm->L, mod);
			// Call
			if (!G_LuaCall(vm, "et_Damage", 5, 1))
			{
				//G_LuaStopVM(vm);
				continue;
			}
			// Return values
			if (lua_tointeger(vm->L, -1) == 1)
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}

#endif // FEATURE_LUA
