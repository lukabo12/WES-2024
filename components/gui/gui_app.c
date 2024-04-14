/**
 * @file gui_app.c
 *
 * @brief This file runs the UI.
 *
 * COPYRIGHT NOTICE: (c) 2023 Byte Lab Grupa d.o.o.
 * All rights reserved.
 */

//--------------------------------- INCLUDES ----------------------------------
#include "gui_app.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* Littlevgl specific */
#include "lvgl.h"
#include "lvgl_helpers.h"
//---------------------------------- MACROS -----------------------------------

//-------------------------------- DATA TYPES ---------------------------------

//---------------------- PRIVATE FUNCTION PROTOTYPES --------------------------
/**
 * @brief This function handles events that happen on LVGL buttons.
 *
 * @param [in] p_event Pointer to the event type.
 */
static void _button_event_handler(lv_event_t *p_event);

/**
 * @brief The function unblockingly sends an event to the user interface queue.
 *
 * @param event Gui event to be sent.
 */
static void _gui_app_event_send(gui_app_event_t event);

static void goto_new_screen(void);

static void lv_example_grid_1(void);

//------------------------- STATIC DATA & CONSTANTS ---------------------------
lv_obj_t *p_btn1;
lv_obj_t *p_btn_led_on;
lv_obj_t *p_btn_led_off;

// Button za novi screen
lv_obj_t *p_btn_naprijed;

// Novi screen
lv_obj_t *novi_screen;

// Pocetni screen
lv_obj_t *pocetni_screen;

// Button za povratak na stari screen
lv_obj_t *p_btn_povratak;

// Ovjde pišem X ili O
lv_obj_t *label;

// Buttoni na gridu odnosno arrayu
lv_obj_t *obj;

static lv_obj_t *array_buttona[9];

// Array labela koji koriste za upisivanje
static lv_obj_t *array_labela[9];

static const char *znak[2] = { "X", "O" };

bool is_Pressed = true;
//------------------------------- GLOBAL DATA ---------------------------------
extern QueueHandle_t p_user_interface_queue;

//------------------------------ PUBLIC FUNCTIONS -----------------------------
void gui_app_init(void)
{

    /* Create button 1 */
    lv_obj_t *p_label1;
    p_btn1 = lv_btn_create(lv_scr_act());
    lv_obj_align_to(p_btn1, NULL, LV_ALIGN_CENTER, 0, -80);
    p_label1 = lv_label_create(p_btn1);
    lv_label_set_text(p_label1, "Button 1");

    /* Add callback for button 1 */
    (void)lv_obj_add_event_cb(p_btn1, _button_event_handler, LV_EVENT_CLICKED, NULL);

    /* Create "led on" button */
    lv_obj_t *p_led_on_label;
    p_btn_led_on = lv_btn_create(lv_scr_act());
    lv_obj_align_to(p_btn_led_on, NULL, LV_ALIGN_LEFT_MID, 0, 0);
    p_led_on_label = lv_label_create(p_btn_led_on);
    lv_label_set_text(p_led_on_label, "LED on");

    /* Add callback for "led on" button */
    (void)lv_obj_add_event_cb(p_btn_led_on, _button_event_handler, LV_EVENT_CLICKED, NULL);

    /* Create "led off" button */
    lv_obj_t *p_led_off_label;
    p_btn_led_off = lv_btn_create(lv_scr_act());
    lv_obj_align_to(p_btn_led_off, NULL, LV_ALIGN_CENTER, 0, 0);
    p_led_off_label = lv_label_create(p_btn_led_off);
    lv_label_set_text(p_led_off_label, "LED off");

    /* Add callback for "led off" button */
    (void)lv_obj_add_event_cb(p_btn_led_off, _button_event_handler, LV_EVENT_CLICKED, NULL);

    // Inicijalizacija buttona koji vodi na novi screen
    lv_obj_t *p_btn_naprijed_label;
    p_btn_naprijed = lv_btn_create(lv_scr_act());
    lv_obj_align_to(p_btn_naprijed, NULL, LV_ALIGN_BOTTOM_MID, 0, -20);
    p_btn_naprijed_label = lv_label_create(p_btn_naprijed);
    lv_label_set_text(p_btn_naprijed_label, "Idemo dalje");

    (void)lv_obj_add_event_cb(p_btn_naprijed, _button_event_handler, LV_EVENT_CLICKED, NULL);
}

