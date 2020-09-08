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

// The game state created when the game starts
typedef struct game_state {
    game_state_phase phase;
    int score;
    int time_passed;
    game_state_direction player_direction;
    int selection;
} game_state;
