/*
 * Simple falling block game developed in C, for use with the espidf32 framework for the TTGO T-Display board.
 * 
 * All code is the work of Harry Felton (Massey Student ID: 18032692), unless otherwise stated.
 * 
 */
#include <driver/adc.h>
#include <driver/gpio.h>
#include <esp_adc_cal.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <esp_log.h>
#include <esp_sntp.h>

#include "main.h"
#include "fonts.h"
#include "graphics.h"

#define TARGET_FPS 30
#define MAX_BLOCKS 15

// Velocity is measured per second. The `dt` provided to the game logic will be used
// to ensure smooth movement even if frames are skipped.
// This also means that even if the FPS above is adjusted, the game will play at the same speed.
#define STARTING_VELOCITY 2
#define MAX_VELOCITY 10

// The packet_queue stores any updates we're dispatching to the game loop
// from outside the loop (a timer callback or GPIO interrupt)
QueueHandle_t packet_queue;

// The ESP timer we're using to drive the game loop
esp_timer_handle_t game_timer;


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
    packet_queue = xQueueCreate(10, sizeof(game_update));

    // Configure the direction and interrupts of our GPIO pins
    configure_gpio();

    // Create and start our game timer
    configure_hw_timer();

    graphics_init();
    start_game();
}

/*
 * This is the main point of the game-loop. It continues to run inifitely, polling for interrupts and
 * timer callbacks present in the packet_queue.
 * 
 * Each time an event is found inside the packet_queue, it's dispatched to the `game_tick` function, which
 * will handle the update, collisions, scoring and redrawing of the game.
 */
static void start_game() {
    // This state struct contains the current state of the game,
    // including the players score, movement and what state of the game
    // we're in (menu, game, game over, etc)
    game_state state = {
        .phase = PHASE_MENU,
        .score = 0,
        .time_passed = 0,
        .player_direction = DIR_NONE,
        .selection = 0
    };

    int frame = 0;
    game_update packet;
    int64_t start_time = esp_timer_get_time();
    // Start the game loop. This will run until the player quits the game.
    while(1) {
        // Keep iterating until we find a packet inside our queue.
        // This is used so that the high-priority callbacks/ISR functions are free as soon as is possible.
        if(xQueueReceive(packet_queue, &packet, 10) == pdTRUE) {
            printf("Frame: %d - ", frame);
            printf("Direction: %d - ", state.player_direction);
            printf("Packet type: %d - ", packet.type);
            printf("Packet data: %d \n\n", packet.data);
            if(packet.type == PACKET_TICK) {
                // We received a TICK packet, this means that the game should recalculate logic
                // and redraw. The ticks should happen according to the frames per second (FPS)
                // defined at the top of the file.
                printf("Advancing frame from %d\n", frame);
                frame++;
                update(packet.data, &state);

                cls(rgbToColour(100,20,20));
                setFont(FONT_DEJAVU18);
                setFontColour(255, 255, 255);

                char str[10];
                double fps = frame / (( esp_timer_get_time() - start_time ) / 1.0e6);
                sprintf(str, "%.1f", fps);
                print_xy(str, 300, 1);
                print_xy(str, 1, 1);
                // printf("FPS: %s\n", str);
                flip_frame();
            } else if(packet.type == PACKET_INPUT) {
                // We received an INPUT packet, meaning we've recieved input from the user.
                // Depending on the phase of the game, this either means we're:
                // - Changing direction (when phase is GAME)
                // - Continuing from the death screen (when phase is DEATH)
                // - Changing the state->selection (when phase is MENU), OR selecting the current selection
                state.player_direction = packet.data;
            }
        }

        // Prevent watchdog from terminating due to failure to yield
        vTaskDelay(1);
    }

    // vTaskDelete(NULL);
}

static void update(double dt, game_state* state) {
    // TODO
};

/*
 * Called during initialisation to configure GPIO (general purpose input/output) pins to the correct
 * direction (IN/OUT), and attach interrupt callbacks to their action.
 */
static void configure_gpio() {
    // First, configure the direction of the GPIO pins we're using (0 and 35)
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_NUM_35, GPIO_MODE_INPUT);

    // Set the interrupt type
    gpio_set_intr_type(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);
    gpio_set_intr_type(GPIO_NUM_35, GPIO_INTR_LOW_LEVEL);

    // Then, install our ISR service
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);

    // And attach the handler to the service
    gpio_isr_handler_add(GPIO_NUM_0, gpio_button_isr_handler, (void*)GPIO_NUM_0);
    gpio_isr_handler_add(GPIO_NUM_35, gpio_button_isr_handler, (void*)GPIO_NUM_35);
}

/*
 * The ISR handler for the GPIO pins allocated to the physical buttons on the board.
 * 
 * This handler is responsible for debouncing and dispatching the button presses
 * to the game loop via the `packet_queue`
 */
static void IRAM_ATTR gpio_button_isr_handler(void* gpio_arg) {
    // First, debounce the button press. Store the current time now for use
    static int64_t last_press_time = 0;
    int64_t current_time = esp_timer_get_time();
    int gpio_pin = (int)gpio_arg;

    // Used to detect when a button was pressed, and then released.
    // Starts at both buttons unpressed (0)
    static int button_status[2] = {1,1};

    // Find whether or not this button is currently pressed down
    // If pin is 35, comparison will return TRUE (1), otherwise FALSE (0). We can use
    // these integers to index our static array of button states
    bool isPressed = button_status[gpio_pin == 35];

    // Time since last change must be more than 500us to continue (debounce)
    if(current_time - last_press_time > 500) {
        game_update packet = {.type = PACKET_INPUT};
        if(isPressed) {
            // The button attached to this GPIO pin was just pressed down
            // Set the direction of movement to the direction this button correlates with
            packet.data = gpio_pin == 35 ? DIR_RIGHT : DIR_LEFT;
        } else {
            // The button attached to this GPIO pin was just released
            // Set direction of movement back to NONE
            packet.data = DIR_NONE;
        }

        xQueueSendFromISR(packet_queue, &packet, 0);
    }

    // Store the new updated state of the button
    button_status[gpio_pin == 35] = !isPressed;

    // Refresh the stored static time for debouncing
    last_press_time = current_time;

    // Stop the iterrupt being fired again just because we're holding the button
    // Do this by setting the interrupt type to LOW, if we're currently holding the
    // button, or HIGH otherwise.
    gpio_set_intr_type(gpio_pin, isPressed ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
}

/**
 * Runs every tick of the game. Use this oppourtunity to calculate the delta time, and then
 * queue this information in the `packet_queue` for handling by our game.
 * 
 * Generates an update packet with type TICK, and the data being the delta time since last update
 */
static void game_tick_timer_callback() {
    // Before we dispatch our event, we must first calculate the time that has passed.
    // `last_time` is a static here so it survives repeat invocations. We'll use this to store the current
    // time once we're done. This allows us to use it to compare against the current time now.
    static int64_t last_time = 0;

    // Calculate the time that has passed since the last time we were here
    int64_t dt = esp_timer_get_time() - last_time;

    // Update the last_time static to store the new, current time
    last_time = esp_timer_get_time();

    // Create our game_update packet
    const game_update update = {
        .type = PACKET_TICK,
        .data = dt
    };

    // Dispatch our update packet to the game loop
    xQueueSend(packet_queue, &update, 0);
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
    esp_timer_start_periodic(game_timer, 1.0e6/TARGET_FPS);
}
