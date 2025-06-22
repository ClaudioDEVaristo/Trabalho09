#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"
#include "ssd1306.h"
#include "lwipopts.h"
#include <string.h>

#define botao_a 5
#define botao_b 6
#define green_led 11
#define interrupcao(bot) gpio_set_irq_enabled_with_callback(bot, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &gpio_irq_handler)

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C

typedef struct {
    uint8_t min;
    uint8_t max;
    volatile bool estado_bomba;
    uint8_t nivel_atual;
    absolute_time_t alarm_a;
} nivel_agua;

nivel_agua nv = {20, 80, false, 30, 0};

void init_bot(void); // Inicialização dos botões
void gpio_irq_handler(uint gpio, uint32_t events);  // Tratamento de interrupção
int64_t botao_pressionado(alarm_id_t, void *user_data); // Ativo da função após os 2 segundos
uint8_t xcenter_pos(char *text); // Função para centralizar o texto no display

int main(){
    stdio_init_all();
    init_bot();
    interrupcao(botao_a);
    interrupcao(botao_b);

    gpio_init(green_led);
    gpio_set_dir(green_led, GPIO_OUT);

    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);

    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "Iniciando Wi-Fi", 0, 0);
    char aguarde[] = "Aguarde...";
    ssd1306_draw_string(&ssd, aguarde, xcenter_pos(aguarde), 30);    
    ssd1306_send_data(&ssd);

    if (cyw43_arch_init())
    {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => FALHA", xcenter_pos("WiFi => FALHA"), 0);
        ssd1306_send_data(&ssd);
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000))
    {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => ERRO", 0, 0);
        ssd1306_send_data(&ssd);
        return 1;
    }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "WiFi => OK", 0, 0);
    ssd1306_draw_string(&ssd, ip_str, 0, 10);
    ssd1306_send_data(&ssd);

    char str_x[14];
    char str_y[14];
    char str_pb[14];
    bool cor = true;

    char ip_text[] = "IP:";
    strcat(ip_text, ip_str);

    while (true) {
        sprintf(str_x, "Nivel Min: %d", nv.min);    
        sprintf(str_y, "Nivel Max: %d", nv.max);    
        sprintf(str_pb, "Agua: %d", nv.nivel_atual);

        ssd1306_fill(&ssd, !cor);               
        ssd1306_rect(&ssd, 0, 0, 127, 63, cor, !cor);
        ssd1306_draw_string(&ssd, str_pb, 2, 2);
        ssd1306_draw_string(&ssd, str_x, 2, 10);
        ssd1306_draw_string(&ssd, str_y, 2, 18);

        if (nv.nivel_atual >= nv.max) {
            ssd1306_draw_string(&ssd, "Bomba: OFF", 2, 26);
            nv.estado_bomba = false;
            printf("Bomba desligada, nivel maximo atingido.\n");
        } else if (nv.nivel_atual < nv.min) {
            ssd1306_draw_string(&ssd, "Bomba: ON", 2, 26);
            printf("Bomba ligada, nivel minimo atingido.\n");
            nv.estado_bomba = true;
        } else {
            //if (nv.estado_bomba) continue; // Se a bomba já estiver ligada, não faz nada
            ssd1306_draw_string(&ssd, "Bomba: OFF", 2, 26);
            printf("Bomba desligada, nivel dentro dos limites.\n");
            nv.estado_bomba = false;
        }

        gpio_put(green_led, nv.estado_bomba); // Liga/desliga o LED verde conforme o estado da bomba

        ssd1306_hline(&ssd, 0, 127, 40, cor);

        ssd1306_draw_string(&ssd, ip_text, xcenter_pos(ip_text), 42);

        ssd1306_draw_string(&ssd, "EmbarcaTech", xcenter_pos("EmbarcaTech"), 50); // Desenha o IP

        ssd1306_send_data(&ssd); 

        sleep_ms(1000);
    }

    cyw43_arch_deinit();
    return 0;
}

void init_bot(void){
    for(uint8_t i = 5; i < 7; i++){
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_pull_up(i);
    }
}
/* Não houve necessidade de utilizar o debounce no botão A, pois o mesmo é tratado com um alarme de 2 segundos para a sua real ativação.
   O botão B foi tratado com um debounce de 300ms.
   Ao ser pressionado, o botão A ativa o alarme e se soltar antes dos 2 segundos, o alarme é desativado.
   **** Botão A: Reseta os valores limítrofes da bomba. (Esses valores podem ser configurados)
   **** Botao B: Ativa e desativa a bomba manualmente. (É só associar a ativação com a booleana)
   E por fim, os printf são só para verificação de funcionamento... pode remover ao finalizar.  */
void gpio_irq_handler(uint gpio, uint32_t events){
    uint64_t current_time = to_ms_since_boot(get_absolute_time());
    static uint64_t last_time = 0;
    static volatile bool estado_a = false;
        if (gpio == botao_a && !estado_a) {
            nv.alarm_a = add_alarm_in_ms(2000, botao_pressionado, NULL, false);
            estado_a = true;
            } else if (gpio == botao_a && estado_a && (events & GPIO_IRQ_EDGE_RISE)) {
                cancel_alarm(nv.alarm_a);
                estado_a = false;
            } else if (gpio == botao_b && (current_time - last_time > 300)) {
                nv.estado_bomba = !nv.estado_bomba;
                gpio_put(green_led, nv.estado_bomba);
                last_time = current_time;
                printf("Botão B pressionado \n");
            }
}

int64_t botao_pressionado(alarm_id_t, void *user_data) {
    if(gpio_get(botao_a) == 0){
        nv.min = 20;
        nv.max = 80;
        printf("Botão A pressionado por 2 segundos!\n");
    }
    return 0;
}

uint8_t xcenter_pos(char* text) {
    return (WIDTH - 8 * strlen(text)) / 2; // Calcula a posição centralizada
}