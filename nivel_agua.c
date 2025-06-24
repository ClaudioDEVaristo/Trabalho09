#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"
#include "ssd1306.h"
#include "ws2812.h"
#include "buzzer.h"
#include "lwipopts.h"
#include <string.h>
#include "lwip/tcp.h"
#include <stdlib.h>
#include "html_body.h"
#include "hardware/adc.h"

#define botao_a 5
#define botao_b 6
#define green_led 11
#define interrupcao(bot) gpio_set_irq_enabled_with_callback(bot, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &gpio_irq_handler)

#define WIFI_SSID "Crau crau do mal"
#define WIFI_PASSWORD "23092003"

#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define rele 8
#define endereco 0x3C

#define valor_min 20
#define valor_max 80

#define POTENCIOMETRO_PIN 28
#define ADC_MAX_LIM 1000
#define ADC_MAX 800 // Valor máximo do ADC para o potenciômetro
#define ADC_MIN 120

typedef struct {
    uint8_t min;
    uint8_t max;
    volatile bool estado_bomba;
    uint8_t nivel_atual;
    absolute_time_t alarm_a;
} nivel_agua;

nivel_agua nv = {valor_min, valor_max, false, 30, 0};

struct http_state{
    char response[8192];
    size_t len;
    size_t sent;
};

void init_bot(void);
void gpio_irq_handler(uint gpio, uint32_t events);
int64_t botao_pressionado(alarm_id_t, void *user_data);
uint16_t media(uint8_t input);
uint8_t xcenter_pos(char *text);

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static void start_http_server(void);

int main(){
    stdio_init_all();
    init_bot();
    interrupcao(botao_a);
    interrupcao(botao_b);

    gpio_init(green_led);
    gpio_set_dir(green_led, GPIO_OUT);
    gpio_init(rele);
    gpio_set_dir(rele, GPIO_OUT);

    // Configura o buzzer com PWM
    buzzer_setup_pwm(BUZZER_PIN, 4000);

    PIO pio = pio0;
    uint sm = 0;
    ws2812_init(pio, sm);

    adc_init();
    adc_gpio_init(POTENCIOMETRO_PIN);

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

    if (cyw43_arch_init()){
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => FALHA", xcenter_pos("WiFi => FALHA"), 0);
        ssd1306_send_data(&ssd);
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000)){
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

    start_http_server();
    char str_x[14];
    char str_y[14];
    char str_pb[14];
    bool cor = true;

    char ip_text[] = "IP:";
    strcat(ip_text, ip_str);

    while (true) {
        cyw43_arch_poll();

        uint16_t adc_value = media(2);

        printf("ADC Value: %d\n", adc_value); // Para depuração, pode ser removido.

        if (adc_value >= ADC_MAX_LIM) {
            nv.nivel_atual = 100;
        } else {
            nv.nivel_atual = (uint8_t)(((adc_value - ADC_MIN) * 100) / (ADC_MAX));
        }

        // 160 ~ 250 min
        //  987 ~ 1076 max
        sprintf(str_x, "Nivel Min: %d%%", nv.min);    
        sprintf(str_y, "Nivel Max: %d%%", nv.max);    
        sprintf(str_pb, "Agua: %d%%", nv.nivel_atual);

        ssd1306_fill(&ssd, !cor);               
        ssd1306_rect(&ssd, 0, 0, 127, 63, cor, !cor);
        ssd1306_draw_string(&ssd, str_pb, 2, 2);
        ssd1306_draw_string(&ssd, str_x, 2, 10);
        ssd1306_draw_string(&ssd, str_y, 2, 18);

        if (nv.nivel_atual >= nv.max) {
            nv.estado_bomba = false;
            buzzer_play(BUZZER_PIN, 3, 1000, 2000);
            printf("Bomba desligada, nivel maximo atingido.\n");
        } else if (nv.nivel_atual < nv.min) {
            printf("Bomba ligada, nivel minimo atingido.\n");
            buzzer_play(BUZZER_PIN, 1, 700, 500);
            nv.estado_bomba = true;
        }

        if (nv.estado_bomba) {
            ssd1306_draw_string(&ssd, "Bomba: ON", 2, 26);
        } else {
            ssd1306_draw_string(&ssd, "Bomba: OFF", 2, 26);
        }

        gpio_put(green_led, nv.estado_bomba); // Liga/desliga o LED verde conforme o estado da bomba
        gpio_put(rele, !nv.estado_bomba); // Liga/desliga o relé conforme o estado da bomba

        ssd1306_hline(&ssd, 0, 127, 40, cor);
        ssd1306_draw_string(&ssd, "EmbarcaTech", xcenter_pos("EmbarcaTech"), 50); 
        ssd1306_send_data(&ssd); 
        set_pattern(pio0, 0, 0, "azul");
        sleep_ms(1000);
    }

    cyw43_arch_deinit();
    return 0;
}


