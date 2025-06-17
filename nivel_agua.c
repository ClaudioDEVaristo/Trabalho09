#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "font.h"
#include "ws2812.pio.h"

#define botao_a 5
#define botao_b 6
#define matriz_led 7
#define green_led 11
#define i2c_sda 14
#define i2c_scl 15
#define I2C_PORT i2c1
#define endereco 0x3C
#define buzzer_a 21
#define matriz_led_pins 25
uint8_t slice = 0;

typedef struct pixeis {
    uint8_t cor1, cor2, cor3;
} pixeis;

pixeis leds [matriz_led_pins];    

PIO pio;                 
uint sm;
ssd1306_t ssd;                

void init_led(void);
void init_bot(void);
void pwm_setup(void);
void pwm_on(void);
void pwm_off(void);
void matriz_init(uint pin);
void setled(const uint index, const uint8_t r, const uint8_t g, const uint8_t b);
void matriz(uint8_t r, uint8_t g, uint8_t b);
void display(void);
void init_i2c(void);
void init_oled(void);
void oled_display(ssd1306_t* ssd, bool cor);

int main(){
    stdio_init_all();
    init_led();
    init_bot();
    pwm_setup();
    pwm_off();
    matriz_init(matriz_led);
    init_i2c();

    while (true) {
        printf("testando\n");
        matriz(0, 1, 0);
        sleep_ms(1000);
        matriz(0 ,0 ,0);
        sleep_ms(1000);
    }
}

void init_led(void){
    gpio_init(green_led);
    gpio_set_dir(green_led, GPIO_OUT);
    gpio_put(green_led, 0);
}

void init_bot(void){
    for (uint8_t i = 5 ; i < 7; i++){
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_pull_up(i);
    }
}

void pwm_setup(void){
    gpio_set_function(buzzer_a, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(buzzer_a);
    pwm_set_clkdiv(slice, 32.0f);
    pwm_set_wrap(slice, 3900);
    pwm_set_gpio_level(buzzer_a, 3900 / 2);
    pwm_set_enabled(slice, false);
}

void pwm_on(void){
    gpio_set_function(buzzer_a, GPIO_FUNC_PWM);
    pwm_set_enabled(slice, true);
}

void pwm_off(void){
    pwm_set_enabled(slice, false);
    gpio_set_function(buzzer_a, GPIO_FUNC_SIO);
}

void matriz_init(uint pin){

    uint offset = pio_add_program(pio0, &ws2812_program);
    pio = pio0;
    
    sm = pio_claim_unused_sm(pio, false);
        if(sm < 0){
            pio = pio1;
            sm = pio_claim_unused_sm(pio, true);
        }
    
    ws2812_program_init(pio, sm, offset, pin, 800000.f);
    }

void setled(const uint index, const uint8_t r, const uint8_t g, const uint8_t b){
    leds[index].cor1 = g;
    leds[index].cor2 = r;
    leds[index].cor3 = b;
}

void matriz(uint8_t r, uint8_t g, uint8_t b){
    const uint8_t digit_leds[] = {24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    size_t count = sizeof(digit_leds)/sizeof (digit_leds[0]);
        for (size_t i = 0; i < count; ++i) {
            setled(digit_leds[i], r, g, b);
        }
        display();    
}

void display(void){
    for (uint i = 0; i < matriz_led_pins; ++i) {
        pio_sm_put_blocking(pio, sm, leds[i].cor1);
        pio_sm_put_blocking(pio, sm, leds[i].cor2);
        pio_sm_put_blocking(pio, sm, leds[i].cor3);
        }
    sleep_us(100); 
}

void init_i2c(void){
    i2c_init(I2C_PORT, 400*1000);
        for(uint8_t i = 14 ; i < 16; i++){
            gpio_set_function(i, GPIO_FUNC_I2C);
            gpio_pull_up(i);
        }
}

/*void init_oled(void){
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

void oled_display(ssd1306_t* ssd, bool cor) {
    ssd1306_fill(   ssd, !cor);
    ssd1306_rect(   ssd, 3,  3, 122, 60, cor, !cor);
    ssd1306_line(   ssd, 3, 15, 123, 15, cor);
    ssd1306_line(   ssd, 3, 37, 123, 37, cor);
    ssd1306_line(   ssd, 60, 37, 60, 60, cor);
    adc_define();
    a.pct_n = (a.nivel * 100) / 4095;
    a.pct_v = (a.volume * 100) / 4095;
    snprintf(a.buff_n, sizeof(a.buff_n), "%3u%%", a.pct_n);
    snprintf(a.buff_v, sizeof(a.buff_v), "%3u%%", a.pct_v);

    ssd1306_draw_string(ssd, "Nivel", 13, 41);
    ssd1306_draw_string(ssd, "Volume", 70, 41);
    ssd1306_draw_string(ssd, "Sen. Enchentes", 8,  6);
    if(a.pct_n >= 70 || a.pct_v >= 80)ssd1306_draw_string(ssd, "Estado:PERIGO" , 8, 25);
    else if(a.pct_n > 40 && a.pct_v > 60)ssd1306_draw_string(ssd, "Estado:ALERTA", 8, 25);
    else ssd1306_draw_string(ssd, "Estado:SEGURO" , 8, 25);  
    ssd1306_draw_string(ssd, a.buff_n, 16, 52);
    ssd1306_draw_string(ssd, a.buff_v, 79, 52);
    ssd1306_send_data(ssd);
 }*/
