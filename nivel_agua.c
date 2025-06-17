#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"

#define botao_a 5
#define botao_b 6
#define green_led 11
#define interrupcao(bot) gpio_set_irq_enabled_with_callback(bot, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &gpio_irq_handler)

typedef struct {
    uint8_t min;
    uint8_t max;
    volatile bool estado_bomba;
    absolute_time_t alarm_a;
} nivel_agua;

nivel_agua nv = {20, 80, false};

void init_bot(void); // Inicialização dos botões
void gpio_irq_handler(uint gpio, uint32_t events);  // Tratamento de interrupção
int64_t botao_pressionado(alarm_id_t, void *user_data); // Ativo da função após os 2 segundos

int main(){
    stdio_init_all();
    init_bot();
    interrupcao(botao_a);
    interrupcao(botao_b);
    while (true) {
        sleep_ms(1000);
    }
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