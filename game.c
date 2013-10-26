#include "game.h"

Player *player_init(int maxname) {
	Player *p;

	p = malloc(sizeof(Player));
	if(p == NULL)
		return(NULL);

	p->name = malloc(maxname + 1);
	if(p->name == NULL) {
		free(p);
		return(NULL);
	}
	p->connection = NULL;
	p->maxname = maxname;

	return(p);
}

void player_free(Player *p) {
	free(p->name);
	free(p);
}

void player_disconnect(Player *p) {
	if(p->connection == NULL)
		return;

	connection_disconnect(p->c);
	p->c = NULL;
}

Game *game_init(int maxplayers, int maxname) {
	Game *g;
	int i;

	g = malloc(sizeof(Game));
	if(g == NULL) {
		goto gerror0;

	g->player = malloc(sizeof(Player *) * maxplayers);
	if(g->player == NULL)
		goto gerror1;

	for(i = 0; i < maxplayers; i++) {
		g->player[i] = player_init(maxname);
		if(g->player[i] == NULL)
			break;
	}
	if(i < maxplayers) {
		for(; i >= 0; i++)
			player_free(g->player[i]);
		goto gerror2;
	}

	g->maxplayers = maxplayers;
	g->maxname = maxname;

	return(g);

gerror2:
	free(g->player);
gerror1:
	free(g);
gerror0:
	return(NULL);
}

void game_free(Game *g) {
	int i;

	for(i = 0; i < g->maxplayers; i++)
		player_free(g->player[i]);
	free(g->player);
	free(g);
}
