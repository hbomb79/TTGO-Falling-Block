#ifndef MAIN_HEADER
#define MAIN_HEADER

#include "core.h"

static void start_game();
static void configure_gpio();
static void gpio_button_isr_handler(void* gpio_arg);
static void configure_hw_timer();
static void game_tick_timer_callback();

#endif