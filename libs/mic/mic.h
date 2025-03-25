#ifndef MIC_H
#define MIC_H

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

// Pino e canal do microfone no ADC.
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL)

// Parâmetros e macros do ADC.
#define ADC_CLOCK_DIV 96.f
#define SAMPLES 1024 // Número de amostras que serão feitas do ADC.
#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f) // Ajuste do valor do ADC para Volts.
#define ADC_MAX 3.3f
#define ADC_STEP (3.3f/5.f) // Intervalos de volume do microfone.

#define abs(x) ((x < 0) ? (-x) : (x))

/**
 * Inicializa o módulo de microfone, configurando ADC e DMA.
 * @return Canal DMA utilizado para leitura do microfone
 */
uint mic_init();

/**
 * Realiza as leituras do ADC e armazena os valores no buffer.
 * @param adc_buffer Buffer para armazenar as amostras
 * @param dma_channel Canal DMA a ser utilizado
 */
void mic_sample(uint16_t* adc_buffer, uint dma_channel);

/**
 * Calcula a potência média das leituras do ADC. (Valor RMS)
 * @param adc_buffer Buffer com as amostras do ADC
 * @return Valor RMS das amostras
 */
float mic_power(const uint16_t* adc_buffer);

/**
 * Calcula a intensidade do volume registrado no microfone, de 0 a 4, usando a tensão.
 * @param v Tensão medida
 * @return Valor de intensidade de 0 a 4
 */
uint8_t mic_get_intensity(float v);

float calculate_sound_level(float voltage, float ref_voltage);


#endif // MIC_H