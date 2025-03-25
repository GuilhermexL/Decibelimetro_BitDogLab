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
typedef enum {
    STATE_IDLE,
    STATE_MEASURING,
    STATE_CALIBRATING
} system_state_t;

// Variáveis globais
float baseline_noise = 0.0f;
PIO pio;
uint sm, offset;
system_state_t system_state = STATE_CALIBRATING;
absolute_time_t measurement_end_time;
uint dma_channel;
uint16_t adc_buffer[SAMPLES];

// Estrutura para medição
typedef struct {
    float voltage_history[5];
    uint history_index;
    float last_db;
    float max_db;
} measurement_t;

measurement_t measurement;

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

bool matrix_init(PIO *pio, uint *sm, uint *offset) {
    if (!set_sys_clock_khz(SYS_CLOCK_KHZ, false)) return false;
    *pio = pio0;
    *offset = pio_add_program(*pio, &main_program);
    *sm = pio_claim_unused_sm(*pio, true);
    if (*offset == -1 || *sm == -1) return false;
    main_program_init(*pio, *sm, *offset, OUT_PIN);
    return true;
}

void init_hardware() {
    if (!matrix_init(&pio, &sm, &offset)) {
        printf("Erro ao inicializar matriz LED!\n");
        while (1) tight_loop_contents();
    }

    gpio_init(BUTTONA_PIN);
    gpio_set_dir(BUTTONA_PIN, GPIO_IN);
    gpio_pull_up(BUTTONA_PIN);
    gpio_set_irq_enabled_with_callback(BUTTONA_PIN, GPIO_IRQ_EDGE_FALL, true, &button_callback);

    gpio_init(BUTTONB_PIN);
    gpio_set_dir(BUTTONB_PIN, GPIO_IN);
    gpio_pull_up(BUTTONB_PIN);

    buzzer_init(BUZZER_PIN);
}

void start_measurement() {
    printf("Iniciando medicao por 10 segundos...\n");
    //beep(BUZZER_PIN, 392);
    system_state = STATE_MEASURING;
    measurement_end_time = make_timeout_time_ms(MEASUREMENT_DURATION_MS);
    measurement.max_db = 0;
    
    // Inicializa matriz de LEDs
    led_matrix_update(0, 0, pio, sm);
}

void update_measurement() {
    mic_sample(adc_buffer, dma_channel);
    float avg_power = mic_power(adc_buffer);
    float voltage_rms = 2.f * abs(ADC_ADJUST(avg_power));

    // Filtro de média móvel
    measurement.voltage_history[measurement.history_index] = voltage_rms;
    measurement.history_index = (measurement.history_index + 1) % 5;
    
    float filtered_voltage = 0.0f;
    for (int i = 0; i < 5; i++) {
        filtered_voltage += measurement.voltage_history[i];
    }
    filtered_voltage /= 5;

    float adjusted_voltage = fmaxf(0.0f, filtered_voltage - baseline_noise);
    if (adjusted_voltage < SILENCE_THRESHOLD) adjusted_voltage = 0.0f;

    // Calcula dB
    measurement.last_db = 0.0f;
    if (adjusted_voltage > 0) {
        float pressure_pa = (adjusted_voltage / ADC_REF_VOLTAGE) * 1.0f;
        measurement.last_db = 20.0f * log10f(pressure_pa / REF_PRESSURE);
        measurement.last_db = fmaxf(30.0f, fminf(measurement.last_db, 120.0f));
        if (measurement.last_db > measurement.max_db) {
            measurement.max_db = measurement.last_db;
        }
    }

    // Atualiza LEDs e mostra no serial
    uint intensity = mic_get_intensity(adjusted_voltage);
    led_matrix_update(intensity, adjusted_voltage, pio, sm);
    printf("Tensao: %.4f V, dB SPL: %.1f \n", 
           adjusted_voltage, measurement.last_db);
}

void complete_measurement() {
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
    
    print_text_display(messages, sizeof(messages)/sizeof(messages[0]));
    
    led_matrix_update(0, 0, pio, sm);
}

void button_callback(uint gpio, uint32_t events) {
    static absolute_time_t last_time = {0};
    absolute_time_t now = get_absolute_time();

    if (absolute_time_diff_us(last_time, now) / 1000 < DEBOUNCE_TIME_MS) return;
    last_time = now;

    if ((gpio == BUTTONA_PIN) && (events & GPIO_IRQ_EDGE_FALL) && system_state == STATE_IDLE) {
        start_measurement();
    }
}

int main() {
    stdio_init_all();
    init_hardware();
    sleep_ms(2000);
	display_init();

    printf("Decibelimetro Inicializado\n");
    
    // Calibração inicial
    printf("Calibrando ruido de fundo...\n");
    dma_channel = mic_init();
    
    float sum = 0.0f;
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        mic_sample(adc_buffer, dma_channel);
        float avg_power = mic_power(adc_buffer);
        float voltage_rms = 2.f * abs(ADC_ADJUST(avg_power));
        sum += voltage_rms;
        sleep_ms(100);
    }
    baseline_noise = sum / CALIBRATION_SAMPLES;
    printf("Ruido de fundo calibrado: %.4f V\n", baseline_noise);
    
    system_state = STATE_IDLE;

    while (true) {
        switch (system_state) {
            case STATE_MEASURING:
                update_measurement();
                if (absolute_time_diff_us(get_absolute_time(), measurement_end_time) <= 0) {
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