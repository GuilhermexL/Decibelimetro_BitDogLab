#include <stdio.h>
#include <math.h>
#include "mic.h"

// Configuração do DMA
static dma_channel_config dma_cfg;

/**
 * Inicializa o módulo de microfone, configurando ADC e DMA.
 */
uint mic_init() {
    printf("Preparando ADC...\n");

    // Inicializa o pino do microfone como entrada analógica
    adc_gpio_init(MIC_PIN);
    adc_init();
    adc_select_input(MIC_CHANNEL);

    adc_fifo_setup(
        true,   // Habilitar FIFO
        true,   // Habilitar request de dados do DMA
        1,      // Threshold para ativar request DMA é 1 leitura do ADC
        false,  // Não usar bit de erro
        false   // Não fazer downscale das amostras para 8-bits, manter 12-bits.
    );

    adc_set_clkdiv(ADC_CLOCK_DIV);

    printf("ADC Configurado!\n\n");

    printf("Preparando DMA...\n");

    // Tomando posse de canal do DMA.
    uint dma_channel = dma_claim_unused_channel(true);

    // Configurações do DMA.
    dma_cfg = dma_channel_get_default_config(dma_channel);

    // Tamanho da transferência é 16-bits (usamos uint16_t para armazenar valores do ADC)
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);

    // Desabilita incremento do ponteiro de leitura (lemos de um único registrador)
    channel_config_set_read_increment(&dma_cfg, false);

    // Habilita incremento do ponteiro de escrita (escrevemos em um array/buffer)
    channel_config_set_write_increment(&dma_cfg, true);
    
    // Usamos a requisição de dados do ADC
    channel_config_set_dreq(&dma_cfg, DREQ_ADC);

    printf("DMA configurado!\n");
    
    return dma_channel;
}

/**
 * Realiza as leituras do ADC e armazena os valores no buffer.
 */
void mic_sample(uint16_t* adc_buffer, uint dma_channel) {
    adc_fifo_drain(); // Limpa o FIFO do ADC.
    adc_run(false);   // Desliga o ADC (se estiver ligado) para configurar o DMA.

    dma_channel_configure(dma_channel, &dma_cfg,
        adc_buffer,     // Escreve no buffer.
        &(adc_hw->fifo), // Lê do ADC.
        SAMPLES,         // Faz "SAMPLES" amostras.
        true             // Liga o DMA.
    );

    // Liga o ADC e espera acabar a leitura.
    adc_run(true);
    dma_channel_wait_for_finish_blocking(dma_channel);
    
    // Acabou a leitura, desliga o ADC de novo.
    adc_run(false);
}

/**
 * Calcula a potência média das leituras do ADC. (Valor RMS)
 */
float mic_power(const uint16_t* adc_buffer) {
    float avg = 0.f;

    for (uint i = 0; i < SAMPLES; ++i)
        avg += adc_buffer[i] * adc_buffer[i];
    
    avg /= SAMPLES;
    return sqrt(avg);
}

/**
 * Calcula a intensidade do volume registrado no microfone, de 0 a 4, usando a tensão.
 */
uint8_t mic_get_intensity(float v) {
    uint count = 0;

    while ((v -= ADC_STEP/20) > 0.f)
        ++count;
    
    return count;
}

float calculate_sound_level(float voltage, float ref_voltage) {
    // Evita log de zero ou valores negativos
    if (voltage < 0.0001f) voltage = 0.0001f;
    if (ref_voltage < 0.0001f) ref_voltage = 0.0001f;
    
    // Calcula dB FS (Full Scale)
    float db = 20.0f * log10f(voltage / ref_voltage);
    
    // Ajuste empírico para aproximar de dB SPL (ajuste conforme seu hardware)
    return db + 94.0f - 30.0f; // 94dB SPL ref, -30dB de ajuste do mic
}