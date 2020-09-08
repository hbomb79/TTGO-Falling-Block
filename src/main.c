/*
 * Simple falling block game developed in C, for use with the espidf32 framework for the TTGO T-Display board.
 * 
 * All code is the work of Harry Felton (Massey Student ID: 18032692), unless otherwise stated.
 * 
 */
#include <stdio.h>
#include <driver/gpio.h>
#include <driver/timer.h>
#include <esp_types.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#define TARGET_FPS 30

// The tick_queue stores any updates we're dispatching to the game loop
// from outside the loop (a timer callback or GPIO interrupt)
QueueHandle_t tick_queue;
esp_timer_handle_t game_timer;

typedef enum {TICK, INPUT} game_update_type;
typedef struct game_update {
    game_update_type type;
    int data;
} game_update;

typedef struct game_state {
    int score;
    int time_passed;
} game_state;

/*
 * The entry point in to this game; Here we need to configure our GPIO pins, setup
 * our game timers and configure interrupts. This is the like the backbone of the
 * program.
 */
void app_main() {
    // Create our tick queue; used to dispatch events to the logic of the game without
    // doing all logic inside of high-priority callback functions from the esp_timer.
    // The data passed will be our dT (delta time), which is the amount of time that has
    // passed since the last update tick was dispatched.
    tick_queue = xQueueCreate(10, sizeof(game_update));

    // Configure the direction and interrupts of our GPIO pins
    configure_gpio();

    // Create and start our game timer
    configure_hw_timer();

    start_game();
}

/*
 * This is the main point of the game-loop. It continues to run inifitely, polling for interrupts and
 * timer callbacks present in the tick_queue.
 * 
 * Each time an event is found inside the tick_queue, it's dispatched to the `game_tick` function, which
 * will handle the update, collisions, scoring and redrawing of the game.
 */
static void start_game() {
    game_state state = {
        .score = 0,
        .time_passed = 0
    };

    while(1) {
        
    }
}

static void update(double dt, game_state state) {};
static void checkCollisions(game_state state) {};
static void checkState(game_state state) {};
static void render(game_state state) {};

/*
 * Called during initialisation to configure GPIO (general purpose input/output) pins to the correct
 * direction (IN/OUT), and attach interrupt callbacks to their action.
 * 
 * TODO: Everything.
 */
static void configure_gpio() {}

/*
 * Runs every tick of the game. Use this oppourtunity to calculate the delta time, and then
 * queue this information in the `tick_queue` for handling by our game.
 * 
 * Generates an update packet with type TICK, and the data being the delta time since last update
 */
static void game_tick_timer_callback() {
    // Before we dispatch our event, we must first calculate the time that has passed.
    // `last_time` is a static here so it survives repeat invocations. We'll use this to store the current
    // time once we're done. This allows us to use it to compare against the current time now.
    static uint16_t last_time = 0;

    // Calculate the time that has passed since the last time we were here
    uint16_t dt = esp_timer_get_time() - last_time;

    // Update the last_time static to store the new, current time
    last_time = esp_timer_get_time();

    // Create our game_update packet
    const game_update update = {
        .type = TICK,
        .data = dt
    };

    // Dispatch our update packet to the game loop
    xQueueSend(tick_queue, &update, 0);
}

/*
 * Configures and starts the ESP high-resolution timer, targetting the TARGET_FPS defined above.
 * 
 * Runs the `game_tick_timer_callback` method every tick
 */
static void configure_hw_timer() {
    // To reduce complexity when handling light-sleep, dynamic frequency, etc.. we're using the high-resoultion
    // timers provided by ESP-IDF, rather than the low-priority timers provided by FreeRTOS.

    // Let's create our timer now
    const esp_timer_create_args_t game_tick_args = {
        .callback = &game_tick_timer_callback,
        .name = "Gametimer"
    };
    esp_timer_create(&game_tick_args, &game_timer);

    // Start the timer to run at 30 ticks per second (second / target runs per second) converted to microseconds
    esp_timer_start_periodic(game_timer, (1/TARGET_FPS)*1.0e6);
}
