#ifndef FALLING_GAME_CORE
#define FALLING_GAME_CORE

#define STARTING_BLOCKS 1
#define MAX_BLOCKS 20

// Velocity is measured per second. The `dt` provided to the game logic will be used
// to ensure smooth movement even if frames are skipped.
// This also means that even if the FPS above is adjusted, the game will play at the same speed.
#define STARTING_VELOCITY 2
#define MAX_VELOCITY 10

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
    int width;
    int height;
} game_block;

// The game state created when the game starts
typedef struct game_state {
    // General information about the game
    game_state_phase phase;
    int score;
    int velocity;
    int block_count;
    int time_passed;

    // The movement of the player
    game_state_direction player_direction;

    // The current selection by the player on the main menu (0 = menu, 1 = instructions)
    int selection;
    
    // The blocks currently in the game
    game_block blocks[MAX_BLOCKS];
    game_block player;

    // Used to automatically return to menu after game over
    int64_t auto_advance_time;
} game_state;
#endif