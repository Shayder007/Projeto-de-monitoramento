#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "neopixel.c"

// Pino e canal do microfone no ADC.
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL)

// Parâmetros e macros do ADC.
#define ADC_CLOCK_DIV 96.f
#define SAMPLES 200 // Número de amostras que serão feitas do ADC.
#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f) // Ajuste do valor do ADC para Volts.
#define ADC_MAX 3.3f
#define ADC_STEP (3.3f/5.f) // Intervalos de volume do microfone.

// Pino e número de LEDs da matriz de LEDs.
#define LED_PIN 7
#define LED_COUNT 25

#define abs(x) ((x < 0) ? (-x) : (x))

// Canal e configurações do DMA
uint dma_channel;
dma_channel_config dma_cfg;

// Buffer de amostras do ADC.
uint16_t adc_buffer[SAMPLES];

void sample_mic();
float mic_power();
uint8_t get_intensity(float v);

int main() {
  stdio_init_all();

  // Delay para o usuário abrir o monitor serial...
  sleep_ms(5000);

  // Preparação da matriz de LEDs.
  printf("Preparando NeoPixel...");
  
  npInit(LED_PIN, LED_COUNT);

  // Preparação do ADC.
  printf("Preparando ADC...\n");

  adc_gpio_init(MIC_PIN);
  adc_init();
  adc_select_input(MIC_CHANNEL);

  adc_fifo_setup(
    true, // Habilitar FIFO
    true, // Habilitar request de dados do DMA
    1, // Threshold para ativar request DMA é 1 leitura do ADC
    false, // Não usar bit de erro
    false // Não fazer downscale das amostras para 8-bits, manter 12-bits.
  );

  adc_set_clkdiv(ADC_CLOCK_DIV);

  printf("ADC Configurado!\n\n");

  printf("Preparando DMA...");

  // Tomando posse de canal do DMA.
  dma_channel = dma_claim_unused_channel(true);

  // Configurações do DMA.
  dma_cfg = dma_channel_get_default_config(dma_channel);

  channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16); // Tamanho da transferência é 16-bits (usamos uint16_t para armazenar valores do ADC)
  channel_config_set_read_increment(&dma_cfg, false); // Desabilita incremento do ponteiro de leitura (lemos de um único registrador)
  channel_config_set_write_increment(&dma_cfg, true); // Habilita incremento do ponteiro de escrita (escrevemos em um array/buffer)
  
  channel_config_set_dreq(&dma_cfg, DREQ_ADC); // Usamos a requisição de dados do ADC

  // Amostragem de teste.
  printf("Amostragem de teste...\n");
  sample_mic();


  printf("Configuracoes completas!\n");

  printf("\n----\nIniciando loop...\n----\n");
  while (true) {

    // Realiza uma amostragem do microfone.
    sample_mic();

    // Pega a potência média da amostragem do microfone.
    float avg = mic_power();
    avg = 2.f * abs(ADC_ADJUST(avg)); // Ajusta para intervalo de 0 a 3.3V. (apenas magnitude, sem sinal)

    uint intensity = get_intensity(avg); // Calcula intensidade a ser mostrada na matriz de LEDs.

    // Limpa a matriz de LEDs.
    npClear();

    // A depender da intensidade do som, acende LEDs específicos.
    switch (intensity) {
      case 0:
          // Nenhum LED aceso.
          break;
  
      case 1:
          // Base da coluna central - Cor inicial azul/ciano
          npSetLED(0, 0, 255, 255); 
          npSetLED(1, 0, 255, 255);
          npSetLED(2, 0, 255, 255);
          npSetLED(3, 0, 255, 255);
          npSetLED(4, 0, 255, 255);  
          break;
  
      case 2:
          // Base + um nível acima com transição suave
          for (int i = 0; i < 5; i++) {
              npSetLED(i, 0, 255, 255);  // Azul/ciano na base
          }
          for (int i = 5; i < 10; i++) {
              int r = map(i, 5, 9, 0, 255);  // Degradê vermelho
              npSetLED(i, r, 255 - r, 0);   // Transição de vermelho para laranja
          }
          break;
  
      case 3:
          // Base + mais um nível mais alto + transição
          for (int i = 0; i < 5; i++) {
              npSetLED(i, 0, 255, 255);  // Azul/ciano
          }
          for (int i = 5; i < 10; i++) {
              int r = map(i, 5, 9, 0, 255);  // Degradê para laranja
              npSetLED(i, r, 255 - r, 0);
          }
          for (int i = 10; i < 15; i++) {
              int r = map(i, 10, 14, 255, 255);  // Amarelo
              npSetLED(i, r, 255, 0);
          }
          break;
  
      case 4:
          // Todos os LEDs até o topo com gradiente suave de azul para vermelho
          for (int i = 0; i < 5; i++) {
              npSetLED(i, 0, 255, 255);  // Azul/ciano
          }
          for (int i = 5; i < 10; i++) {
              int r = map(i, 5, 9, 0, 255);  // Degradê de ciano para vermelho
              npSetLED(i, r, 255 - r, 0);
          }
          for (int i = 10; i < 15; i++) {
              int r = map(i, 10, 14, 255, 255);  // Amarelo
              npSetLED(i, r, 255, 0);
          }
          for (int i = 15; i < 20; i++) {
              int r = map(i, 15, 19, 255, 0);  // Degradê de amarelo para laranja
              npSetLED(i, r, 100, 0); // Laranja
          }
          break;
  
      case 5:
          // Matriz inteira com efeito de pico máximo (gradiente intenso)
          for (int i = 0; i < 5; i++) {
              npSetLED(i, 0, 255, 255);  // Azul/ciano
          }
          for (int i = 5; i < 10; i++) {
              int r = map(i, 5, 9, 0, 255);  // Degradê de azul para vermelho
              npSetLED(i, r, 255 - r, 0);
          }
          for (int i = 10; i < 15; i++) {
              int r = map(i, 10, 14, 255, 255);  // Amarelo
              npSetLED(i, r, 255, 0);
          }
          for (int i = 15; i < 20; i++) {
              int r = map(i, 15, 19, 255, 0);  // Degradê de amarelo para laranja
              npSetLED(i, r, 100, 0);
          }
          for (int i = 20; i < 25; i++) {
              int r = map(i, 20, 24, 255, 0);  // Degradê de laranja para vermelho
              npSetLED(i, r, 0, 0);
          }
          break;
  }
    // Atualiza a matriz.
    npWrite();

    // Envia a intensidade e a média das leituras do ADC por serial.
    printf("%2d %8.4f\r", intensity, avg);
  }
}

/**
 * Realiza as leituras do ADC e armazena os valores no buffer.
 */
void sample_mic() {
  adc_fifo_drain(); // Limpa o FIFO do ADC.
  adc_run(false); // Desliga o ADC (se estiver ligado) para configurar o DMA.

  dma_channel_configure(dma_channel, &dma_cfg,
    adc_buffer, // Escreve no buffer.
    &(adc_hw->fifo), // Lê do ADC.
    SAMPLES, // Faz SAMPLES amostras.
    true // Liga o DMA.
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
float mic_power() {
  float avg = 0.f;

  for (uint i = 0; i < SAMPLES; ++i)
    avg += adc_buffer[i] * adc_buffer[i];
  
  avg /= SAMPLES;
  return sqrt(avg);
}

/**
 * Calcula a intensidade do volume registrado no microfone, de 0 a 4, usando a tensão.
 */
uint8_t get_intensity(float v) {
  uint count = 0;

  while ((v -= ADC_STEP/20) > 0.f)
    ++count;
  
  return count;
}