/****************************************************************
 * In dien er onder de ultrasonen iets komt te staan
 * (de lijn word onderbroken/ er wordt iets gededecteerd)
 * moet er een timer starten van 10seconden
 * indien er in die 10seconden nogsteeds iets gedetecteerd word
 * gaat de userled aan.
 ****************************************************************/

#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

#define TRIGGER_PIN (P13_1)
#define ECHO_PIN (P13_2)
#define LED_PIN CYBSP_USER_LED
#define POTENTIOMETER_PIN (P10_0)

cyhal_timer_t timer_obj;
cyhal_timer_cfg_t timer_cfg = {
    .period = 10000000, // 10 seconden in microseconden
    .direction = CYHAL_TIMER_DIR_UP,
    .is_compare = false,
    .is_continuous = false,
    .compare_value = 0
};

cyhal_pwm_t pwm_obj;

cyhal_adc_t adc_obj;
cyhal_adc_channel_t adc_channel_obj;

_Bool object_detected = false;
uint32_t echo_start_time = 0;
uint32_t echo_end_time = 0;
uint32_t distance = 0; // Declaratie van de variabele 'distance'
uint32_t threshold = 0;

void timer_handler(void *callback_arg, cyhal_timer_event_t event)
{
    if (event & CYHAL_TIMER_IRQ_TERMINAL_COUNT)
    {
        // Als de timer is afgelopen en er is nog steeds een object gedetecteerd, zet de LED aan
        if (object_detected)
        {
            cyhal_gpio_write(LED_PIN, false);
        }
    }
}

void echo_handler(void *handler_arg, cyhal_gpio_event_t event)
{
    if (event == CYHAL_GPIO_IRQ_RISE)
    {
        // Noteer de tijd bij de stijgende flank
        echo_start_time = cyhal_timer_read(&timer_obj);
        cyhal_timer_start(&timer_obj); // Start de timer bij de stijgende flank
    }
    else if (event == CYHAL_GPIO_IRQ_FALL)
    {
        // Noteer de tijd bij de dalende flank
        echo_end_time = cyhal_timer_read(&timer_obj);
        cyhal_timer_stop(&timer_obj); // Stop de timer bij de dalende flank
        distance = echo_end_time - echo_start_time;

        // Controleer of de afstand kleiner is dan de drempelwaarde
        object_detected = (distance > threshold);

        // Zet de LED op basis van object detectie
        if (object_detected)
        {
            cyhal_gpio_write(LED_PIN, false);
        }
        else
        {
            cyhal_gpio_write(LED_PIN, true);
        }
    }
}

void setup_pwm_trigger(void)
{
    cyhal_pwm_init(&pwm_obj, TRIGGER_PIN, NULL);
    cyhal_pwm_set_duty_cycle(&pwm_obj, 50.0, 60.0); // 50% duty cycle, 60Hz
    cyhal_pwm_start(&pwm_obj);
}

int main(void)
{
    cyhal_gpio_callback_data_t echo_callback_data =
    {
        .callback = echo_handler,
        .callback_arg = NULL
    };

    cyhal_adc_channel_config_t channel_config =
    {
        .enable_averaging = false,
        .min_acquisition_ns = 220,
        .enabled = true
    };

    // Initialisaties
    cybsp_init();

    // printf functie initialisatie
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    // GPIO initialisaties
    cyhal_gpio_init(ECHO_PIN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, CYBSP_LED_STATE_OFF);
    cyhal_gpio_init(LED_PIN, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    // GPIO interrupt initialisatie voor echo pin
    cyhal_gpio_register_callback(ECHO_PIN, &echo_callback_data);
    cyhal_gpio_enable_event(ECHO_PIN, CYHAL_GPIO_IRQ_BOTH, 3, true);

    // Timer initialisatie
    cyhal_timer_init(&timer_obj, NC, NULL);
    cyhal_timer_configure(&timer_obj, &timer_cfg);
    cyhal_timer_register_callback(&timer_obj, timer_handler, NULL);
    cyhal_timer_enable_event(&timer_obj, CYHAL_TIMER_IRQ_TERMINAL_COUNT, 3, true);

    // ADC initialisatie
    cyhal_adc_init(&adc_obj, POTENTIOMETER_PIN, NULL);
    cyhal_adc_channel_init_diff(&adc_channel_obj, &adc_obj, POTENTIOMETER_PIN, CYHAL_ADC_VNEG, &channel_config);

    // PWM setup voor ultrasone trigger
    setup_pwm_trigger();

    __enable_irq();

    // Hoofdprogramma
    for (;;)
    {
        // Lees de analoge waarde van de potentiometer
        int32_t potentiometer_value = cyhal_adc_read_u16(&adc_channel_obj);
        printf("potentiometer waarde: %d\n", potentiometer_value);

        // Pas de drempelwaarde van de ultrasone sensor aan op basis van de potentiometerwaarde
        threshold = potentiometer_value / 20; // Voorbeeld: pas de drempelwaarde aan op basis van de potentiometerwaarde

        // Zet het systeem in slaapmodus om energie te besparen
        cyhal_syspm_sleep();
    }
}
