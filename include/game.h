#ifndef FALLING_GAME_LOGIC
#define FALLING_GAME_LOGIC

// Pull in required structs, enums and constants
#include "core.h"

/*
 * Dispatches a TICK game update packet, this means we're being requested
 * to movement the game elements, calculate collisions, and redraw the
 * game world
 */
void handleTickPacket(game_update packet, game_state* state);

/*
 * Dispatch an INPUT game update packet, this means the user has provided
 * input to the game via the use of the buttons on the display board.
 * 
 * Depending on the current game states phase, this input will need to be handled
 * differently. For instance, in game it's used to change direction. On the menu,
 * it will be used to start the game, etc..
 */
void handleInputPacket(game_update packet, game_state* state);

void initialiseGame(game_state* state);

#endif