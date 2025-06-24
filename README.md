# Controle de Nível de Água com Display OLED SSD1306, Wi-Fi e Rele – Raspberry Pi Pico W

## Descrição

Projeto de monitoramento e controle de nível de água usando Raspberry Pi Pico W.  
Utiliza um potenciômetro como sensor de nível, exibe dados em um display OLED SSD1306, aciona uma bomba por meio de relé e envia informações via servidor web local com Wi-Fi.  
Conta com botões físicos para reset e controle manual, além de alerta sonoro com buzzer.

---

## Hardware Utilizado

- Raspberry Pi Pico W (RP2040)  
- Display OLED SSD1306 (I2C)  
- Potenciômetro (atuando como sensor de nível)  
- Botões: botao_a (GPIO 5), botao_b (GPIO 6)  
- LED verde (GPIO 11)  
- Relé (GPIO 17)  
- Buzzer (GPIO 21)  
- Matriz de LEDs WS2812 (PIO)  

---

## Pinagem

| Função           | GPIO  |
|------------------|--------|
| Botão A          | 5      |
| Botão B          | 6      |
| LED Verde        | 11     |
| Relé             | 8      |
| I2C SDA (OLED)   | 14     |
| I2C SCL (OLED)   | 15     |
| Potenciômetro    | 28     |
| WS2812 (LEDs)    | Definido via PIO |
| Buzzer           | 21     |

---

## Funcionalidades

- Leitura do nível de água via ADC com média de 10 amostras.  
- Controle automático da bomba baseado em limites mínimo e máximo.  
- Display OLED exibe:
  - Nível atual (%)
  - Limites configurados
  - Estado da bomba
- Botão A: pressionado por 2 segundos, reseta os limites para valores padrão.  
- Botão B: alterna manualmente o estado da bomba (liga/desliga).  
- Buzzer emite sinal sonoro:
  - 1 beep ao ligar a bomba
  - 3 beeps ao desligar (nível máximo)  
- LED verde indica bomba ligada.  
- Relé ativa/desativa fisicamente a bomba.  
- Matriz WS2812 com efeito visual fixo (azul).  
- Servidor HTTP embutido:
  - Página HTML de visualização
  - API REST:
    - `GET /api/data`: retorna dados JSON do sistema
    - `POST /api/limites`: define novos valores para `min` e `max`  

---

## Como Compilar e Rodar

1. Instale o SDK da Raspberry Pi Pico W.  
2. Inclua as bibliotecas necessárias:  
   - `pico/`  
   - `hardware/adc.h`, `gpio.h`, `i2c.h`, `pwm.h`  
   - `cyw43_arch.h`, `lwip/tcp.h`  
   - `ssd1306.h`, `ws2812.h`, `buzzer.h`  
3. Compile com `cmake` e `make`, ou use o Visual Studio Code com extensão Pico C/C++.  
4. Faça o upload do `.uf2` para a placa via BOOTSEL.  
5. Conecte todos os periféricos conforme a pinagem.  
6. Verifique o IP da Placa e utilize-o no navegador.
Ou utilize a extensão do VSCode e faça uma importação.  

---

## Autor

- Hugo Martins Santana
- Cláudio Evaristo Júnior
- Davi Leão
- Caique de Brito Freitas
- Naylane do Nascimento Ribeiro

---

## Observações

- Os valores são mapeados de 120 a 1000 (ADC) para 0% a 100%.  
- Os botões usam interrupções e debounce com tempo mínimo (botão B) ou alarme (botão A).  
- Os dados são acessíveis remotamente via navegador.  
- O sistema é autônomo e independente de software externo após carregado.
