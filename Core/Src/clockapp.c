#include "clockapp.h"
#include "shift595.h"
#include "clockface.h"
#include "clocktime.h"
#include "ds3231.h"
#include "bme280.h"
#include "buttons.h"
#include "buzzer.h"

static bme280_data_t cur_env = { 0, 0u, 0u };

/* ---- Display modes (cycled with the MODE button) ---- */
typedef enum
{
    MODE_TIME = 0,
    MODE_DATE,
    MODE_TH,          /* temperature / humidity */
    MODE_ALARM,
    MODE_COUNT
} disp_mode_t;

/* ---- Edit state within a mode ---- */
typedef enum
{
    ED_NONE = 0,
    ED_HOURS,
    ED_MINUTES
} edit_state_t;

/* ---- Parameters ---- */
#define BLINK_PERIOD_MS   500u
#define SYNC_EACH_MINUTE  1u

/* ---- Internal state ---- */
static disp_mode_t  disp_mode = MODE_TIME;
static edit_state_t edit      = ED_NONE;
static uint8_t      edit_hours = 0u;
static uint8_t      edit_min   = 0u;
static uint32_t     blink_ref  = 0u;
static uint32_t     last_frame = 0xFFFFFFFFu;
static clock_date_t cur_date  = { 1u, 1u, 0u };
static uint8_t      need_refresh = 1u;   /* re-read this mode's data source */
static uint8_t      alarm_ringing = 0u;  /* alarm is currently ringing */
static uint32_t     ring_started  = 0u;  /* when it started ringing (for timeout) */
static uint8_t      alarm_h = 0u;        /* alarm time (cached for display) */
static uint8_t      alarm_m = 0u;
static uint8_t      alarm_on = 0u;       /* alarm enabled */
static uint8_t      baro_mask = 0x04u;
static uint32_t     baro_next = 0u;

#define BARO_PERIOD_MS   60000u
#define BARO_NORMAL_PA   100600u
/* ---- Display ---- */

static uint32_t compose(uint8_t hours, uint8_t minutes, uint8_t mode,
                        uint8_t blink_hours, uint8_t blink_min)
{
    uint8_t on = (((HAL_GetTick() - blink_ref) / BLINK_PERIOD_MS) & 1u) == 0u;

    uint8_t h = (blink_hours && !on) ? 0u : hours;
    uint8_t m = (blink_min   && !on) ? 0u : minutes;

    return face_build(h, m, mode, alarm_on, baro_mask);
}

static void push(uint32_t frame)
{
    if (frame != last_frame)
    {
        last_frame = frame;
        sr595_write(frame);
    }
}

/* Current-mode bit indicator on the third row */
static uint8_t mode_bits(void)
{
    return (uint8_t)(1u << disp_mode);
}

/* ---- Public ---- */

void clockapp_init(void)
{
    clock_time_t t = { 12u, 0u, 0u };

    if (ds3231_lost_power())
    {
        ds3231_write_time(&t);
    }

    if (ds3231_read_time(&t) == HAL_OK)
    {
        clock_time_init(t.hours, t.minutes, t.seconds);
    }

    disp_mode = MODE_TIME;
    edit      = ED_NONE;
    blink_ref = HAL_GetTick();

    alarm_on = ds3231_alarm_is_enabled();
}

