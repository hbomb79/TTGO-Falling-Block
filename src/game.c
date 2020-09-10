#include "game.h"

/*
 * We've received a TICK game update packet, this means we're being requested
 * to movement the game elements, calculate collisions, and redraw the
 * game world
 */
void handleTickPacket(game_update packet, game_state* state) {
    // Move blocks, create new ones, advance velocity, move player, et
    // Change the delta time from the tick packet to ms, as microseconds is a bit overkill
    tick(packet.data / 1.0e3, state);

    // Render the game world
    render(state);
}

/*
 * We've received an INPUT game update packet, this means the user has provided
 * input to the game via the use of the buttons on the display board.
 * 
 * Depending on the current game states phase, this input will need to be handled
 * differently. For instance, in game it's used to change direction. On the menu,
 * it will be used to start the game, etc..
 */
void handleInputPacket(game_update packet, game_state* state) {
    // We received an INPUT packet, meaning we've recieved input from the user.
    // Depending on the phase of the game, this either means we're:
    // - Changing direction (when phase is GAME)
    // - Continuing from the death screen (when phase is DEATH) - this also will happen automatically after 2 seconds has elapsed
    // - Changing the state->selection (when phase is MENU), OR selecting the current selection

    int input = packet.data;
    switch(state->phase) {
        case PHASE_MENU:
            // If we're on the main menu, allow one click to show instructions, second click starts the game
            state->selection++;
            if(state->selection > 1) {
                state->selection = 0;
                state->phase = PHASE_GAME;
            }

            break;
        case PHASE_GAME:
            // In game; change player direction
            state->player_direction = input;
            break;
        case PHASE_DEATH:
            // On death screen; if user has pressed button then go to menu.
            state->phase = PHASE_MENU;
            break;
        default:
            printf("[WARNING] Unknown game state phase detected: %d\n", state->phase);
            break;
    }
}

/*
 * Ticks the game by moving the blocks, calculating collisions, moving the player, etc
 */
static void tick(double dt, game_state* state) {
    // If we're on the death screen, check to see if the death screen has been showing
    // for the allocated time already. If so, return to main menu.
    if(state->phase == PHASE_DEATH && state->auto_advance_time > esp_timer_get_time()) {
        state->selection = 0;
        state->phase = PHASE_MENU;
    }
};

/*
 * Renders the game world
 */
static void render(game_state* state) {
    switch(state->phase) {
        case PHASE_MENU:
            renderMainMenuScreen(state);
            break;
        case PHASE_DEATH:
            renderDeathScreen(state);
            break;
        case PHASE_GAME:
            renderGameScreen(state);
            break;
        default:
            printf("[WARNING] Unknown game state phase detected: %d\n", state->phase);
            break;
    }
};

/*
 * Renders the main menu, or the start game instructions depending
 * on the selection provided (derived from the game state phase).
 */
static void renderMainMenuScreen(game_state* state) {};

/*
 * Renders the game (players score, blocks, player itself, etc)
 */
static void renderGameScreen(game_state* state) {};

/*
 * Renders the death/game over screen which displays the users score
 * briefly before automatically returning to menu (the user can also
 * click the buttons to immediately return).
 */
static void renderDeathScreen(game_state* state) {};
