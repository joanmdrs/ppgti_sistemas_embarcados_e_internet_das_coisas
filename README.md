# ♻️ Lixeira Inteligente com ESP32

---

## 📌 Contexto
O projeto consiste em desenvolver uma **lixeira inteligente** utilizando o microcontrolador **ESP32**, integrada a sensores e comunicação **MQTT**, capaz de monitorar o nível de preenchimento, peso e presença de gases no ambiente.  

O objetivo é criar uma solução que possa ser utilizada em **cidades inteligentes**, permitindo monitoramento remoto, automação de alertas e análise de dados em tempo real.

---

## 🛑 Problema
Coletar informações de lixeiras urbanas de forma manual é:

- Trabalhoso e demorado;  
- Pouco preciso na detecção do nível de resíduos;  
- Limitado em termos de gestão e planejamento de coleta.  

Soluções convencionais não permitem **monitoramento em tempo real** nem **integração com sistemas IoT**.

---

## 💡 Solução Proposta
A lixeira inteligente utiliza:

- **Sensor ultrassônico** → para medir o nível de resíduos;  
- **Sensor de peso** → para estimar a quantidade de resíduos em kg;  
- **Sensor de gás (simulado)** → para monitorar presença de gases nocivos (ex: metano);  
- **LEDs indicadores** → sinalizam o nível da lixeira (verde, amarelo, vermelho);  
- **ESP32 + MQTT** → envia os dados para um broker MQTT, permitindo monitoramento remoto;  
- **ArduinoJson** → formata os dados em JSON para fácil integração com sistemas IoT;  
- **NTP** → sincroniza data e hora das medições.

### Características principais
- **Monitoramento em tempo real:** dados enviados periodicamente via MQTT;  
- **Feedback visual:** LEDs indicam status de preenchimento;  
- **Flexível e expansível:** sensores podem ser ajustados ou substituídos conforme necessidade;  
- **Simulação de sensores adicionais:** peso e gás para testes sem hardware específico.

---

## 🧪 Procedimentos e Funcionalidades
1. **Medição de nível de resíduos**
   - Sensor ultrassônico calcula a distância entre o topo da lixeira e o lixo;  
   - Converte distância em percentual de preenchimento;  
   - Atualiza LEDs:  
     - Verde (<60%) → Normal  
     - Amarelo (60–90%) → Alerta  
     - Vermelho (>=90%) → Cheia  

2. **Leitura do peso**
   - Valor lido no pino analógico simula o peso dos resíduos;  
   - Enviado junto com o nível da lixeira em JSON.

3. **Monitoramento de gases**
   - Simulação de leitura de gases nocivos (0-1000 PPM);  
   - Pode ser substituído por sensor MQ real (ex: MQ-4).

4. **Comunicação MQTT**
   - Publica dados dos sensores no tópico `cidadeinteligente/lixeira1/nivel`;  
   - Publica status "online" a cada 60 segundos;  
   - Permite controle remoto de LEDs via tópico `cidadeinteligente/lixeira1/comando`.

5. **Registro de data e hora**
   - Sincronização via NTP;  
   - Data e hora incluídas nos dados enviados.

---

## ⚙️ Tecnologias e Ferramentas
- **ESP32** → microcontrolador principal;  
- **Sensores:** Ultrassônico HC-SR04, sensor de peso simulado, sensor de gás simulado (MQ);  
- **LEDs** → indicadores de status da lixeira;  
- **Wi-Fi** → conexão com broker MQTT;  
- **MQTT** → protocolo de comunicação IoT (broker público: `broker.hivemq.com`);  
- **ArduinoJson** → manipulação de dados JSON;  
- **NTP** → sincronização de hora;  
- **IDE Arduino** → programação do ESP32.

---

## 🔗 Tópicos MQTT
- **Nível da lixeira:** `cidadeinteligente/lixeira1/nivel`  
- **Status "online":** `cidadeinteligente/lixeira1/status`  
- **Comando de LEDs:** `cidadeinteligente/lixeira1/comando`  

Exemplo de payload JSON publicado no nível da lixeira:

```json
{
  "device_id": "lixeira1",
  "capacidade": "75.0",
  "status": "Alerta",
  "ultima_leitura": "01/10/2025 16:00",
  "peso_kg": "12.34",
  "gas_ppm": 230,
  "led": "amarelo",
  "localizacao": "Rua A, Setor 1",
  "tipo_residuo": "Reciclável"
}
