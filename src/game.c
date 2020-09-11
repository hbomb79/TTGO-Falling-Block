#include "game.h"
#include <esp_timer.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

/* Forward declaration of static methods */

/*
 * Ticks the game by moving the blocks, calculating collisions, moving the player, etc
 */
static void tick(double dt, game_state* state);

/*
 * Checks for collisions between the blocks and players. Advances the player score when
 * blocks have completely left the screen.
 */
static void checkCollisions(game_state* state);

/*
 * Renders the game world
 */
static void render(game_state* state);

/*
 * Renders the main menu, or the start game instructions depending
 * on the selection provided (derived from the game state phase).
 */
static void renderMainMenuScreen(game_state* state);

/*
 * Renders the game (players score, blocks, player itself, etc)
 */
static void renderGameScreen(game_state* state);

/*
 * Renders the death/game over screen which displays the users score
 * briefly before automatically returning to menu (the user can also
 * click the buttons to immediately return).
 */
static void renderDeathScreen(game_state* state);

/*
 * When a block falls off the screen, we move it back up to the top to
 * provide the illusion of a new block.
 * 
 * In reality, removing and recreating a block is a waste of time and memory
 * if we can just change the Y value instead.
 */
static void respawnBlock(game_block* block);

/*
 * Spawns all our blocks (MAX_BLOCKS)
 */
static void initialiseBlocks(game_state* state);

/*
 * Enables the blocks that are currently spawned and waiting to be deployed
 * from the top of the screen.
 * 
 * Only blocks in the game state block array (state->blocks) up to the index
 * provided are enabled; this integer is derived from the current players score
 * and is increased as the difficulty is increased in response the players increasing
 * score.
 */
static void enableBlocks(game_state* state, int toBlockIndex);

/*
 * Checks if the player provided is colliding with the block provided.
 * 
 * Returns 1 if they are colliding, 0 otherwise.
 */
static int isPlayerColliding(player p, game_block b);

/* Method definitions */
void handleTickPacket(game_update packet, game_state* state) {
    // Move blocks, create new ones, advance velocity, move player, et
    // Change the delta time from the tick packet to ms, as microseconds is a bit overkill
    tick(packet.data / 1.0e3, state);

    // Render the game world
    render(state);
}

