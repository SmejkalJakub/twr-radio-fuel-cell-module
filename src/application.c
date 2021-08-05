#include <application.h>
#include <twr_fuel_cell_module.h>

#define VOLTAGE_UPDATE_INTERVAL (5 * 1000)

#define BATTERY_UPDATE_INTERVAL (1 * 60 * 1000)
#define APPLICATION_TASK_ID 0

#define VOLTAGE_GRAPH (5 * 60 * 1000)

TWR_DATA_STREAM_FLOAT_BUFFER(voltage_stream_buffer, (VOLTAGE_GRAPH / VOLTAGE_UPDATE_INTERVAL))

// LED instance
twr_led_t led;

twr_led_t lcd_led_green;
twr_led_t lcd_led_blue;
twr_led_t lcd_led_red;

// Button instance
twr_button_t button;

twr_gfx_t *gfx;

float voltage = NAN;
float x = 0;

int points = 0;

twr_data_stream_t voltage_stream;

int game_counter = 0;
int game_counter_stop = 5;

int counter = 6;
bool game_active = false;

bool timer_active = false;
bool timer_done = false;
bool voltage_low = false;

int point_x = 63;
int point_y = 80;

bool page = true;

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    if (event == TWR_BUTTON_EVENT_PRESS)
    {
        twr_scheduler_plan_now(APPLICATION_TASK_ID);
    }
}

void twr_radio_set_power(int dbm_power)
{
    for(int i = 0; i < 8; i++)
    {
        SpiritRadioSetPALeveldBm(i, dbm_power);
    }
}

void fast_radio_messages()
{
    if(!voltage_low)
    {
        twr_radio_pub_int("game/points", &points);

        game_counter++;

        if(game_counter == game_counter_stop)
        {
            game_active = true;
            game_counter = 0;

            game_counter_stop = (rand() % 4) + 4;
            twr_log_debug("%d", game_counter_stop);

            twr_scheduler_plan_current_from_now(800);
        }
        else
        {
            game_active = false;
            twr_scheduler_plan_current_from_now(300);
            twr_led_pulse(&lcd_led_blue, 100);
        }
    }
    else
    {
        timer_done = false;
        game_active = false;
    }

    twr_scheduler_plan_now(APPLICATION_TASK_ID);
}

void countdown_timer()
{
    counter--;
    if(counter > 0)
    {
        twr_led_pulse(&lcd_led_blue, 300);
        twr_scheduler_plan_current_from_now(1000);
    }
    else
    {
        counter = 6;
        timer_active = false;

        timer_done = true;

        twr_scheduler_register(fast_radio_messages, NULL, 0);
    }
    twr_scheduler_plan_now(APPLICATION_TASK_ID);
}

