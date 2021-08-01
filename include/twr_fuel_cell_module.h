#ifndef _TWR_MODULE_FUEL_CELL_H
#define _TWR_MODULE_FUEL_CELL_H

#include <twr_tick.h>
#include <twr_sc16is740.h>
#include <twr_scheduler.h>
#include <twr_fifo.h>

#define _TWR_MODULE_FUEL_CELL_I2C_TLA2021_ADDRESS 0x4B
#define _TWR_MODULE_FUEL_CELL_DELAY_MEASUREMENT 200

#define _TWR_MODULE_FUEL_CELL_DELAY_RUN 1000

#define TWR_FUEL_CELL_R1_DIVIDER 10000
#define TWR_FUEL_CELL_R2_DIVIDER 10000

typedef enum
{
    TWR_MODULE_FUEL_CELL_STATE_ERROR = -1,
    TWR_MODULE_FUEL_CELL_STATE_INITIALIZE = 0,
    TWR_MODULE_FUEL_CELL_STATE_MEASURE = 1,
    TWR_MODULE_FUEL_CELL_STATE_READ = 2,
    TWR_MODULE_FUEL_CELL_STATE_UPDATE = 3

} twr_module_fuel_cell_state_t;

typedef enum
{
    //! @brief Error event
    TWR_MODULE_FUEL_CELL_EVENT_ERROR = 0,

    //! @brief Update event
    TWR_MODULE_FUEL_CELL_EVENT_VOLTAGE = 1,

} twr_module_fuel_cell_event_t;

bool twr_module_fuel_cell_init(void);
bool twr_module_fuel_cell_measure(void);
void twr_module_fuel_cell_set_event_handler(void (*event_handler)(twr_module_fuel_cell_event_t, void *), void *event_param);

void twr_module_fuel_cell_set_update_interval(twr_tick_t interval);

bool twr_module_fuel_cell_get_voltage(float *voltage);


#endif
