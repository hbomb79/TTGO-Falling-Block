#ifndef MAIN_HEADER
#define MAIN_HEADER

#include <game.h>

static void start_game();
static void update(double dt, game_state* state);
static void checkCollisions(game_state* state);
static void checkState(game_state* state);
static void render(game_state* state);
static void configure_gpio();
static void gpio_button_isr_handler(void* gpio_arg);

static void configure_hw_timer();
static void game_tick_timer_callback();

#endif