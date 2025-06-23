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
} nivel_agua_t;

nivel_agua_t nv = {20, 80, false, 30, 0};

const char HTML_BODY[] =
    "<!DOCTYPE html>"
    "<html lang=\"pt-br\">"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>Controle de Nível</title>"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{font-family:system-ui,-apple-system,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}"
    ".container{background:rgba(255,255,255,0.1);backdrop-filter:blur(20px);border-radius:20px;padding:30px;width:100%;max-width:400px;box-shadow:0 8px 32px rgba(0,0,0,0.3);border:1px solid rgba(255,255,255,0.2)}"
    ".title{text-align:center;font-size:1.8rem;margin-bottom:30px;font-weight:300}"
    ".tank{position:relative;width:100%;height:200px;background:rgba(255,255,255,0.1);border-radius:15px;overflow:hidden;margin-bottom:20px;border:2px solid rgba(255,255,255,0.2)}"
    ".water{position:absolute;bottom:0;width:100%;background:linear-gradient(180deg,#00d4ff,#0099cc);transition:height 0.8s cubic-bezier(0.4,0,0.2,1);border-radius:0 0 13px 13px}"
    ".level-text{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);font-size:2.5rem;font-weight:bold;text-shadow:0 2px 10px rgba(0,0,0,0.5);z-index:10}"
    ".limit{position:absolute;width:100%;height:2px;background:#ff6b6b;left:0;z-index:5;transition:bottom 0.8s ease}"
    ".limit::after{content:attr(data-label);position:absolute;right:5px;top:-10px;transform:translateY(-50%);font-size:0.8rem;font-weight:bold;color:#ff6b6b;text-shadow:0 1px 3px rgba(0,0,0,0.5)}"
    ".status{text-align:center;padding:15px;border-radius:10p;margin-bottom:25px;font-weight:bold;font-size:1.1rem;transition:all 0.3s ease;background:rgba(255,255,255,0.1);backdrop-filter:blur(10px);border:2px solid rgba(255,255,255,0.2)}"
    ".on{background:rgba(46,204,113,0.25);border:2px solid #2ecc71;box-shadow:0 0 20px rgba(46,204,113,0.3)}"
    ".off{background:rgba(231,76,60,0.25);border:2px solid #e74c3c;box-shadow:0 0 20px rgba(231,76,60,0.3)}"
    ".form{display:flex;flex-direction:column;gap:20px;background:rgba(255,255,255,0.1);backdrop-filter:blur(10px);border-radius:15px;padding:20px;border:1px solid rgba(255,255,255,0.2)}"
    ".input-group{display:flex;justify-content:space-between;align-items:center;padding:5px 0}"
    ".input-group label{font-size:1rem;font-weight:500;text-shadow:0 1px 3px rgba(0,0,0,0.3)}"
    ".input-group input{width:70px;padding:8px 12px;border:none;border-radius:8px;background:rgba(255,255,255,0.2);color:#fff;text-align:center;font-size:1rem;backdrop-filter:blur(10px);border:1px solid rgba(255,255,255,0.3);transition:all 0.3s ease}"
    ".input-group input:focus{outline:none;background:rgba(255,255,255,0.3);border-color:rgba(255,255,255,0.5);box-shadow:0 0 15px rgba(255,255,255,0.2)}"
    ".input-group input::placeholder{color:rgba(255,255,255,0.7)}"
    ".btn{width:100%;padding:12px;border:none;border-radius:10px;background:linear-gradient(45deg,#ff6b6b,#ee5a52);color:#fff;font-size:1rem;font-weight:bold;cursor:pointer;transition:transform 0.2s,box-shadow 0.2s}"
    ".btn:hover{transform:translateY(-2px);box-shadow:0 6px 20px rgba(0,0,0,0.3)}"
    ".btn:active{transform:translateY(0)}"
    ".feedback{text-align:center;margin-top:15px;font-weight:bold;height:20px;transition:opacity 0.3s}"
    "@media (max-width:480px){"
    ".container{padding:20px;margin:10px}"
    ".title{font-size:1.5rem}"
    ".level-text{font-size:2rem}"
    ".tank{height:150px}"
    "}"
    "</style>"
    "</head>"
    "<body>"
    "<div class=\"container\">"
    "<h1 class=\"title\">💧 Controle de Nível</h1>"
    "<div class=\"tank\">"
    "<div class=\"level-text\" id=\"level\">--%</div>"
    "<div class=\"water\" id=\"water\"></div>"
    "<div class=\"limit\" id=\"max-limit\" data-label=\"MAX: --%\"></div>"
    "<div class=\"limit\" id=\"min-limit\" data-label=\"MIN: --%\"></div>"
    "</div>"
    "<div class=\"status off\" id=\"pump-status\">Conectando...</div>"
    "<form class=\"form\" id=\"limits-form\">"
    "<div class=\"input-group\">"
    "<label>Nível Mínimo (%):</label>"
    "<input type=\"number\" id=\"min-input\" min=\"0\" max=\"100\" required>"
    "</div>"
    "<div class=\"input-group\">"
    "<label>Nível Máximo (%):</label>"
    "<input type=\"number\" id=\"max-input\" min=\"0\" max=\"100\" required>"
    "</div>"
    "<button type=\"submit\" class=\"btn\">Atualizar Limites</button>"
    "</form>"
    "<div class=\"feedback\" id=\"feedback\"></div>"
    "</div>"
    "<script>"
    "const water=document.getElementById('water');"
    "const levelText=document.getElementById('level');"
    "const pumpStatus=document.getElementById('pump-status');"
    "const maxLimit=document.getElementById('max-limit');"
    "const minLimit=document.getElementById('min-limit');"
    "const minInput=document.getElementById('min-input');"
    "const maxInput=document.getElementById('max-input');"
    "const feedback=document.getElementById('feedback');"
    "const form=document.getElementById('limits-form');"
    "async function fetchData(){"
    "try{"
    "const res=await fetch('/api/data');"
    "if(!res.ok)throw new Error('Network error');"
    "const data=await res.json();"
    "updateUI(data);"
    "}catch(err){"
    "console.error('Fetch failed:',err);"
    "pumpStatus.textContent='Erro de Conexão';"
    "pumpStatus.className='status off';"
    "}"
    "}"
    "function updateUI(data){"
    "water.style.height=data.nivel_atual+'%';"
    "levelText.textContent=data.nivel_atual+'%';"
    "pumpStatus.textContent=data.estado_bomba?'Bomba Ligada':'Bomba Desligada';"
    "pumpStatus.className='status '+(data.estado_bomba?'on':'off');"
    "maxLimit.style.bottom=data.max+'%';"
    "minLimit.style.bottom=data.min+'%';"
    "maxLimit.setAttribute('data-label','MAX: '+data.max+'%');"
    "minLimit.setAttribute('data-label','MIN: '+data.min+'%');"
    "if(document.activeElement!==minInput)minInput.value=data.min;"
    "if(document.activeElement!==maxInput)maxInput.value=data.max;"
    "}"
    "form.addEventListener('submit',async(e)=>{"
    "e.preventDefault();"
    "const minVal=parseInt(minInput.value);"
    "const maxVal=parseInt(maxInput.value);"
    "if(minVal>=maxVal){"
    "showFeedback('O nível mínimo deve ser menor que o máximo.','#e74c3c');"
    "return;"
    "}"
    "try{"
    "const res=await fetch('/api/limites',{"
    "method:'POST',"
    "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
    "body:`min=${minVal}&max=${maxVal}`"
    "});"
    "if(res.ok){"
    "showFeedback('Limites atualizados!','#2ecc71');"
    "await fetchData();"
    "}else{"
    "throw new Error('Failed to update');"
    "}"
    "}catch(err){"
    "console.error('Update failed:',err);"
    "showFeedback('Erro ao enviar dados.','#e74c3c');"
    "}"
    "});"
    "function showFeedback(msg,color){"
    "feedback.textContent=msg;"
    "feedback.style.color=color;"
    "feedback.style.opacity='1';"
    "setTimeout(()=>{"
    "feedback.style.opacity='0';"
    "setTimeout(()=>feedback.textContent='',300);"
    "},3000);"
    "}"
    "setInterval(fetchData,2000);"
    "fetchData();"
    "</script>"
    "</body>"
    "</html>";

