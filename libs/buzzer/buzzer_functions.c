#include "buzzer_functions.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include <stdio.h>  // Necessário para printf
#include <stdlib.h> // Necessário para funções da stdlib

// Frequências aceitáveis para o buzzer
const uint buzzer_frequencies[] = {2500, 800, 400, 3500, 4500};
const char *buzzer_colors[] = {"branco", "rosa", "marrom", "azul", "violeta"};
const uint num_frequencies = sizeof(buzzer_frequencies) / sizeof(buzzer_frequencies[0]);
volatile uint current_frequency_index = 0;

// Variável para controlar se o buzzer deve tocar ou parar
volatile bool buzzer_active = true;

// Variáveis para debouncing
volatile uint32_t last_button_a_press = 0;
volatile uint32_t last_button_b_press = 0;
const uint32_t debounce_delay = 200; // Tempo de debounce (em milissegundos)


// Função para inicializar o PWM no pino do buzzer
void buzzer_init(uint pin)
{

    // Configuração do GPIO para o buzzer como saída
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);

    // Inicializar o PWM no pino do buzzer
    pwm_init_buzzer(pin);
}

// Função para inicializar o PWM no pino do buzzer
void pwm_init_buzzer(uint pin)
{
    // Configura o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obtém o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configura o PWM com valores padrão
    pwm_config config = pwm_get_default_config();
    pwm_init(slice_num, &config, true);

    // Inicia o PWM com nível baixo (sem som)
    pwm_set_gpio_level(pin, 0);
}

void beep(uint pin, uint frequency) {
    // Obtém o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configura o PWM com a frequência desejada
    pwm_set_clkdiv(slice_num, clock_get_hz(clk_sys) / (frequency * 4096));
    pwm_set_wrap(slice_num, 4095);

    // Define o duty cycle para 50% (ativo)
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(pin), 2048);

    // Habilita o PWM
    pwm_set_enabled(slice_num, true);
}