void clockapp_task(void)
{
    buttons_update();

    uint8_t tick = 0u;

    /* The clock keeps running regardless of the displayed mode */
    if (clock_time_update())
    {
        tick = 1u;
        const clock_time_t *cur = clock_time_get();
        if (SYNC_EACH_MINUTE && cur->seconds == 0u)
        {
            clock_time_t hw;
            if (ds3231_read_time(&hw) == HAL_OK)
            {
                clock_time_init(hw.hours, hw.minutes, hw.seconds);
            }
        }
    }

    /* --- Alarm: DS3231 INT pin is active-low --- */
    if (!alarm_ringing &&
        HAL_GPIO_ReadPin(ALARM_INPUT_GPIO_Port, ALARM_INPUT_Pin) == GPIO_PIN_RESET)
    {
        ds3231_alarm_clear();          /* clearing A1F releases the pin back to 1 */
        if (ds3231_alarm_is_enabled())
        {
            alarm_ringing = 1u;
            ring_started  = HAL_GetTick();
            buzzer_beep(0u, 300u, 300u);   /* rings continuously */
        }
    }

    if (alarm_ringing)
    {
        uint8_t any_btn = button_pressed(BTN_MODE) | button_pressed(BTN_SET) |
                          button_pressed(BTN_PREV) | button_pressed(BTN_NEXT);

        if (any_btn || (int32_t)(HAL_GetTick() - (ring_started + 60000u)) >= 0)
        {
            buzzer_stop();
            alarm_ringing = 0u;
            alarm_on = 0u;              /* one-shot: turn the alarm off */
            ds3231_alarm_enable(0u);    /* clear A1IE on the DS3231 (won't ring again tomorrow) */
        }
    }

    if ((int32_t)(HAL_GetTick() - baro_next) >= 0)
    {
        baro_next = HAL_GetTick() + BARO_PERIOD_MS;
        bme280_read(&cur_env);

        uint32_t p = cur_env.pressure_pa;
        uint8_t zone;
        if      (p < BARO_NORMAL_PA - 1600u) { zone = 0u; }   /* storm */
        else if (p < BARO_NORMAL_PA -  600u) { zone = 1u; }   /* rain  */
        else if (p <= BARO_NORMAL_PA + 600u) { zone = 2u; }   /* normal*/
        else if (p <= BARO_NORMAL_PA + 1600u){ zone = 3u; }   /* clear */
        else                                 { zone = 4u; }   /* dry   */
        baro_mask = (uint8_t)(1u << zone);
    }

    uint8_t mode_click = button_pressed(BTN_MODE);
    if (edit == ED_NONE && mode_click)
    {
        disp_mode = (disp_mode_t)((disp_mode + 1u) % (uint8_t)MODE_COUNT);
        blink_ref = HAL_GetTick();
        need_refresh = 1u;    
    }

    uint32_t frame = 0u;

    switch (disp_mode)
    {
    /* ---------- TIME (+ editing) ---------- */
    case MODE_TIME:
    {
        if (edit == ED_NONE)
        {
            if (button_pressed(BTN_SET))
            {
                const clock_time_t *cur = clock_time_get();
                edit_hours = cur->hours;
                edit_min   = cur->minutes;
                edit       = ED_HOURS;
                blink_ref  = HAL_GetTick();
            }

            const clock_time_t *cur = clock_time_get();
            frame = compose(cur->hours, cur->minutes, mode_bits(), 0u, 0u);
        }
        else if (edit == ED_HOURS)
        {
            if (button_pressed(BTN_NEXT)) { edit_hours = (uint8_t)((edit_hours + 1u)  % 24u); }
            if (button_pressed(BTN_PREV)) { edit_hours = (uint8_t)((edit_hours + 23u) % 24u); }
            if (button_pressed(BTN_SET))  { edit = ED_MINUTES; blink_ref = HAL_GetTick(); }

            frame = compose(edit_hours, edit_min, mode_bits(), 1u, 0u);
        }
        else /* ED_MINUTES */
        {
            if (button_pressed(BTN_NEXT)) { edit_min = (uint8_t)((edit_min + 1u)  % 60u); }
            if (button_pressed(BTN_PREV)) { edit_min = (uint8_t)((edit_min + 59u) % 60u); }
            if (button_pressed(BTN_SET))
            {
                clock_time_t t = { edit_hours, edit_min, 0u };
                ds3231_write_time(&t);
                clock_time_init(edit_hours, edit_min, 0u);
                edit      = ED_NONE;
                blink_ref = HAL_GetTick();
            }

            frame = compose(edit_hours, edit_min, mode_bits(), 0u, 1u);
        }
        break;
    }


    case MODE_DATE:
    {
        /* Read the date on mode entry and once a second, not every iteration */
        if (need_refresh || tick)
        {
            ds3231_read_date(&cur_date);
        }
        frame = compose(cur_date.day, cur_date.month, mode_bits(), 0u, 0u);
        break;
    }
    case MODE_TH:
    {
        if (need_refresh || tick)
        {
            bme280_read(&cur_env);
        }

        /* Temperature -> hours row, whole °C */
        uint8_t tC = (cur_env.temp_c100 < 0) ? 0u
                                             : (uint8_t)(cur_env.temp_c100 / 100);

        uint8_t hum = (cur_env.humidity > 99u) ? 99u : cur_env.humidity;

        frame = compose(tC, hum, mode_bits(), 0u, 0u);
        break;
    }
    /* ---------- ALARM (display + editing + on/off) ---------- */
    case MODE_ALARM:
    {
        /* Read the alarm from the DS3231 on mode entry */
        if (need_refresh)
        {
            ds3231_get_alarm(&alarm_h, &alarm_m);
            alarm_on = ds3231_alarm_is_enabled();
        }

        if (edit == ED_NONE)
        {
            if (button_pressed(BTN_SET))
            {
                edit_hours = alarm_h;
                edit_min   = alarm_m;
                edit       = ED_HOURS;
                blink_ref  = HAL_GetTick();
            }
            if (button_pressed(BTN_NEXT)) { alarm_on = 1u; ds3231_alarm_enable(1u); }
            if (button_pressed(BTN_PREV)) { alarm_on = 0u; ds3231_alarm_enable(0u); }

            /* disabled -> whole display blinks; enabled -> steady */
            frame = compose(alarm_h, alarm_m, mode_bits(),
                            alarm_on ? 0u : 1u, alarm_on ? 0u : 1u);
        }
        else if (edit == ED_HOURS)
        {
            if (button_pressed(BTN_NEXT)) { edit_hours = (uint8_t)((edit_hours + 1u)  % 24u); }
            if (button_pressed(BTN_PREV)) { edit_hours = (uint8_t)((edit_hours + 23u) % 24u); }
            if (button_pressed(BTN_SET))  { edit = ED_MINUTES; blink_ref = HAL_GetTick(); }

            frame = compose(edit_hours, edit_min, mode_bits(), 1u, 0u);
        }
        else /* ED_MINUTES */
        {
            if (button_pressed(BTN_NEXT)) { edit_min = (uint8_t)((edit_min + 1u)  % 60u); }
            if (button_pressed(BTN_PREV)) { edit_min = (uint8_t)((edit_min + 59u) % 60u); }
            if (button_pressed(BTN_SET))
            {
                ds3231_set_alarm(edit_hours, edit_min);
                ds3231_alarm_enable(1u);        /* setting the alarm enables it */
                alarm_h = edit_hours; alarm_m = edit_min; alarm_on = 1u;
                edit = ED_NONE; blink_ref = HAL_GetTick();
            }

            frame = compose(edit_hours, edit_min, mode_bits(), 0u, 1u);
        }
        break;
    }

    default:
        frame = compose(0u, 0u, mode_bits(), 0u, 0u);
        break;
    }

    push(frame);
    need_refresh = 0u;
}