#include <twr_fuel_cell_module.h>

#include <twr_log.h>

static struct
{
    bool _initialized;
    twr_module_fuel_cell_state_t _state;

    twr_scheduler_task_id_t _task_id_measure;
    twr_scheduler_task_id_t _task_id_interval;
    twr_tick_t _update_interval;
    twr_tick_t _tick_ready;
    uint16_t _reg_result;
    bool _voltage_valid;
    bool _measurement_active;
    void (*_event_handler)(twr_module_fuel_cell_event_t, void *);
    void *_event_param;

} _twr_module_fuel_cell;

static void _twr_module_fuel_cell_task_measure(void *param);
static void _twr_module_fuel_cell_task_interval(void *param);

bool twr_module_fuel_cell_init(void)
{
    memset(&_twr_module_fuel_cell, 0, sizeof(_twr_module_fuel_cell));

    _twr_module_fuel_cell._task_id_interval = twr_scheduler_register(_twr_module_fuel_cell_task_interval, NULL, TWR_TICK_INFINITY);
    _twr_module_fuel_cell._task_id_measure = twr_scheduler_register(_twr_module_fuel_cell_task_measure, NULL, _TWR_MODULE_FUEL_CELL_DELAY_RUN);

    _twr_module_fuel_cell._initialized = true;

    return true;
}

bool twr_module_fuel_cell_measure(void)
{
    if (_twr_module_fuel_cell._measurement_active)
    {
        return false;
    }

    _twr_module_fuel_cell._measurement_active = true;

    twr_scheduler_plan_absolute(_twr_module_fuel_cell._task_id_measure, _twr_module_fuel_cell._tick_ready);

    return true;
}

static void _twr_module_fuel_cell_task_interval(void *param)
{
    (void) param;
    twr_module_fuel_cell_measure();

    twr_log_debug("task interval");
    twr_scheduler_plan_current_relative(_twr_module_fuel_cell._update_interval);
}

