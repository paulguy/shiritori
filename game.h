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

Game *game_init(int maxplayers, int maxname);
