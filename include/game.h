#ifndef FALLING_GAME_LOGIC
#define FALLING_GAME_LOGIC

#include "core.h"

void handleTickPacket(game_update packet, game_state* state);
void handleInputPacket(game_update packet, game_state* state);

static void checkCollisions(game_state* state);
static void checkState(game_state* state);
static void render(game_state* state);
static void tick(double dt, game_state* state);

#endif