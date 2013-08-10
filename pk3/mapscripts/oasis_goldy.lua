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

for i,modelname in pairs({"models/mapobjects/radios_sd/compostaxisopened.md3", "models/mapobjects/radios_sd/compostalliedopened.md3", "models/mapobjects/radios_sd/compostneutralclosed.md3", "models/mapobjects/cmarker/cmarker_flag.md3",
						  "models/mapobjects/cmarker/cmarker_crates.md3"}) do
	for entnum,fields in pairs(et.G_SearchEntities({model=modelname})) do
		et.G_FreeEntity(entnum) 
	end
end

for i,scriptName in pairs({}) do
	for entnum,fields in pairs(et.G_SearchEntities({scriptName=scriptName})) do
		et.G_FreeEntity(entnum) 
	end
end

et.G_Printf("Final amount entities: %i\n", et.G_CountEntities())

et.G_DumpEntities()