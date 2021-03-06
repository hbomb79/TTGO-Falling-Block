#include <esp_timer.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"

/* Forward declaration of static methods */

/*
 * Ticks the game by moving the blocks, calculating collisions, moving the player, etc
 */
static void tick(double dt, GameState* state);

/*
 * Checks for collisions between the blocks and players. Advances the player score when
 * blocks have completely left the screen.
 */
static void check_collisions(GameState* state);

/*
 * Renders the game world
 */
static void render(GameState* state);

/*
 * Renders the main menu, or the start game instructions depending
 * on the selection provided (derived from the game state phase).
 */
static void render_main_menu(GameState* state);

/*
 * Renders the game (players score, blocks, player itself, etc)
 */
static void render_game(GameState* state);

/*
 * Renders the death/game over screen which displays the users score
 * briefly before automatically returning to menu (the user can also
 * click the buttons to immediately return).
 */
static void render_gameover(GameState* state);

/*
 * When a block falls off the screen, we move it back up to the top to
 * provide the illusion of a new block.
 *
 * In reality, removing and recreating a block is a waste of time and memory
 * if we can just change the Y value instead.
 */
static void respawn_block(GameBlock* block);

/*
 * Spawns all our blocks (MAX_BLOCKS)
 */
static void initialise_blocks(GameState* state);

/*
 * Enables the blocks that are currently spawned and waiting to be deployed
 * from the top of the screen.
 *
 * Only blocks in the game state block array (state->blocks) up to the index
 * provided are enabled; this integer is derived from the current players score
 * and is increased as the difficulty is increased in response the players increasing
 * score.
 */
static void enable_blocks(GameState* state, int toBlockIndex);

/*
 * Draws the rectangle representing the block using the colour provided.
 *
 * If the X/Y position of the block is less than zero, the width/height
 * is reduced and the appropiatte X/Y value is set to zero to prevent
 * the underlying graphics method from crashing.
 */
static void draw_block(GameBlock b, uint16_t colour);

/*
 * Checks if the player provided is colliding with the block provided.
 *
 * Returns 1 if they are colliding, 0 otherwise.
 */
static int check_player_collision(Player p, GameBlock b);

/*
 * Uses the dt provided (in ms) to calculate the velocity.
 */
static int calc_velocity(int vel, double dt);

/*
 * Initialises the game by resetting the game state, player position,
 * velocity, etc.
 */
static void initialise_game(GameState* state);

/* Method definitions */

void handleTickPacket(GamePacket packet, GameState* state) {
    // Move blocks, create new ones, advance velocity, move player, et
    // Change the delta time from the tick packet to ms, as microseconds is a bit overkill
    tick(packet.data / 1.0e3, state);

    // Render the game world
    render(state);
}

void handleInputPacket(GamePacket packet, GameState* state) {
    // Ignore button presses in first second of runtime as the TTGO
    // board seems to emit two button presses on GPIO pin 0 without
    // any user action.
    if(esp_timer_get_time() < 1e6) return;

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
                    state->phase = PHASE_GAME;

                    initialise_game(state);
                }

                break;
            case PHASE_DEATH:
                // On death screen; if user has pressed button then go to menu.
                // Only respond after 500ms incase user hit button trying to avoid
                // block moments before death.
                if(esp_timer_get_time() >= state->auto_advance_time - 4.5e6) {
                    state->phase = PHASE_MENU;
                }

                break;
            default:
                printf("[WARNING] Unknown game state phase detected: %d\n", state->phase);
                break;
        }
    }
}

static void initialise_game(GameState* state) {
    // Create the RNG if not already created
    static int generatorExists = 0;
    if(!generatorExists) {
        srand((unsigned int)time(NULL));
        generatorExists = 1;
    }

    // Reset the state
    state->player_direction = DIR_NONE;
    state->velocity = STARTING_VELOCITY;
    state->selection = 0;

    // Reset the player
    Player* p = &state->player;
    p->x = (display_width / 2) - PLAYER_WIDTH / 2;
    p->y = display_height - PLAYER_HEIGHT - 5;
    p->score = 0;

    // Reset all blocks and re-enable only the required ones
    initialise_blocks(state);
    enable_blocks(state, STARTING_BLOCKS);
}

