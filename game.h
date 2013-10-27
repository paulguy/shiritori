#include "net.h"

typedef struct {
	char *name;
	Connection *c;

	int maxname;
} Player;

typedef struct {
	int maxplayers;
	Player **player;

	int maxname;
} Game;

Player *player_init(int maxname);

void player_free(Player *p);

void player_disconnect(Player *p);

Game *game_init(int maxplayers, int maxname);

void game_free(Game *g);
