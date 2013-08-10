et.G_Printf("Goldrush Mapscript\n")
et.G_Printf("Initial amount entities: %i\n", et.G_CountEntities())

-- remove unused map entities
for i,classname in pairs({"trigger_objective_info", "func_constructible", "target_explosion", "target_smoke", "info_train_spline_control", "misc_cabinet_health", "trigger_ammo", "trigger_heal", "trigger_multiple", 
						  "misc_constructiblemarker", "info_limbo_camera", "misc_cabinet_supply", "target_speaker", "team_CTF_redflag", "team_CTF_blueflag", "info_null", "misc_mg42", "trigger_hurt", "target_script_trigger",
						  "trigger_flagonly_multiple", "misc_commandmap_marker", "info_player_deathmatch", "func_explosive", "func_timer"}) do
	for entnum,fields in pairs(et.G_SearchEntities({classname=classname})) do
		et.G_FreeEntity(entnum) 
	end
end

for i,modelname in pairs({"models/mapobjects/goldbox_sd/goldbox_trans_red.md3", "models/mapobjects/goldbox_sd/goldbox.md3", "models/mapobjects/blitz_sd/blitzwheelsb.md3", "models/mapobjects/blitz_sd/blitzwheelsf.md3",
						  "models/mapobjects/radios_sd/compostaxisopened.md3", "models/mapobjects/radios_sd/compostalliedopened.md3", "models/mapobjects/radios_sd/compostneutralclosed.md3", "models/mapobjects/cmarker/cmarker_flag.md3",
						  "models/mapobjects/cmarker/cmarker_crates.md3", "models/mapobjects/miltary_trim/barbwire.md3", "models/mapobjects/miltary_trim/dragon_teeth_wils.md3", "models/mapobjects/tanks_sd/churchhill_flash.md3",
						  "models/mapobjects/tanks_sd/jagdpanther_africa_turret.md3"}) do
	for entnum,fields in pairs(et.G_SearchEntities({model=modelname})) do
		et.G_FreeEntity(entnum) 
	end
end

for i,scriptName in pairs({"neutral_compost_clip_lms", "neutral_compost_closed_clip_lms", "south_ammocabinet_clip", "north_ammocabinet_clip", "neutral_compost_clip", "south_healthcabinet_clip", "neutral_compost_closed_clip", 
						   "north_healthcabinet_clip", "tank_sound", "boomtrigger"}) do
	for entnum,fields in pairs(et.G_SearchEntities({scriptName=scriptName})) do
		et.G_FreeEntity(entnum) 
	end
end

-- remove depot spawn
for entnum,fields in pairs(et.G_SearchEntities({scriptName="tankdepotblob"})) do
	et.G_FreeEntity(entnum) 
end

for entnum,fields in pairs(et.G_SearchEntities({targetname="tankdepot_allied_spawns"})) do
	et.G_FreeEntity(entnum) 
end

for entnum,fields in pairs(et.G_SearchEntities({targetname="tankdepot_axis_spawns"})) do
	et.G_FreeEntity(entnum) 
end

--remove truck & tank
for entnum,fields in pairs(et.G_SearchEntities({model="models/mapobjects/tanks_sd/jagdpanther_africa_shell.md3"})) do
	et.G_FreeEntity(entnum) 
end

for entnum,fields in pairs(et.G_SearchEntities({model2="models/mapobjects/tanks_sd/jagdpanther_africa_tracks.md3"})) do
	et.G_FreeEntity(entnum) 
end

for entnum,fields in pairs(et.G_SearchEntities({model2="models/mapobjects/blitz_sd/blitzbody.md3"})) do
	et.G_FreeEntity(entnum) 
end

--open bank
for entnum,fields in pairs(et.G_SearchEntities({scriptName="doorframe"})) do
	et.G_FreeEntity(entnum) 
end

for entnum,fields in pairs(et.G_SearchEntities({scriptName="bank_door_damaged"})) do
	et.gentity_set(entnum, "entstate", 0) -- 0 = default
end

for entnum,fields in pairs(et.G_SearchEntities({scriptName="bank_door2_damaged"})) do
	et.gentity_set(entnum, "entstate", 0) -- 0 = default
end

for entnum,fields in pairs(et.G_SearchEntities({target="bankdoor_debris1"})) do
	et.G_FreeEntity(entnum) 
end

for entnum,fields in pairs(et.G_SearchEntities({targetname="bankdoor_debris1"})) do
	et.G_FreeEntity(entnum) 
end

for entnum,fields in pairs(et.G_SearchEntities({target="bankdoor_debris2"})) do
	et.G_FreeEntity(entnum) 
end

for entnum,fields in pairs(et.G_SearchEntities({targetname="bankdoor_debris2"})) do
	et.G_FreeEntity(entnum) 
end

-- flags
et.G_SpawnGEntityFromSV({
	classname="team_WOLF_checkpoint",
	origin="-438 192 312",
	scriptname="depot_flag"
})

et.G_SpawnGEntityFromSV({
	classname="team_WOLF_checkpoint",
	origin="2941 1118 -417",
	scriptname="depot_flag"
})


et.G_SpawnGEntityFromSV({
	classname="team_WOLF_checkpoint",
	origin="-249 -1245 -210",
	scriptname="depot_flag"
})

et.G_Printf("Final amount entities: %i\n", et.G_CountEntities())

et.G_DumpEntities()