static int calc_velocity(int vel, double dt) {
    return ceil(vel * (dt/1000));
}

static void tick(double dt, GameState* state) {
    // If we're on the death screen, check to see if the death screen has been showing
    // for the allocated time already. If so, return to main menu.
    if(state->phase == PHASE_DEATH && state->auto_advance_time <= esp_timer_get_time()) {
        state->selection = 0;
        state->phase = PHASE_MENU;
    }
    
    if(state->phase == PHASE_GAME) {
        // Move the player
        Player* p = &state->player;
        if(state->player_direction == DIR_LEFT) {
            p->x -= calc_velocity(state->velocity * PLAYER_VELOCITY_MULT, dt);
        } else if(state->player_direction == DIR_RIGHT) {
            p->x += calc_velocity(state->velocity * PLAYER_VELOCITY_MULT, dt);
        }

        // Keep player inside game
        if(p->x < 0) {
            p->x = 0;
        } else if(p->x + PLAYER_WIDTH > display_width) {
            p->x = display_width - PLAYER_WIDTH;
        }

        // Move/respawn blocks
        GameBlock* block;
        for(int i = 0; i < MAX_BLOCKS; i++) {
            block = &state->blocks[i];
            if(block->enabled) {
                if(block->waiting_for_respawn) { respawn_block(block); }

                block->y += calc_velocity(state->velocity, dt);
            };
        };

        // Increase speed and amount of blocks as the users score rises
        if(p->score > 200) {
            int score_difference = p->score - 200;
            enable_blocks(state, (score_difference/300) + STARTING_BLOCKS);
            state->velocity = STARTING_VELOCITY + (score_difference/400)*0.5;
        }

        check_collisions(state);
    }
};

static void render(GameState* state) {
    switch(state->phase) {
        case PHASE_MENU:
            render_main_menu(state);
            break;
        case PHASE_DEATH:
            render_gameover(state);
            break;
        case PHASE_GAME:
            render_game(state);
            break;
        default:
            printf("[WARNING] Unknown game state phase detected: %d\n", state->phase);
            break;
    }
};

static void render_main_menu(GameState* state) {
    cls(rgbToColour(190,190,190));
    setFont(FONT_DEJAVU24);
    setFontColour(255, 255, 255);
    if(state->selection == 0) {
        print_xy("Fall", 20, 20);
        print_xy("i", 66, 24);
        print_xy("n", 74, 28);
        print_xy("g", 90, 32);
        print_xy("Blocks!", 22, 60);

        draw_rectangle(24, 120, 1, 10, rgbToColour(150,150,150));
        draw_rectangle(54, 105, 1, 12, rgbToColour(150,150,150));
        draw_rectangle(20, 135, 40, 30, rgbToColour(255, 0, 0));

        draw_rectangle(82, 155, 1, 15, rgbToColour(150,150,150));
        draw_rectangle(100, 145, 1, 8, rgbToColour(150,150,150));
        draw_rectangle(105, 155, 1, 12, rgbToColour(150,150,150));
        draw_rectangle(75, 185, 40, 30, rgbToColour(255, 0, 0));

        setFontColour(0,0,0);
        setFont(FONT_UBUNTU16);
        print_xy("Press to Start", 10, display_height - getFontHeight());
    } else if(state->selection == 1) {
        setFont(FONT_DEJAVU24);
        setFontColour(100, 100, 100);
        print_xy("Guide", 1, 1);

        setFontColour(255, 255, 255);
        setFont(FONT_DEJAVU18);
        print_xy("Dodge the", 1, 45);
        setFontColour(255, 0, 0);
        print_xy("falling blocks", 1, 65);
        setFontColour(255, 255, 255);
        print_xy("using the left", 1, 85);
        print_xy("and right", 1, 105);
        print_xy("buttons!", 1, 125);

        setFontColour(0,0,255);
        print_xy("Good Luck!", 10, 165);

        setFontColour(0,0,0);
        setFont(FONT_UBUNTU16);
        print_xy("Press to Start", 10, display_height - getFontHeight());
    }
};