struct http_state
{
    char response[8192];
    size_t len;
    size_t sent;
};

void init_bot(void);
void gpio_irq_handler(uint gpio, uint32_t events);
int64_t botao_pressionado(alarm_id_t, void *user_data);
uint8_t xcenter_pos(char *text);

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
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

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs)
    {
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

    }
    else if (strstr(req, "POST /api/limites"))
    {
        char *min_str = strstr(req, "min=");
        char *max_str = strstr(req, "max=");
        if (min_str && max_str)
        {
            min_str += 4; // Pular "min="
            max_str += 4; // Pular "max="

            nv.min = atoi(min_str);
            nv.max = atoi(max_str);

            if (nv.min >= nv.max)
            {
                hs->len = snprintf(hs->response, sizeof(hs->response),
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n"
                    "\r\n");
            }
        }
    }
    else
    {
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

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

static void start_http_server(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

int main(){
    stdio_init_all();
    init_bot();
    interrupcao(botao_a);
    interrupcao(botao_b);

    gpio_init(green_led);
    gpio_set_dir(green_led, GPIO_OUT);

    // Configura o buzzer com PWM
    buzzer_setup_pwm(BUZZER_PIN, 4000);

    PIO pio = pio0;
    uint sm = 0;
    ws2812_init(pio, sm);

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

    start_http_server();
    char str_x[14];
    char str_y[14];
    char str_pb[14];
    bool cor = true;

    char ip_text[] = "IP:";
    strcat(ip_text, ip_str);

    while (true) {
        cyw43_arch_poll();

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
            ssd1306_draw_string(&ssd, "Bomba: OFF", 2, 26);
            printf("Bomba desligada, nivel dentro dos limites.\n");
            nv.estado_bomba = false;
        }

        gpio_put(green_led, nv.estado_bomba); // Liga/desliga o LED verde conforme o estado da bomba
        if (nv.estado_bomba) {
            buzzer_play(BUZZER_PIN, 1, 700, 100); // Toca o buzzer se a bomba estiver ligada
        }

        ssd1306_hline(&ssd, 0, 127, 40, cor);

        ssd1306_draw_string(&ssd, ip_text, xcenter_pos(ip_text), 42);

        ssd1306_draw_string(&ssd, "EmbarcaTech", xcenter_pos("EmbarcaTech"), 50); // Desenha o IP

        ssd1306_send_data(&ssd); 

        set_pattern(pio0, 0, 0, "azul");

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
            gpio_put(green_led, nv.estado_bomba);
            last_time = current_time;
            printf("Botão B pressionado \n");
        }
}

int64_t botao_pressionado(alarm_id_t alarm, void *user_data) {
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