void lcd_event_handler(twr_module_lcd_event_t event, void *event_param)
{
    if(event == TWR_MODULE_LCD_EVENT_RIGHT_PRESS && !voltage_low)
    {
        if(timer_done && game_active)
        {
            twr_log_debug("points++");
            twr_led_pulse(&lcd_led_green, 200);
            points++;
        }
        else if(timer_done && !game_active)
        {
            twr_log_debug("points--");

            twr_led_pulse(&lcd_led_red, 200);
            points--;
        }
    }
    else if(event == TWR_MODULE_LCD_EVENT_LEFT_PRESS && !voltage_low)
    {
        if(timer_done && game_active)
        {
            twr_log_debug("points++");

            twr_led_pulse(&lcd_led_green, 200);
            points++;
        }
        else if(timer_done && !game_active)
        {
            twr_log_debug("points--");

            twr_led_pulse(&lcd_led_red, 200);
            points--;
        }
    }
    else if(event == TWR_MODULE_LCD_EVENT_BOTH_HOLD  && !timer_done)
    {
        timer_active = true;
        twr_scheduler_register(countdown_timer, NULL, 0);
    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    voltage = NAN;

    if (twr_module_battery_get_voltage(&voltage))
    {
        twr_radio_pub_battery(&voltage);
    }
}

void fuel_cell_module_event_handler(twr_module_fuel_cell_event_t event, void *param)
{
    if(event == TWR_MODULE_FUEL_CELL_EVENT_VOLTAGE && !voltage_low)
    {
        voltage = NAN;
        twr_module_fuel_cell_get_voltage(&voltage);

        twr_log_debug("voltage: %.2f", voltage);

        if(voltage != NAN)
        {
            twr_data_stream_feed(&voltage_stream, &voltage);
        }
        if(voltage <= 0.98)
        {
            twr_log_debug("voltage low");
            game_active = false;
            voltage_low = true;
        }
    }
    twr_scheduler_plan_now(APPLICATION_TASK_ID);

}

void application_init(void)
{
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    twr_data_stream_init(&voltage_stream, 1, &voltage_stream_buffer);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_set_power(POWER_DBM);
    twr_radio_pairing_request("fuel-cell", VERSION);

    twr_module_lcd_init();
    twr_module_lcd_set_event_handler(lcd_event_handler, NULL);
    gfx = twr_module_lcd_get_gfx();

    const twr_led_driver_t* driver = twr_module_lcd_get_led_driver();
    twr_led_init_virtual(&lcd_led_green, TWR_MODULE_LCD_LED_GREEN, driver, 1);
    twr_led_init_virtual(&lcd_led_blue, TWR_MODULE_LCD_LED_BLUE, driver, 1);
    twr_led_init_virtual(&lcd_led_red, TWR_MODULE_LCD_LED_RED, driver, 1);

    twr_module_fuel_cell_init();
    twr_module_fuel_cell_set_event_handler(fuel_cell_module_event_handler, NULL);
    twr_module_fuel_cell_set_update_interval(VOLTAGE_UPDATE_INTERVAL);

    twr_led_pulse(&led, 2000);
}

void graph(twr_gfx_t *gfx, int x0, int y0, int x1, int y1, twr_data_stream_t *data_stream, int time_step, float min_value, float max_value, int number_of_y_parts, bool grid_lines, const char *format)
{
    int w, h;
    char str[32];
    int width = x1 - x0;
    int height = y1 - y0;

    twr_module_lcd_set_font(&twr_font_ubuntu_11);

    int number_of_samples = twr_data_stream_get_number_of_samples(data_stream);

    int end_time = - number_of_samples * time_step / 1000;

    h = 10;

    int first_line_x = 40;

    float range = fabsf(max_value) + fabsf(min_value);
    float fh = height - h - 2;

    snprintf(str, sizeof(str), "%ds", end_time);
    w = twr_gfx_calc_string_width(gfx, str) + 8;

    int lines = width / w;
    int y_time = y1 - h - 2;
    int y_zero = range > 0 ? y_time - ((fabsf(min_value) / range) * fh) : y_time;
    int tmp;

    for (int i = 0, time_step = end_time / lines, w_step = width / lines; i < lines; i++)
    {
        snprintf(str, sizeof(str), "%ds", time_step * i);

        w = twr_gfx_calc_string_width(gfx, str);

        tmp = width - w_step * i;
        first_line_x = tmp;

        twr_gfx_draw_string(gfx, tmp - w, y1 - h, str, 1);

        twr_gfx_draw_line(gfx, tmp - 3, y_zero - 2, tmp - 3, y_zero + 2, 1);
        twr_gfx_draw_line(gfx, tmp - 2, y_zero - 2, tmp - 2, y_zero + 2, 1);
        twr_gfx_draw_line(gfx, tmp - 1, y_zero - 2, tmp - 1, y_zero + 2, 1);

        twr_gfx_draw_line(gfx, tmp - 3, y0, tmp - 3, y0 + 2, 1);
        twr_gfx_draw_line(gfx, tmp - 2, y0, tmp - 2, y0 + 2, 1);
        twr_gfx_draw_line(gfx, tmp - 1, y0, tmp - 1, y0 + 2, 1);


        if(grid_lines)
        {
            twr_gfx_draw_line(gfx, tmp - 2, y_zero - 2, tmp - 2, y0, 1);
        }

        if (y_time != y_zero)
        {
            twr_gfx_draw_line(gfx, tmp - 2, y_time - 2, tmp - 2, y_time, 1);
        }
    }

    twr_gfx_draw_line(gfx, x0, y_zero, x1, y_zero, 1);

    if (y_time != y_zero)
    {
        twr_gfx_draw_line(gfx, x0, y_time, y1, y_time, 1);

        snprintf(str, sizeof(str), format, min_value);

        twr_gfx_draw_string(gfx, x0, y_time - 10, str, 1);
    }

    twr_gfx_draw_line(gfx, x0, y0, x1, y0, 1);

    snprintf(str, sizeof(str), format, max_value);

    twr_gfx_draw_string(gfx, x0, y0, str, 1);

    twr_gfx_draw_string(gfx, x0, y_zero - 10, "0", 1);

    int step = (y_zero - y0) / (number_of_y_parts - 1);
    float step_number = (max_value - min_value) / (float)(number_of_y_parts - 1);

    for(int i = 1; i < number_of_y_parts - 1; i++)
    {
        twr_gfx_draw_line(gfx, x0, (y_zero - step * i) - 1, x0 + 2, (y_zero - step * i) - 1, 1);
        twr_gfx_draw_line(gfx, x0, (y_zero - step * i), x0 + 2, (y_zero - step * i), 1);
        twr_gfx_draw_line(gfx, x0, (y_zero - step * i) + 1, x0 + 2, (y_zero - step * i) + 1, 1);

        twr_gfx_printf(gfx, x0, (y_zero - step * i), 1, "%.2f", step_number * i);

        if(grid_lines)
        {
            twr_gfx_draw_line(gfx, x0, (y_zero - step * i), x1, (y_zero - step * i), 1);
        }
    }

    if (range == 0)
    {
        return;
    }

    int length = twr_data_stream_get_length(data_stream);
    float value;

    int x_zero = x1 - 2;
    float fy;

    int dx = width / (number_of_samples - 1);
    int point_x = x_zero + dx;
    int point_y;
    int last_x;
    int last_y;

    min_value = fabsf(min_value);

    for (int i = 1; i <= length; i++)
    {
        if (twr_data_stream_get_nth(data_stream, -i, &value))
        {
            fy = (value + min_value) / range;

            point_y = y_time - (fy * fh);
            point_x -= dx;

            if (i == 1)
            {
                last_y = point_y;
                last_x = point_x;
            }

            if(point_x > first_line_x)
            {
                twr_gfx_draw_fill_rectangle(gfx, point_x - 1, point_y - 1, point_x + 1, point_y + 1, 1);

                twr_gfx_draw_line(gfx, point_x, point_y, last_x, last_y, 1);
            }

            last_y = point_y;
            last_x = point_x;

        }
    }
}

void application_task(void)
{
    twr_adc_async_measure(TWR_ADC_CHANNEL_A0);

    if (!twr_gfx_display_is_ready(gfx))
    {
        return;
    }

    twr_system_pll_enable();

    twr_gfx_clear(gfx);

    if(voltage_low)
    {
        twr_gfx_set_font(gfx, &twr_font_ubuntu_33);
        twr_gfx_printf(gfx, 15, 50, 1, "Konec");
    }
    else if (page && !timer_active && !timer_done)
    {
        int w;
        twr_gfx_set_font(gfx, &twr_font_ubuntu_15);
        w = twr_gfx_draw_string(gfx, 5, 23, "Charge", 1);

        twr_gfx_set_font(gfx, &twr_font_ubuntu_28);
        w = twr_gfx_printf(gfx, w + 5, 15, 1, "%.2f", voltage);

        twr_gfx_set_font(gfx, &twr_font_ubuntu_15);
        twr_gfx_draw_string(gfx, w + 5, 23, "V", 1);

        graph(gfx, 0, 40, 127, 127, &voltage_stream, VOLTAGE_UPDATE_INTERVAL, 0, 1.5f, 4, true, "%.2f" "\xb0" "V");
    }
    else if(timer_active)
    {
        twr_gfx_set_font(gfx, &twr_font_ubuntu_33);
        twr_gfx_printf(gfx, 55, 50, 1, "%d", counter);
    }
    else if(timer_done)
    {
        int w;
        twr_gfx_set_font(gfx, &twr_font_ubuntu_15);
        w = twr_gfx_draw_string(gfx, 5, 23, "Charge", 1);

        twr_gfx_set_font(gfx, &twr_font_ubuntu_28);
        w = twr_gfx_printf(gfx, w + 5, 15, 1, "%.2f", voltage);

        twr_gfx_set_font(gfx, &twr_font_ubuntu_15);
        twr_gfx_draw_string(gfx, w + 5, 23, "V", 1);

        graph(gfx, 0, 40, 127, 127, &voltage_stream, VOLTAGE_UPDATE_INTERVAL, 0, 1.5f, 4, true, "%.2f" "\xb0" "V");
    }

    twr_gfx_update(gfx);

    twr_system_pll_disable();
}


