#ifndef FALLING_GAME_CORE
#define FALLING_GAME_CORE

#define TARGET_FPS 30
#define STARTING_BLOCKS 1
#define MAX_BLOCKS 20
#define PLAYER_WIDTH 20
#define PLAYER_HEIGHT 20
#define PLAYER_VELOCITY_MULT 2
#define BLOCK_WIDTH 15
#define BLOCK_HEIGHT 10
#define DEATH_SCREEN_DELAY 5.0e6


// Include the graphics library supplied by Martin Johnson for 159236
// CAUTION: No include guards provided, must only be included from CORE/when ifndef FALLING_GAME_CORE
#include "graphics.h"
#include "fonts.h"

// Velocity is measured per second. The `dt` provided to the game logic will be used
// to ensure smooth movement even if frames are skipped.
// This also means that even if the FPS above is adjusted, the game will play at the same speed.
#define STARTING_VELOCITY 25
#define MAX_VELOCITY 100

// The type of the game_update packet being dispatched. Tick means a redraw due to the game timer, input means an input from the user on GPIO(0/35)
typedef enum game_update_type {PACKET_TICK, PACKET_INPUT} game_update_type;

// A game update packet forcing the game to either process user input, or process the game logic and redraw the game
typedef struct game_update {
    game_update_type type;
    int data;
} game_update;

// The current phase of the game
typedef enum game_state_phase {PHASE_MENU, PHASE_DEATH, PHASE_GAME} game_state_phase;

// The current direction of movement for the player
typedef enum game_state_direction {DIR_LEFT, DIR_RIGHT, DIR_NONE} game_state_direction;

typedef struct game_block {
    int x;
    int y;
    int enabled;
    int waiting_for_respawn;
} game_block;

typedef struct player {
    int x;
    int y;
    int score;
} player;

// The game state created when the game starts
typedef struct game_state {
    // General information about the game
    game_state_phase phase;
    int velocity;

    // The movement of the player
    game_state_direction player_direction;

    // The current selection by the player on the main menu (0 = menu, 1 = instructions)
    int selection;
    
    // The blocks currently in the game
    game_block blocks[MAX_BLOCKS];
    player player;

    // Used to automatically return to menu after game over
    int64_t auto_advance_time;
} game_state;
#endif