#include "ws2812.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include <string.h>

#define WS2812_PIN 7

uint32_t led_matrix[25] = {0}; // Buffer para armazenar o estado de cada LED (5x5)

// Buffer para padrões de desenhos
const uint32_t numbers[][25] = {
    // 0 - ºC
    1, 1, 1, 0, 0,
    0, 0, 1, 0, 0,
    1, 1, 1, 0, 0,
    1, 1, 0, 0, 0,
    0, 0, 0, 1, 1,

    // 1 - água
    0, 1, 1, 1, 0,
    1, 1, 1, 1, 1,
    1, 1, 1, 1, 1,
    0, 1, 1, 1, 0,
    0, 0, 1, 0, 0,
    
    // 2 - oxigênio
    0, 0, 0, 1, 1,
    1, 1, 0, 0, 0,
    0, 0, 1, 0, 0,
    0, 0, 0, 1, 1,
    1, 1, 0, 0, 0,
};

const uint32_t colors[][3] = {
    {0, 0, 0},      // Preto
    {5, 2, 1},    // Marrom
    {10, 0, 0},     // Vermelho
    {5, 1, 0},      // Laranja
    {10, 5, 0},     // Amarelo
    {0, 10, 0},     // Verde
    {0, 0, 10},     // Azul
    {5, 0, 10},     // Roxo
    {1, 1, 1},      // Cinza
    {5, 5, 5},      // Branco
    {10, 1, 3}      // Rosa
};

const char *color_names[] = {
    "preto", "marrom", "vermelho", "laranja", "amarelo",
    "verde", "azul", "roxo", "cinza", "branco", "rosa"
};


static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}


/**
 * @brief Transforma a cor RGB em um inteiro de 32 bits sem sinal.
 * 
 * @param r cor vermelha
 * @param g cor verde
 * @param b cor azul
 * 
 * @return Inteiro de 32 bits sem sinal.
 */
static inline uint32_t urgb_u32(char *color_name) {
    int8_t index = get_color_index(color_name);
    uint8_t r, g, b;
    r = colors[index][0];
    g = colors[index][1];
    b = colors[index][2];
    
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}


/**
 * @brief Atualiza um LED específico na matriz com base nas coordenadas (x, y).
 * 
 * @param x Coordenada X do LED (coluna).
 * @param y Coordenada Y do LED (linha).
 * @param color_name Nome da cor a ser aplicada ao LED.
 */
void set_led(uint8_t x, uint8_t y, char *color_name) {
    if (x >= 5 || y >= 5) {
        //printf("Coordenadas fora do limite da matriz (5x5).\n");
        return;
    }

    // Calcula o índice do LED na matriz (assumindo ordem linear de 0 a 24)
    uint8_t index = y * 5 + x;

    // Converte o nome da cor para o valor RGB e atualiza o buffer
    led_matrix[index] = urgb_u32(color_name);
}


/**
 * @brief Envia o estado atual da matriz (buffer) para os LEDs.
 * 
 * @param pio Instância do PIO utilizada.
 * @param sm Número da state machine.
 */
void update_matrix(PIO pio, uint sm) {
    for (int i = 0; i < 25; i++) {
        pio_sm_put_blocking(pio, sm, led_matrix[i] << 8u);
    }
    sleep_us(50);
}


/**
 * @brief Atualiza a matriz de LEDs com o número especificado
 * @param pattern Desenho a ser exibido
 * 
 * @details Envia os dados para a matriz LED WS2812 usando PIO
 */
void set_pattern(PIO pio, uint sm, uint8_t pattern, char *color_name) {
    uint32_t color = urgb_u32(color_name);

    for (int i = 0; i < 25; i++) {
        if (numbers[pattern][i]) {
            pio_sm_put_blocking(pio, sm, color<<8u); // Liga o LED com 1 no buffer
        } else {
            pio_sm_put_blocking(pio, sm, 0); // Desliga os LEDs com 0 no buffer
        }
    }
    sleep_us(50);
}


/**
 * @brief Encontra o índice da cor correspondente ao nome fornecido.
 * @param color Cor a ser exibida
 * @returns O índice da cor correspondente
 * @details Retorna -1 se a cor não for encontrada
 */
int get_color_index(char *color) {
    for (int i = 0; i < sizeof(color_names) / sizeof(color_names[0]); i++) {
        if (strcmp(color, color_names[i]) == 0) {
            return i; // Retorna o índice correspondente
        }
    }
    return -1; // Cor não encontrada
}


/**
 * @brief Apaga todos os LEDs da matriz.
 * 
 * @param pio Instância do PIO utilizada.
 * @param sm Número da state machine.
 */
void clear_matrix(PIO pio, uint sm) {
    for (int i = 0; i < 25; i++) {
        pio_sm_put_blocking(pio, sm, 0);
    }
    sleep_us(50);
}