//---------------------------- PRIVATE FUNCTIONS ------------------------------
static void _button_event_handler(lv_event_t *p_event)
{
    if(p_btn1 == p_event->target)
    {
        if(LV_EVENT_CLICKED == p_event->code)
        {
            printf("Button 1 clicked\n");
        }
    }
    else if(p_btn_led_on == p_event->target)
    {
        if(LV_EVENT_CLICKED == p_event->code)
        {
            /* Inform the user interface about button click. */
            _gui_app_event_send(GUI_APP_EVENT_BUTTON_LED_ON_PRESSED);
        }
    }
    else if(p_btn_led_off == p_event->target)
    {
        if(LV_EVENT_CLICKED == p_event->code)
        {
            /* Inform the user interface about button click. */
            _gui_app_event_send(GUI_APP_EVENT_BUTTON_LED_OFF_PRESSED);
        }
    }
    else if(p_btn_naprijed == p_event->target) // Handle new button for screen transition
    {
        if(LV_EVENT_CLICKED == p_event->code)
        {
            // Load the yellow screen
            // printf("Nesto se kuha gospodine bijeli");
            goto_new_screen();
            lv_example_grid_1();
        }
    }
    else if(p_btn_povratak == p_event->target) // Handle new button for screen transition
    {
        if(LV_EVENT_CLICKED == p_event->code)
        {
            // Load the yellow screen
            // printf("Nesto se kuha gospodine bijeli");
            lv_scr_load(pocetni_screen);
        }
    }

    // TIC TAC TOE GAME -------------------------------------------------------------------------

    else if(array_buttona[0] == p_event->target) // Handle new button for screen transition
    {

        if(LV_EVENT_CLICKED == p_event->code)
        {

            // Load the yellow screen
            // printf("Nesto se kuha gospodine bijeli");
            if(is_Pressed == true)
            {
                lv_label_set_text_fmt(array_labela[0], znak[0]);
                lv_obj_center(array_labela[0]);
                is_Pressed = !is_Pressed;
            }
            else
            {
                lv_label_set_text_fmt(array_labela[0], znak[1]);
                lv_obj_center(array_labela[0]);
                is_Pressed = !is_Pressed;
            }
        }
    }

    else if(array_buttona[1] == p_event->target) // Handle new button for screen transition
    {

        if(LV_EVENT_CLICKED == p_event->code)
        {

            // Load the yellow screen
            // printf("Nesto se kuha gospodine bijeli");

            if(is_Pressed == true)
            {
                lv_label_set_text_fmt(array_labela[1], znak[0]);
                lv_obj_center(array_labela[1]);
                is_Pressed = !is_Pressed;
            }
            else
            {
                lv_label_set_text_fmt(array_labela[1], znak[1]);
                lv_obj_center(array_labela[1]);
                is_Pressed = !is_Pressed;
            }
        }
    }
    else if(array_buttona[2] == p_event->target) // Handle new button for screen transition
    {

        if(LV_EVENT_CLICKED == p_event->code)
        {

            // Load the yellow screen
            // printf("Nesto se kuha gospodine bijeli");

            if(is_Pressed == true)
            {
                lv_label_set_text_fmt(array_labela[2], znak[0]);
                lv_obj_center(array_labela[2]);
                is_Pressed = !is_Pressed;
            }
            else
            {
                lv_label_set_text_fmt(array_labela[2], znak[1]);
                lv_obj_center(array_labela[2]);
                is_Pressed = !is_Pressed;
            }
        }
    }

    else if(array_buttona[3] == p_event->target) // Handle new button for screen transition
    {

        if(LV_EVENT_CLICKED == p_event->code)
        {

            if(is_Pressed == true)
            {
                lv_label_set_text_fmt(array_labela[3], znak[0]);
                lv_obj_center(array_labela[3]);
                is_Pressed = !is_Pressed;
            }
            else
            {
                lv_label_set_text_fmt(array_labela[3], znak[1]);
                lv_obj_center(array_labela[3]);
                is_Pressed = !is_Pressed;
            }
        }
    }

    else if(array_buttona[4] == p_event->target) // Handle new button for screen transition
    {

        if(LV_EVENT_CLICKED == p_event->code)
        {

            if(is_Pressed == true)
            {
                lv_label_set_text_fmt(array_labela[4], znak[0]);
                lv_obj_center(array_labela[4]);
                is_Pressed = !is_Pressed;
            }
            else
            {
                lv_label_set_text_fmt(array_labela[4], znak[1]);
                lv_obj_center(array_labela[4]);
                is_Pressed = !is_Pressed;
            }
        }
    }

    else if(array_buttona[5] == p_event->target) // Handle new button for screen transition
    {

        if(LV_EVENT_CLICKED == p_event->code)
        {

            if(is_Pressed == true)
            {
                lv_label_set_text_fmt(array_labela[5], znak[0]);
                lv_obj_center(array_labela[5]);
                is_Pressed = !is_Pressed;
            }
            else
            {
                lv_label_set_text_fmt(array_labela[5], znak[1]);
                lv_obj_center(array_labela[5]);
                is_Pressed = !is_Pressed;
            }
        }
    }
    else if(array_buttona[6] == p_event->target) // Handle new button for screen transition
    {

        if(LV_EVENT_CLICKED == p_event->code)
        {

            if(is_Pressed == true)
            {
                lv_label_set_text_fmt(array_labela[6], znak[0]);
                lv_obj_center(array_labela[6]);
                is_Pressed = !is_Pressed;
            }
            else
            {
                lv_label_set_text_fmt(array_labela[6], znak[1]);
                lv_obj_center(array_labela[6]);
                is_Pressed = !is_Pressed;
            }
        }
    }

    else if(array_buttona[7] == p_event->target) // Handle new button for screen transition
    {

        if(LV_EVENT_CLICKED == p_event->code)
        {

            if(is_Pressed == true)
            {
                lv_label_set_text_fmt(array_labela[7], znak[0]);
                lv_obj_center(array_labela[7]);
                is_Pressed = !is_Pressed;
            }
            else
            {
                lv_label_set_text_fmt(array_labela[7], znak[1]);
                lv_obj_center(array_labela[7]);
                is_Pressed = !is_Pressed;
            }
        }
    }

    else if(array_buttona[8] == p_event->target) // Handle new button for screen transition
    {

        if(LV_EVENT_CLICKED == p_event->code)
        {

            if(is_Pressed == true)
            {
                lv_label_set_text_fmt(array_labela[8], znak[0]);
                lv_obj_center(array_labela[8]);
                is_Pressed = !is_Pressed;
            }
            else
            {
                lv_label_set_text_fmt(array_labela[8], znak[1]);
                lv_obj_center(array_labela[8]);
                is_Pressed = !is_Pressed;
            }
        }
    }

    else if(array_buttona[9] == p_event->target) // Handle new button for screen transition
    {

        if(LV_EVENT_CLICKED == p_event->code)
        {

            if(is_Pressed == true)
            {
                lv_label_set_text_fmt(array_labela[9], znak[0]);
                lv_obj_center(array_labela[9]);
                is_Pressed = !is_Pressed;
            }
            else
            {
                lv_label_set_text_fmt(array_labela[9], znak[1]);
                lv_obj_center(array_labela[9]);
                is_Pressed = !is_Pressed;
            }
        }
    }
    //-----------------------------------------------------------------------------------------------------------
    else
    {
        /* Unknown button event. */
    }
}
static void _gui_app_event_send(gui_app_event_t event)
{
    if(p_user_interface_queue != NULL)
    {
        xQueueSend(p_user_interface_queue, &event, 0U);
    }
}