/**
 * @brief Inicializa os botões A e B.
 */
void init_bot(void){
    for(uint8_t i = 5; i < 7; i++){
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_pull_up(i);
    }
}


/**
 * Manipulador de interrupção GPIO.
 *
 * Esta função é chamada quando ocorre uma interrupção em um dos pinos GPIO monitorados.
 *
 * Parâmetros:
 *   @param gpio   Número do pino GPIO que gerou a interrupção.
 *   @param events Máscara de eventos que indica o tipo de borda (subida/descida) que ocorreu.
 *
 * Funcionamento:
 * - Para o botão 'A' (botao_a):
 *   - Se o botão não estava pressionado (estado_a == false), agenda um alarme para 2 segundos e marca o estado como pressionado.
 *   - Se o botão estava pressionado e ocorreu uma borda de subida, cancela o alarme e marca o estado como não pressionado.
 * - Para o botão 'B' (botao_b):
 *   - Se passaram mais de 300 ms desde o último acionamento, alterna o estado da bomba (nv.estado_bomba) e atualiza o tempo do último acionamento.
 */
void gpio_irq_handler(uint gpio, uint32_t events) {
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
            }
}

/**
 * @brief Função chamada após 2 segundos de pressionamento do botão A.
 */
int64_t botao_pressionado(alarm_id_t, void *user_data) {
    if(gpio_get(botao_a) == 0){
        nv.estado_bomba = false;
        nv.min = valor_min;
        nv.max = valor_max;
    }
    return 0;
}

uint16_t media(uint8_t input){
    uint16_t media_adc = 0;
    for(uint8_t i = 0; i < 10; i++){
        adc_select_input(input);
        sleep_us(2);
        media_adc += adc_read();
    }
return media_adc / 10;
}

/**
 * @brief Calcula a posição centralizada do texto no display.
 * 
 * @param text Texto a ser centralizado.
 * @return Posição X centralizada.
 */
uint8_t xcenter_pos(char* text) {
    return (WIDTH - 8 * strlen(text)) / 2; // Calcula a posição centralizada
}

/**
 * @brief Função de callback chamada após o envio de dados HTTP via TCP.
 * 
 * Esta função é responsável por gerenciar o envio progressivo de dados HTTP.
 * Ela mantém o controle da quantidade de bytes enviados e, quando todos os dados
 * forem transmitidos, fecha a conexão TCP e libera os recursos alocados.
 * Se ainda houver dados a serem enviados, a função envia o próximo chunk
 * de até 1024 bytes.
 * 
 * @param arg Ponteiro para a estrutura http_state associada à conexão
 * @param tpcb Ponteiro para o bloco de controle de protocolo TCP
 * @param len Quantidade de bytes que foram enviados na última operação
 * 
 * @return ERR_OK Se o processamento foi concluído com sucesso
 * 
 * @note A função libera automaticamente a memória alocada para http_state
 *       quando a transmissão é concluída.
 */
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len){
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;

    if (hs->sent >= hs->len) {
        tcp_close(tpcb);
        free(hs);
        return ERR_OK;
    }

    size_t remaining = hs->len - hs->sent;
    size_t chunk = remaining > 1024 ? 1024 : remaining;
    err_t err = tcp_write(tpcb, hs->response + hs->sent, chunk, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) {
        tcp_output(tpcb);
    } else {
        printf("Erro ao enviar chunk restante: %d\n", err);
    }

    return ERR_OK;
}