static void render_game(GameState* state) {
    cls(rgbToColour(0,0,0));
    GameBlock b;
    for(int i = 0; i < MAX_BLOCKS; i++) {
        b = state->blocks[i];
        if(b.enabled == 1 && b.waiting_for_respawn == 0) {
            draw_block(b, rgbToColour(255, 0, 0));
        }
    }

    Player p = state->player;
    draw_rectangle(p.x, p.y, PLAYER_WIDTH, PLAYER_HEIGHT, rgbToColour(0, 0, 255));

    setFontColour(240,240,240);
    draw_rectangle(0, 0, display_width, getFontHeight() + 4, rgbToColour(10, 10, 10));

    char score[32];
    sprintf(score, "Score: %d", p.score);
    print_xy(score, 1, 2);
};

static void render_gameover(GameState* state) {
    cls(rgbToColour(190,190,190));
    setFontColour(255, 0, 0);
    setFont(FONT_DEJAVU18);
    print_xy("Game over", 1, 20);

    setFont(FONT_UBUNTU16);
    setFontColour(255, 255, 255);
    char score[32];
    sprintf(score, "Score: %04d", state->player.score);
    print_xy(score, 1, 45);

    int64_t current_time = esp_timer_get_time();
    int64_t target_time = state->auto_advance_time;
    double perc_time_remaining = 1 - (abs(target_time - current_time) / DEATH_SCREEN_DELAY);

    setFontColour(0,0,0);
    setFont(FONT_SMALL);

    int bar_height = getFontHeight() * 2;
    draw_rectangle(0, display_height - bar_height, display_width * perc_time_remaining, bar_height, rgbToColour(255, 255, 255));
    print_xy("Press to Continue", 10, display_height - getFontHeight()*1.5);
};

static void check_collisions(GameState* state) {
    Player* p = &state->player;
    for(int i = 0; i < MAX_BLOCKS; i++) {
        GameBlock* block = &state->blocks[i];
        if(block->enabled && block->waiting_for_respawn == 0) {
            if(check_player_collision(*p, *block) == 1) {
                state->phase = PHASE_DEATH;
                state->auto_advance_time = esp_timer_get_time() + DEATH_SCREEN_DELAY;
            } else if(block->y > display_height) {
                block->waiting_for_respawn = 1;
                p->score += 100;
            }
        }
    }
};

static int check_player_collision(Player p, GameBlock b) {
    return !(p.x > b.x + BLOCK_WIDTH || p.x + PLAYER_WIDTH < b.x || p.y > b.y + BLOCK_HEIGHT || p.y + PLAYER_HEIGHT < b.y);
}

static void enable_blocks(GameState* state, int toBlockIndex) {
    for(int i = 0; i < MAX_BLOCKS; i++) {
        int enabled = i < toBlockIndex;
        GameBlock* b = &state->blocks[i];

        if(b->enabled == 0) {
            b->enabled = enabled;
            b->waiting_for_respawn = enabled;
        }
    }
}

static void initialise_blocks(GameState* state) {
    for(int i = 0; i < MAX_BLOCKS; i++) {
        (&state->blocks[i])->enabled = 0;
        (&state->blocks[i])->waiting_for_respawn= 0;
    };
}

static void respawn_block(GameBlock* block) {
    if(block->enabled == 0) return;

    // The last block we spawned. Used as a quick and dirty test to determine
    // if we need to poll for blocks close to the top of the screen or not
    static GameBlock* last_spawned;

    if(last_spawned == NULL || last_spawned->enabled == 0 || last_spawned->waiting_for_respawn == 1 || last_spawned->y > BLOCK_HEIGHT*1.5) {
        block->y = BLOCK_HEIGHT * -1;
        block->x = rand() % (display_width - BLOCK_WIDTH);
        block->waiting_for_respawn = 0;

        last_spawned = block;
    }
};

static void draw_block(GameBlock b, uint16_t colour) {
    int x = b.x < 0 ? 0 : b.x;
    int y = b.y < 0 ? 0 : b.y;
    int width = b.x < 0 ? BLOCK_WIDTH + b.x : BLOCK_WIDTH;
    int height = b.y < 0 ? BLOCK_HEIGHT + b.y : BLOCK_HEIGHT;

    if(width <= 0 || height <= 0) {return;}

    draw_rectangle(x, y, width, height, colour);
}

