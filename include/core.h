#ifndef FALLING_GAME_CORE
#define FALLING_GAME_CORE

// The FPS (frames per second) the game will try to run at
#define TARGET_FPS 30

// The amount of blocks first spawned when the user starts the game.
#define STARTING_BLOCKS 3

// The maximum amount of blocks that can ever exist at once
#define MAX_BLOCKS 15

// The width of the players block
#define PLAYER_WIDTH 20

// The height of the players block
#define PLAYER_HEIGHT 20

// A multiplier to the current game velocity which allows the player to move
// at a different speed to the blocks
#define PLAYER_VELOCITY_MULT 2

// The width of each falling block
#define BLOCK_WIDTH 15

// The head of each falling block
#define BLOCK_HEIGHT 10

// The amount of time (us) before the game over screen returns to the main menu
#define DEATH_SCREEN_DELAY 5.0e6

// Velocity is measured per second. The `dt` provided to the game logic will be used
// to ensure smooth movement even if frames are skipped.
// This also means that even if the FPS above is adjusted, the game will play at the same speed.
#define STARTING_VELOCITY 25
#define MAX_VELOCITY 100

// Include the graphics library supplied by Martin Johnson for 159236
// CAUTION: No include guards provided, must only be included from CORE/when ifndef FALLING_GAME_CORE
#include "graphics.h"
#include "fonts.h"

// The type of the game_update packet being dispatched. Tick means a redraw due to the game timer, input means an input from the user on GPIO(0/35)
typedef enum GamePacketType {PACKET_TICK, PACKET_INPUT} GamePacketType;

// A game update packet forcing the game to either process user input, or process the game logic and redraw the game
typedef struct GamePacket {
    GamePacketType type;
    int data;
} GamePacket;

// The current phase of the game
typedef enum GameStatePhase {PHASE_MENU, PHASE_DEATH, PHASE_GAME} GameStatePhase;

// The current direction of movement for the player
typedef enum GameStateDirection {DIR_LEFT, DIR_RIGHT, DIR_NONE} GameStateDirection;

typedef struct GameBlock {
    int x;
    int y;
    int enabled;
    int waiting_for_respawn;
} GameBlock;

typedef struct Player {
    int x;
    int y;
    int score;
} Player;

// The game state created when the game starts
typedef struct GameState {
    // General information about the game
    GameStatePhase phase;
    int velocity;

    // The movement of the player
    GameStateDirection player_direction;

    // The current selection by the player on the main menu (0 = menu, 1 = instructions)
    int selection;
    
    // The blocks currently in the game
    GameBlock blocks[MAX_BLOCKS];
    Player player;

    // Used to automatically return to menu after game over
    int64_t auto_advance_time;
} GameState;
#endif