static void _twr_module_fuel_cell_task_measure(void *param)
{
    twr_log_debug("task measure %d", _twr_module_fuel_cell._state);

    (void) param;

    start:

    switch (_twr_module_fuel_cell._state)
    {
        case TWR_MODULE_FUEL_CELL_STATE_ERROR:
        {
            twr_log_debug("error");

            if (_twr_module_fuel_cell._event_handler != NULL)
            {
                _twr_module_fuel_cell._event_handler(TWR_MODULE_FUEL_CELL_EVENT_ERROR, _twr_module_fuel_cell._event_param);
            }

            _twr_module_fuel_cell._state = TWR_MODULE_FUEL_CELL_STATE_INITIALIZE;

            return;
        }
        case TWR_MODULE_FUEL_CELL_STATE_INITIALIZE:
        {
            twr_log_debug("init");
            _twr_module_fuel_cell._state = TWR_MODULE_FUEL_CELL_STATE_ERROR;

            if (!twr_i2c_memory_write_16b(TWR_I2C_I2C0, _TWR_MODULE_FUEL_CELL_I2C_TLA2021_ADDRESS, 0x01, 0x8503))
            {
                twr_log_debug("error init");

                goto start;
            }

            _twr_module_fuel_cell._state = TWR_MODULE_FUEL_CELL_STATE_MEASURE;

            _twr_module_fuel_cell._tick_ready = twr_tick_get();

            if (_twr_module_fuel_cell._measurement_active)
            {
                twr_scheduler_plan_current_absolute(_twr_module_fuel_cell._tick_ready);
            }

            return;
        }
        case TWR_MODULE_FUEL_CELL_STATE_MEASURE:
        {
            twr_log_debug("measure");
            _twr_module_fuel_cell._state = TWR_MODULE_FUEL_CELL_STATE_ERROR;

            if (!twr_i2c_memory_write_16b(TWR_I2C_I2C0, _TWR_MODULE_FUEL_CELL_I2C_TLA2021_ADDRESS, 0x01, 0x8503))
            {
                twr_log_debug("error measure");

                goto start;
            }

            _twr_module_fuel_cell._state = TWR_MODULE_FUEL_CELL_STATE_READ;

            twr_scheduler_plan_current_from_now(_TWR_MODULE_FUEL_CELL_DELAY_MEASUREMENT);

            return;
        }
        case TWR_MODULE_FUEL_CELL_STATE_READ:
        {
            twr_log_debug("read");
            _twr_module_fuel_cell._state = TWR_MODULE_FUEL_CELL_STATE_ERROR;

            uint16_t reg_configuration;

            if (!twr_i2c_memory_read_16b(TWR_I2C_I2C0, _TWR_MODULE_FUEL_CELL_I2C_TLA2021_ADDRESS, 0x01, &reg_configuration))
            {
                twr_log_debug("error read 0");

                goto start;
            }

            if ((reg_configuration & 0x8000) != 0x8000)
            {
                twr_log_debug("error read 1");

                goto start;
            }

            if (!twr_i2c_memory_read_16b(TWR_I2C_I2C0, _TWR_MODULE_FUEL_CELL_I2C_TLA2021_ADDRESS, 0x00, &_twr_module_fuel_cell._reg_result))
            {
                twr_log_debug("error read 2");

                goto start;
            }

            _twr_module_fuel_cell._voltage_valid = true;

            _twr_module_fuel_cell._state = TWR_MODULE_FUEL_CELL_STATE_UPDATE;

            twr_log_debug("%d", reg_configuration);

            twr_log_debug("error read 3");

            goto start;
        }
        case TWR_MODULE_FUEL_CELL_STATE_UPDATE:
        {
            twr_log_debug("update");
            _twr_module_fuel_cell._measurement_active = false;

            if (_twr_module_fuel_cell._event_handler != NULL)
            {
                _twr_module_fuel_cell._event_handler(TWR_MODULE_FUEL_CELL_EVENT_VOLTAGE, _twr_module_fuel_cell._event_param);
            }

            _twr_module_fuel_cell._state = TWR_MODULE_FUEL_CELL_STATE_MEASURE;

            return;
        }
        default:
        {
            _twr_module_fuel_cell._state = TWR_MODULE_FUEL_CELL_STATE_ERROR;

            twr_log_debug("error default");

            goto start;
        }
    }
}

bool twr_module_fuel_cell_get_voltage(float *voltage)
{
    if (!_twr_module_fuel_cell._voltage_valid)
    {
        return false;
    }

    int16_t reg_result = _twr_module_fuel_cell._reg_result;

    if (reg_result == 0x7ff0)
        return false;

    twr_log_debug("RAW: %u", (uint32_t) (reg_result >> 4));



    *voltage = reg_result < 0 ? 0 : (uint64_t) (reg_result >> 4) * 2048 * (TWR_FUEL_CELL_R1_DIVIDER + TWR_FUEL_CELL_R2_DIVIDER) / (2047 * TWR_FUEL_CELL_R2_DIVIDER);
    *voltage /= 1000.f;

    return true;
}

void twr_module_fuel_cell_set_update_interval(twr_tick_t interval)
{
    _twr_module_fuel_cell._update_interval = interval;

    if (_twr_module_fuel_cell._update_interval == TWR_TICK_INFINITY)
    {
        twr_scheduler_plan_absolute(_twr_module_fuel_cell._task_id_interval, TWR_TICK_INFINITY);
    }
    else
    {
        twr_scheduler_plan_relative(_twr_module_fuel_cell._task_id_interval, _twr_module_fuel_cell._update_interval);

        twr_module_fuel_cell_measure();
    }
}

void twr_module_fuel_cell_set_event_handler(void (*event_handler)(twr_module_fuel_cell_event_t, void *), void *event_param)
{
    _twr_module_fuel_cell._event_handler = event_handler;
    _twr_module_fuel_cell._event_param = event_param;
}
