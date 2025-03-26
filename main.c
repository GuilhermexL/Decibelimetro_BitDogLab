#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "pico/bootrom.h"
#include "main.pio.h"
#include "./libs/matriz/frames.h"
#include "./libs/matriz/letters.h"
#include "./libs/matriz/led_functions.h"
#include "./libs/buzzer/buzzer_functions.h"
#include "./libs/mic/mic.h"
#include "./libs/display/ssd1306.h"
#include "./libs/display/display.h"
#include "hardware/i2c.h"

// Configurações
#define SYS_CLOCK_KHZ 128000
#define DEBOUNCE_TIME_MS 400
#define BUTTONA_PIN 5
#define BUTTONB_PIN 6
#define OUT_PIN 7
#define BUZZER_PIN 21
#define MEASUREMENT_DURATION_MS 10000

// Constantes
#define ADC_REF_VOLTAGE 3.3f
#define ADC_RESOLUTION (1 << 12)
#define REF_PRESSURE 0.00002f
#define SILENCE_THRESHOLD 0.01f
#define CALIBRATION_SAMPLES 50

// Estados do sistema
typedef enum
{
    STATE_IDLE,
    STATE_MEASURING,
    STATE_CALIBRATING
} system_state_t;

// Estrutura para medição
typedef struct
{
    float voltage_history[5];
    uint history_index;
    float last_db;
    float max_db;
} measurement_t;

measurement_t measurement;

// Variáveis globais
float baseline_noise = 0.0f;
PIO pio;
uint sm, offset;
system_state_t system_state = STATE_CALIBRATING;
absolute_time_t measurement_end_time;
uint dma_channel;
uint16_t adc_buffer[SAMPLES];
bool buzzer_active = false;

// Cores
const RGBColor RED = {1.0, 0.0, 0.0};
const RGBColor GREEN = {0.0, 1.0, 0.0};
const RGBColor BLUE = {0.0, 0.0, 1.0};

// Protótipos
bool matrix_init(PIO *pio, uint *sm, uint *offset);
void init_hardware();
void start_measurement();
void update_measurement();
void complete_measurement();
void button_callback(uint gpio, uint32_t events);
void calibrate_with_feedback(uint pin);

/**
 * @brief Inicializa a matriz de LEDs usando PIO (Programmable I/O)
 * @param pio Ponteiro para a instância PIO a ser utilizada
 * @param sm Ponteiro para a máquina de estados a ser configurada
 * @param offset Ponteiro para armazenar o offset do programa PIO carregado
 * @return true se a inicialização foi bem-sucedida, false caso contrário
 * 
 * Configura o clock do sistema, carrega o programa PIO na memória,
 * aloca uma máquina de estados e inicializa o programa na máquina de estados.
 */
bool matrix_init(PIO *pio, uint *sm, uint *offset)
{
    if (!set_sys_clock_khz(SYS_CLOCK_KHZ, false))
        return false;
    *pio = pio0;
    *offset = pio_add_program(*pio, &main_program);
    *sm = pio_claim_unused_sm(*pio, true);
    if (*offset == -1 || *sm == -1)
        return false;
    main_program_init(*pio, *sm, *offset, OUT_PIN);
    return true;
}

/**
 * @brief Inicializa todo o hardware do sistema
 * 
 * Configura a matriz de LEDs, os botões A e B com interrupções,
 * e o buzzer. Se houver falha na inicialização da matriz LED,
 * entra em loop infinito.
 */
void init_hardware()
{
    if (!matrix_init(&pio, &sm, &offset))
    {
        printf("Erro ao inicializar matriz LED!\n");
        while (1)
            tight_loop_contents();
    }

    // Configuração do botão A
    gpio_init(BUTTONA_PIN);
    gpio_set_dir(BUTTONA_PIN, GPIO_IN);
    gpio_pull_up(BUTTONA_PIN);
    gpio_set_irq_enabled_with_callback(BUTTONA_PIN, GPIO_IRQ_EDGE_FALL, true, &button_callback);

    // Configuração do botão B
    gpio_init(BUTTONB_PIN);
    gpio_set_dir(BUTTONB_PIN, GPIO_IN);
    gpio_pull_up(BUTTONB_PIN);
    gpio_set_irq_enabled_with_callback(BUTTONB_PIN, GPIO_IRQ_EDGE_FALL, true, &button_callback);

    // Inicialização do buzzer
    buzzer_init(BUZZER_PIN);
}

/**
 * @brief Inicia uma nova medição de decibéis
 * 
 * Configura o sistema para o estado de medição, define o tempo final
 * da medição (10 segundos), zera o valor máximo anterior e
 * inicializa a matriz de LEDs.
 */
void start_measurement(){
    printf("Iniciando medicao por 10 segundos...\n");
    system_state = STATE_MEASURING;
    measurement_end_time = make_timeout_time_ms(MEASUREMENT_DURATION_MS);
    measurement.max_db = 0;

    // Inicializa matriz de LEDs (apaga todos os LEDs)
    led_matrix_update(0, 0, pio, sm);
}

/**
 * @brief Atualiza a medição atual de decibéis
 * 
 * Realiza uma nova amostra do microfone, aplica filtro de média móvel,
 * calcula o valor em dB SPL e atualiza a matriz de LEDs e a saída serial.
 */