static void goto_new_screen(void)
{
    // Create a new screen
    novi_screen = lv_obj_create(NULL);
    // Inicijalizacija buttona koji vodi nazad na stari screen
    lv_obj_t *p_btn_povratak_label;
    p_btn_povratak = lv_btn_create(novi_screen);
    lv_obj_align_to(p_btn_povratak, NULL, LV_ALIGN_BOTTOM_RIGHT, 50, 0);
    p_btn_povratak_label = lv_label_create(p_btn_povratak);
    lv_label_set_text(p_btn_povratak_label, "Nazad");
    (void)lv_obj_add_event_cb(p_btn_povratak, _button_event_handler, LV_EVENT_CLICKED, NULL);
    lv_scr_load(novi_screen);
}

static void lv_example_grid_1(void)
{
    static int16_t col_dsc[] = { 40, 40, 40, LV_GRID_TEMPLATE_LAST };
    static int16_t row_dsc[] = { 50, 50, 50, LV_GRID_TEMPLATE_LAST };

    /*Create a container with grid*/

    // Ovo je container na novom screenu
    lv_obj_t *cont = lv_obj_create(novi_screen);
    lv_obj_set_style_grid_column_dsc_array(cont, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, 0);
    lv_obj_set_size(cont, 200, 220);
    lv_obj_center(cont);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);

    uint32_t i;

    for(i = 0; i < 9; i++)
    {
        uint8_t col = i % 3;
        uint8_t row = i / 3;

        // Buttoni koji su vezani na gridu

        array_buttona[i] = lv_btn_create(cont);
        /*Stretch the cell horizontally and vertically too
         *Set span to 1 to make the cell 1 column/row sized*/
        lv_obj_set_grid_cell(array_buttona[i], LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);

        array_labela[i] = lv_label_create(array_buttona[i]);
        lv_label_set_text_fmt(array_labela[i], " ");
        lv_obj_center(array_labela[i]);

        (void)lv_obj_add_event_cb(array_buttona[i], _button_event_handler, LV_EVENT_CLICKED, NULL);
    }
}

//---------------------------- INTERRUPT HANDLERS -----------------------------
