function et.G_Printf(...)
       et.G_Print(string.format(unpack(arg)))
end

function et_InitGame( levelTime, randomSeed, restart )
       et.G_Printf( "et_InitGame [%d] [%d] [%d]\n", levelTime, randomSeed, restart )
       et.RegisterModname( "ETGoldy Base " .. et.FindSelf() )
end