void update_measurement(){
    // Obtém nova amostra do microfone via DMA
    mic_sample(adc_buffer, dma_channel);
    float avg_power = mic_power(adc_buffer);
    float voltage_rms = 2.f * abs(ADC_ADJUST(avg_power));

    // Aplica filtro de média móvel (5 amostras)
    measurement.voltage_history[measurement.history_index] = voltage_rms;
    measurement.history_index = (measurement.history_index + 1) % 5;

    float filtered_voltage = 0.0f;
    for (int i = 0; i < 5; i++)
    {
        filtered_voltage += measurement.voltage_history[i];
    }
    filtered_voltage /= 5;

    // Subtrai o ruído de fundo e aplica limiar de silêncio
    float adjusted_voltage = fmaxf(0.0f, filtered_voltage - baseline_noise);
    if (adjusted_voltage < SILENCE_THRESHOLD)
        adjusted_voltage = 0.0f;

    // Calcula o valor em decibéis (dB SPL)
    measurement.last_db = 0.0f;
    if (adjusted_voltage > 0)
    {
        float pressure_pa = (adjusted_voltage / ADC_REF_VOLTAGE) * 1.0f;
        measurement.last_db = 20.0f * log10f(pressure_pa / REF_PRESSURE);
        measurement.last_db = fmaxf(30.0f, fminf(measurement.last_db, 120.0f));
        if (measurement.last_db > measurement.max_db)
        {
            measurement.max_db = measurement.last_db;
        }
    }

    // Atualiza a saída visual e serial
    uint intensity = mic_get_intensity(adjusted_voltage);
    led_matrix_update(intensity, adjusted_voltage, pio, sm);
    printf("Tensao: %.4f V, dB SPL: %.1f \n",
           adjusted_voltage, measurement.last_db);
}

/**
 * @brief Finaliza a medição atual
 * 
 * Exibe o resultado máximo no display e serial, retorna ao estado
 * ocioso e desliga a matriz de LEDs.
 */
void complete_measurement(){
    printf("Medicao concluida. dB maximo: %.1f\n", measurement.max_db);
    system_state = STATE_IDLE;

    // Formata e exibe a mensagem no display
    char max_message[32];
    snprintf(max_message, sizeof(max_message), "decibeis: %.1f", measurement.max_db);

    char *messages[] = {
        "Teste Concluido",
        "               ",
        max_message,
        "               ",
        "Aperte A",
        "Para Reiniciar"};

    print_text_display(messages, sizeof(messages) / sizeof(messages[0]));

    // Desliga a matriz de LEDs
    led_matrix_update(0, 0, pio, sm);
}

/**
 * @brief Callback de interrupção para os botões
 * @param gpio GPIO que gerou a interrupção
 * @param events Tipo de evento que acionou a interrupção
 * 
 * Trata os eventos de pressionamento dos botões A e B com debounce.
 * Botão A inicia nova medição quando no estado ocioso.
 * Botão B alterna o estado do buzzer (liga/desliga).
 */
void button_callback(uint gpio, uint32_t events){
    static absolute_time_t last_time = {0};
    absolute_time_t now = get_absolute_time();

    // Verifica debounce (evita múltiplas ativações rápidas)
    if (absolute_time_diff_us(last_time, now) / 1000 < DEBOUNCE_TIME_MS)
        return;
    last_time = now;

    // Trata botão A (inicia medição)
    if ((gpio == BUTTONA_PIN) && (events & GPIO_IRQ_EDGE_FALL) && system_state == STATE_IDLE)
    {
        start_measurement();
    }

    // Trata botão B (toggle buzzer)
    if ((gpio == BUTTONB_PIN) && (events & GPIO_IRQ_EDGE_FALL))
    {
        if (buzzer_active) {
            // Se o buzzer já está tocando, desliga
            beepOff(BUZZER_PIN);
            buzzer_active = false;
        } else {
            // Se o buzzer está desligado, toca
            beep(BUZZER_PIN, 500);
            buzzer_active = true;
        }
    }
}

// Função para calibração com feedback sonoro
void calibrate_with_feedback(uint pin)
{
    printf("Calibrando ruido de fundo...\n");

    // Primeiro beep - início da calibração
    beep(pin, 784); // G5 (mais agudo)
    sleep_ms(300);
    beepOff(pin);

    dma_channel = mic_init();
    float sum = 0.0f;

    // Beep progressivo durante a calibração
    for (int i = 0; i < CALIBRATION_SAMPLES; i++)
    {
        mic_sample(adc_buffer, dma_channel);
        float avg_power = mic_power(adc_buffer);
        float voltage_rms = 2.f * abs(ADC_ADJUST(avg_power));
        sum += voltage_rms;

        // Feedback sonoro a cada 10 amostras
        if (i % 10 == 0)
        {
            beep(pin, 523 + (i * 2)); // C5 aumentando gradualmente
            sleep_ms(50);
            beepOff(pin);
        }

        sleep_ms(50); // Reduzi o tempo entre amostras para acelerar
    }

    baseline_noise = sum / CALIBRATION_SAMPLES;
    printf("Ruido de fundo calibrado: %.4f V\n", baseline_noise);

    // Beep de conclusão (3 tons ascendentes)
    for (int freq = 523; freq <= 659; freq += 68)
    { // C5, E5, G5
        beep(pin, freq);
        sleep_ms(150);
        beepOff(pin);
        sleep_ms(50);
    }
}

int main()
{
    sleep_ms(2000);
    stdio_init_all();
    init_hardware();
    display_init();

    printf("Decibelimetro Inicializado\n");

    // Calibração com feedback sonoro
    calibrate_with_feedback(BUZZER_PIN);

    system_state = STATE_IDLE;

    while (true)
    {
        switch (system_state)
        {
        case STATE_MEASURING:
            update_measurement();
            if (absolute_time_diff_us(get_absolute_time(), measurement_end_time) <= 0)
            {
                complete_measurement();
            }
            break;

        case STATE_IDLE:
        case STATE_CALIBRATING:
        default:
            tight_loop_contents();
            break;
        }
        sleep_ms(100);
    }

    return 0;
}