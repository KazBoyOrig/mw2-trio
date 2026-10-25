// Own code, MIT. Private v11 genuine-rig integration candidate, runtime UNTESTED.
// Native source selection must fall back to the inert baseline unless both
// the guarded script-value channel and this load's native asset path exist.
// A direct native predicate supplies ready; an ordinary Dvar is not proof.
// Setup/status does not establish correct pose, rendering or locomotion.
#include maps\_utility;
#using_animtree( "multiplayer" );

players_ready( players )
{
	SetDvar( "mw2trio_script_players", players.size );
	if ( isdefined( level.mw2trio_stock_body_started ) )
		return;
	SetDvar( "mw2trio_script_probe", "v1 map=" + GetDvar( "mapname" ) + " players=" + players.size );
	if ( !scope_matches( players ) )
		return;
	// Synchronous native proof must precede the FIRST model registration.
	// Never wait for a later frame and then attempt an initial precache.
	if ( !assets_ready() )
	{
		report_models( players, "refused-assets" );
		return;
	}
	level.mw2trio_stock_body_started = true;
	PreCacheModel( "coop_body_us_army" );
	PreCacheModel( "coop_head_us_army" );
	thread install_after_loadout( players );
}

scope_matches( players )
{
	if ( GetDvar( "mapname" ) != "trainer" || GetDvar( "coop" ) != "1" || GetDvar( "specialops" ) != "0" || players.size != 2 )
		return false;
	if ( !isdefined( level.script ) || level.script != "trainer" || isdefined( level.so_trainer_start ) )
		return false;
	return true;
}

assets_ready()
{
	// The existing native formatter CALL wrappers intercept this exact name.
	// Both the first-initialization proof and later current-epoch continuity
	// proof return canonical 1 only. No GSC setter or cached result is used.
	return GetDvar( "mw2trio_body_assets_ready" ) == "1";
}

model_phase( players )
{
	if ( !isdefined( players[ 0 ] ) || !isdefined( players[ 1 ] ) || !isdefined( players[ 0 ].model ) || !isdefined( players[ 1 ].model ) )
		return "refused-model";
	if ( players[ 0 ].model == "coop_body_us_army" && players[ 1 ].model == "coop_body_us_army" )
		return "existing";
	if ( players[ 0 ].model != "" || players[ 1 ].model != "" )
		return "refused-model";
	return "assign";
}

install_after_loadout( players )
{
	level endon( "mw2coop_level_end" );
	waittillframeend;
	for ( attempt = 0; attempt < 100; attempt++ )
	{
		if ( isdefined( level.loadoutComplete ) && level.loadoutComplete )
			break;
		wait 0.05;
	}
	if ( !isdefined( level.loadoutComplete ) || !level.loadoutComplete )
	{
		observe_models( players, "timeout" );
		return;
	}
	if ( !scope_matches( players ) )
	{
		observe_models( players, "refused-scope" );
		return;
	}
	phase = model_phase( players );
	if ( phase != "assign" )
	{
		observe_models( players, phase );
		return;
	}
	// No wait lies between this fresh native proof and either player update.
	// The native module retains/rechecks this load's early-proven generation.
	if ( !assets_ready() )
	{
		observe_models( players, "refused-assets" );
		return;
	}
	players[ 0 ] maps\_utility::SetModelFunc( ::body_ranger );
	players[ 0 ] SetAnim( %code, 1, 0 );
	players[ 1 ] maps\_utility::SetModelFunc( ::body_ranger );
	players[ 1 ] SetAnim( %code, 1, 0 );
	observe_models( players, "assigned" );
}

body_ranger()
{
	// Stock UpdateModel owns weapon attachment and hide/show behavior.
	self SetModel( "coop_body_us_army" );
	self Attach( "coop_head_us_army", "", true );
}

observe_models( players, phase )
{
	for ( sample = 0; sample < 20; sample++ )
	{
		report_models( players, phase );
		wait 0.5;
	}
}

report_models( players, phase )
{
	loadout_ready = 0;
	if ( isdefined( level.loadoutComplete ) && level.loadoutComplete )
		loadout_ready = 1;
	SetDvar( "mw2trio_script_probe", "v11 map=trainer players=2 p0=" + model_name( players[ 0 ] ) + " p1=" + model_name( players[ 1 ] ) + " loadout=" + loadout_ready + " body=" + phase + " script=" + script_name() + " wrapper=" + wrapper_state() );
}

wrapper_state()
{
	if ( isdefined( level.so_trainer_start ) )
		return 1;
	return 0;
}

script_name()
{
	if ( !isdefined( level.script ) )
		return "<undef>";
	script = level.script;
	if ( script.size > 8 )
		return GetSubStr( script, 0, 8 );
	return script;
}

model_name( player )
{
	if ( !isdefined( player ) )
		return "<no-ent>";
	if ( !isdefined( player.model ) )
		return "<undefined>";
	model = player.model;
	if ( model == "" )
		return "<empty>";
	if ( model.size > 34 )
		return GetSubStr( model, 0, 34 );
	return model;
}