/**
 * @brief Função de callback para receber dados HTTP através de uma conexão TCP.
 * 
 * Esta função é chamada quando um pacote TCP é recebido para uma conexão HTTP.
 * Processa os dados recebidos no buffer de pacote e realiza as operações
 * apropriadas com base no conteúdo da requisição HTTP.
 * 
 * @param arg Ponteiro para argumentos adicionais passados durante o registro do callback
 * @param tpcb Ponteiro para o bloco de controle de protocolo TCP
 * @param p Ponteiro para o buffer de pacote recebido (NULL se a conexão foi fechada)
 * @param err Código de erro recebido do lwIP
 * @return err_t ERR_OK se processado com sucesso, ou outro código de erro
 */
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
    if (!p){
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs){
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

    if (strstr(req, "GET /api/data")) {
        char json_payload[128];
        int json_len = snprintf(json_payload, sizeof(json_payload),
            "{\"min\":%d,\"max\":%d,\"nivel_atual\":%d,\"estado_bomba\":%s}\r\n",
            nv.min, nv.max, nv.nivel_atual, nv.estado_bomba ? "true" : "false");
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            json_len, json_payload);

    } else if (strstr(req, "POST /api/limites")){
        char *min_str = strstr(req, "min=");
        char *max_str = strstr(req, "max=");
        if (min_str && max_str){
            min_str += 4; // Pular "min="
            max_str += 4; // Pular "max="

            nv.min = atoi(min_str);
            nv.max = atoi(max_str);

            if (nv.min >= nv.max){
                hs->len = snprintf(hs->response, sizeof(hs->response),
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n"
                    "\r\n");
            }
        }
    } else {
       size_t html_len = strlen(HTML_BODY);

        int hdr_len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n", html_len);

        memcpy(hs->response + hdr_len, HTML_BODY, html_len);

        hs->len = hdr_len + html_len;
        hs->sent = 0;

        tcp_arg(tpcb, hs);
        tcp_sent(tpcb, http_sent); // chama http_sent() após cada envio

        // envia apenas o primeiro pedaço (até 1024 bytes)
        size_t chunk = hs->len > 1024 ? 1024 : hs->len;
        tcp_write(tpcb, hs->response, chunk, TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        hs->sent = chunk;

        pbuf_free(p);
        return ERR_OK;
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);

    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}

/**
 * @brief Função de callback para novas conexões TCP
 * 
 * Esta função é chamada quando uma nova conexão TCP é estabelecida.
 * É usada para processar novas conexões de clientes no sistema de monitoramento 
 * de nível de água.
 * 
 * @param arg Ponteiro para argumentos definidos pelo usuário (não utilizado)
 * @param newpcb Ponteiro para o bloco de controle de protocolo (PCB) da nova conexão
 * @param err Código de erro da operação de aceitação
 * 
 * @return err_t Código de erro indicando o resultado do processamento da conexão
 *         - ERR_OK se a conexão foi processada com sucesso
 *         - Outros códigos de erro em caso de falha
 */
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err){
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

/**
 * @brief Inicializa e inicia o servidor HTTP para monitoramento do nível de água
 * 
 * Esta função configura e lança o servidor HTTP embutido que fornece
 * a interface web para monitoramento e controle dos níveis de água. O servidor
 * gerencia conexões de entrada e processa requisições HTTP relacionadas ao
 * sistema de nível de água.
 * 
 * @note Esta função é estática e só acessível dentro deste arquivo
 * @note Esta função não retorna até que o servidor seja explicitamente parado
 *       ou encontre um erro fatal
 * 
 * @return Nenhum
 */
static void start_http_server(void){
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb){
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK){
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}