void handleInputPacket(game_update packet, game_state* state) {
    int input = packet.data;
    if(state->phase == PHASE_GAME) {
        state->player_direction = input;
    } else if(input != DIR_NONE) {
        switch(state->phase) {
            case PHASE_MENU:
                // If we're on the main menu, allow one click to show instructions, second click starts the game
                state->selection++;
                if(state->selection > 1) {
                    state->selection = 0;
                    initialiseGame(state);
                    state->phase = PHASE_GAME;
                }

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
}

void initialiseGame(game_state* state) {
    static int generatorExists = 0;
    if(!generatorExists) {
        srand((unsigned int)time(NULL));
        generatorExists = 1;
    }

    state->player_direction = DIR_NONE;
    state->time_passed = 0;
    state->velocity = STARTING_VELOCITY;

    player* p = &state->player;
    p->x = (display_width / 2) + PLAYER_WIDTH / 2;
    p->y = display_height - PLAYER_HEIGHT - 2;
    p->score = 0;

    initialiseBlocks(state);
    enableBlocks(state, STARTING_BLOCKS);
}

static int calculateVelocity(int vel, double dt) {
    // Velocity is calculated based on movement per SECOND.
    // However, if each frame is of varying duration/delay, we
    // should take that in to account when moving entities.
    // Here, the velocity value is returned as a fraction of the dt; if dt is one second,
    // then the velocity is returned as is.
    return ceil(vel * (dt/1000));
}

static void tick(double dt, game_state* state) {
    // If we're on the death screen, check to see if the death screen has been showing
    // for the allocated time already. If so, return to main menu.
    printf("TICK with dt: %fms and state phase: %d\n", dt, state->phase);
    if(state->phase == PHASE_DEATH && state->auto_advance_time > esp_timer_get_time()) {
        state->selection = 0;
        state->phase = PHASE_MENU;
    }
    
    if(state->phase == PHASE_GAME) {
        // Move the player
        player* p = &state->player;
        if(state->player_direction == DIR_LEFT) {
            p->x -= calculateVelocity(state->velocity * PLAYER_VELOCITY_MULT, dt);
        } else if(state->player_direction == DIR_RIGHT) {
            p->x += calculateVelocity(state->velocity * PLAYER_VELOCITY_MULT, dt);
        }

        // Keep player inside game
        if(p->x < 0) {
            p->x = 0;
        } else if(p->x + PLAYER_WIDTH > display_width) {
            p->x = display_width - PLAYER_WIDTH;
        }

        // Move/respawn blocks
        game_block* block;
        for(int i = 0; i < MAX_BLOCKS; i++) {
            block = &state->blocks[i];
            if(block->enabled) {
                if(block->waiting_for_respawn) {
                    respawnBlock(block);
                }

                block->y += calculateVelocity(state->velocity, dt);
            };
        };

        if(p->score > 200) {
            enableBlocks(state, ((p->score - 200) / 100) + STARTING_BLOCKS);
            state->velocity = STARTING_VELOCITY + (p->score-200)/400;
        }

        checkCollisions(state);
    }
};

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


static void renderMainMenuScreen(game_state* state) {
    cls(rgbToColour(0,255,0));
    setFont(FONT_DEJAVU18);
    setFontColour(255, 255, 255);
    if(state->selection == 0) {
        print_xy("Falling Block!", display_width / 7, 10);
    } else if(state->selection == 1) {
        print_xy("instructions", 1, 1);
    }
};

static void drawBlock(game_block b, uint16_t colour) {
    int x = b.x < 0 ? 0 : b.x;
    int y = b.y < 0 ? 0 : b.y;
    int width = b.x < 0 ? BLOCK_WIDTH + b.x : BLOCK_WIDTH;
    int height = b.y < 0 ? BLOCK_HEIGHT + b.y : BLOCK_HEIGHT;

    if(width <= 0 || height <= 0) {return;}

    draw_rectangle(x, y, width, height, colour);
}

static void renderGameScreen(game_state* state) {
    cls(rgbToColour(0,0,0));
    game_block b;
    for(int i = 0; i < MAX_BLOCKS; i++) {
        b = state->blocks[i];
        if(b.enabled == 1 && b.waiting_for_respawn == 0) {
            drawBlock(b, rgbToColour(255, 0, 0));
        }
    }

    player p = state->player;
    draw_rectangle(p.x, p.y, PLAYER_WIDTH, PLAYER_HEIGHT, rgbToColour(0, 0, 255));
};

static void renderDeathScreen(game_state* state) {
    cls(rgbToColour(255,0,0));
};

static void checkCollisions(game_state* state) {
    player* p = &state->player;
    for(int i = 0; i < MAX_BLOCKS; i++) {
        game_block* block = &state->blocks[i];
        if(block->enabled && block->waiting_for_respawn == 0) {
            if(isPlayerColliding(*p, *block) == 1) {
                state->phase = PHASE_DEATH;
            } else if(block->y > display_height) {
                block->waiting_for_respawn = 1;
                p->score += 100;
            }
        }
    }
};

static void enableBlocks(game_state* state, int toBlockIndex) {
    printf("Enabling blocks 0-%d\n", toBlockIndex);
    for(int i = 0; i < MAX_BLOCKS; i++) {
        int enabled = i < toBlockIndex;
        game_block* b = &state->blocks[i];

        if(b->enabled == 0) {
            b->enabled = enabled;
            b->waiting_for_respawn = enabled;
        }
    }
}

static void initialiseBlocks(game_state* state) {
    for(int i = 0; i < MAX_BLOCKS; i++) {
        (&state->blocks[i])->enabled = 0;
        (&state->blocks[i])->waiting_for_respawn= 0;
    };
}

static void respawnBlock(game_block* block) {
    printf("Attempting to respawn block!\n");
    if(block->enabled == 0) return;

    // The last block we spawned. Used as a quick and dirty test to determine
    // if we need to poll for blocks close to the top of the screen or not
    static game_block* last_spawned;

    // Find a space for this block to spawn
    if(last_spawned == NULL || last_spawned->y > BLOCK_HEIGHT*1.5) {
        // We can just spawn anywhere
        block->y = BLOCK_HEIGHT * -1;
        block->x = rand() % (display_width - BLOCK_WIDTH);
        block->waiting_for_respawn = 0;

        last_spawned = block;
    } else {
        // // There is a spawned block and it's close to the top of the screen.
        // // Either try and spawn a block adjacent to it, otherwise abort and
        // // we can retry on the next game tick

        // // Generate a random number; if even spawn on left of new block if possible
        // // If odd, spawn on right
        // int spawnSide = ( rand() % 2 );
        // if(spawnSide == 0 && last_spawned->x - BLOCK_WIDTH > 2) {
        //     block->x = 1;
        // } else if(spawnSide == 1 && last_spawned->x+(BLOCK_WIDTH*2) < display_width - 2) {
        //     block->x = last_spawned->x+(BLOCK_WIDTH*2) + 1;
        // } else {
        //     // Failed to spawn, no free space
        //     return;
        // };

        // block->y = BLOCK_HEIGHT * -1;
        // block->waiting_for_respawn = 0;
        // last_spawned = block;
    }
};

static int isPlayerColliding(player p, game_block b) {
    return !(p.x > b.x + BLOCK_WIDTH || p.x + PLAYER_WIDTH < b.x || p.y > b.y + BLOCK_HEIGHT || p.y + PLAYER_HEIGHT < b.y);
}

