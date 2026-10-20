// Own code, MIT. Loader/compiler probe only; no networking or gameplay changes.
players_ready( players )
{
	SetDvar( "mw2trio_script_players", players.size );
	SetDvar( "mw2trio_script_probe", "v1 map=" + GetDvar( "mapname" ) + " players=" + players.size );
}
