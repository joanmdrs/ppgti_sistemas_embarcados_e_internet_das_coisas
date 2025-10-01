#include <WiFi.h>           // Biblioteca para conexão Wi-Fi
#include <PubSubClient.h>   // Biblioteca para comunicação MQTT
#include <ArduinoJson.h>    // Biblioteca para manipulação de JSON (serialização/desserialização)
#include <time.h>           // Biblioteca para obter a data e hora via NTP

// --- Definição dos Pinos Físicos do ESP32 ---
#define TRIG_PIN 5          // Pino para o sensor ultrassônico (envia o pulso)
#define ECHO_PIN 18         // Pino para o sensor ultrassônico (recebe o pulso refletido)
#define PINO_PESO 35        // Pino analógico para simulação do sensor de peso
#define PINO_GAS 34         // Pino analógico para simulação do sensor de gás (ex: MQ-4)

#define LED_VERDE 2         // Pino para o LED indicador de nível 'Normal'
#define LED_AMARELO 4       // Pino para o LED indicador de nível 'Alerta'
#define LED_VERMELHO 15     // Pino para o LED indicador de nível 'Cheia'

// --- Configurações da Rede Wi-Fi ---
const char* ssid = "Wokwi-GUEST";     // Nome da rede Wi-Fi (SSID)
const char* password = "";            // Senha da rede Wi-Fi (vazio para Wokwi-GUEST)

// --- Configurações do Broker MQTT ---
const char* mqtt_server = "broker.hivemq.com"; // Endereço do broker MQTT público
const int mqtt_port = 1883;                   // Porta padrão do MQTT
const char* base_topic = "cidadeinteligente/lixeira1"; // Tópico base para esta lixeira específica
const char* device_id = "lixeira1";           // ID único para esta lixeira no sistema MQTT

// --- Variáveis de Cliente para Wi-Fi e MQTT ---
WiFiClient espClient;          // Objeto cliente Wi-Fi para o ESP32
PubSubClient client(espClient); // Objeto cliente MQTT, usando o cliente Wi-Fi

// --- Temporizadores para Controle de Envio de Dados ---
unsigned long ultimoEnvio = 0;   // Armazena o último momento em que os dados foram enviados
unsigned long ultimoStatus = 0;  // Armazena o último momento em que o status foi enviado
const unsigned long intervaloEnvio = 5000;  // Intervalo para enviar dados dos sensores (5 segundos)
const unsigned long intervaloStatus = 60000; // Intervalo para enviar o status "online" (60 segundos)

// --- Informações Fixas da Lixeira (Metadados) ---
const char* LOCALIZACAO = "Rua A, Setor 1";     // Localização física da lixeira
const char* TIPO_RESIDUO = "Reciclável";        // Tipo de resíduo aceito pela lixeira
const char* ULTIMA_COLETA = "22/07/2025 10:30"; // Data e hora da última coleta (exemplo)
const char* TEMPO_DESDE_COLETA = "2 dias";      // Tempo decorrido desde a última coleta (exemplo)
const char* OBSERVACOES = "";                   // Campo para observações adicionais

// --- Funções Auxiliares ---

// Função para conectar o ESP32 à rede Wi-Fi
void conectaWiFi() {
  Serial.print("Conectando Wi-Fi"); // Mensagem inicial no monitor serial
  WiFi.begin(ssid, password);       // Inicia a conexão Wi-Fi
  while (WiFi.status() != WL_CONNECTED) { // Loop enquanto não estiver conectado
    delay(500);                     // Espera 500ms
    Serial.print(".");              // Imprime um ponto para indicar que está tentando
  }
  Serial.println(" conectado!");    // Mensagem de sucesso
}

// Função para conectar o ESP32 ao broker MQTT
void conectaMQTT() {
  while (!client.connected()) {     // Loop enquanto não estiver conectado ao MQTT
    Serial.print("Conectando MQTT..."); // Mensagem de tentativa
    // Tenta conectar com o ID do cliente (device_id)
    if (client.connect(device_id)) {
      Serial.println(" conectado!"); // Mensagem de sucesso
      // Subscreve ao tópico de comando para este dispositivo.
      // Isso permite que o ESP32 receba comandos (ex: para controlar LEDs)
      client.subscribe((String(base_topic) + "/comando").c_str());
    } else {
      Serial.print(" falhou, rc="); // Mensagem de falha
      Serial.println(client.state()); // Imprime o código do erro MQTT
      delay(5000); // Espera 5 segundos antes de tentar novamente
    }
  }
}

// Função para configurar os pinos dos LEDs como saída e desligá-los inicialmente
void configurarLEDs() {
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  digitalWrite(LED_VERDE, LOW);    // Desliga o LED verde
  digitalWrite(LED_AMARELO, LOW);  // Desliga o LED amarelo
  digitalWrite(LED_VERMELHO, LOW); // Desliga o LED vermelho
}

// Função para atualizar o estado dos LEDs com base no nível de preenchimento da lixeira
void atualizarLEDs(float nivel) {
  // Desliga todos os LEDs primeiro para garantir que apenas um esteja aceso
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_VERMELHO, LOW);

  // Acende o LED apropriado com base no nível
  if (nivel < 60) {
    digitalWrite(LED_VERDE, HIGH); // Nível abaixo de 60%: Normal (LED Verde aceso)
  } else if (nivel < 90) {
    digitalWrite(LED_AMARELO, HIGH); // Nível entre 60% e 90%: Alerta (LED Amarelo aceso)
  } else {
    digitalWrite(LED_VERMELHO, HIGH); // Nível igual ou acima de 90%: Cheia (LED Vermelho aceso)
  }
}

// Função que retorna qual LED está atualmente aceso (para incluir no JSON)
String ledAtual() {
  if (digitalRead(LED_VERDE)) return "verde";     // Se o LED verde está HIGH
  if (digitalRead(LED_AMARELO)) return "amarelo"; // Se o LED amarelo está HIGH
  if (digitalRead(LED_VERMELHO)) return "vermelho"; // Se o LED vermelho está HIGH
  return "desligado"; // Se nenhum LED estiver aceso
}

// Função para medir a distância usando o sensor ultrassônico
float medirDistancia() {
  // Limpa o pino Trig definindo-o como LOW por 2 microssegundos
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  // Define o pino Trig como HIGH por 10 microssegundos para enviar um pulso sonoro
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Mede a duração do pulso de retorno no pino Echo.
  // O timeout de 30ms evita que a função trave se o eco não for detectado.
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms de timeout
  // Se nenhum pulso for recebido (timeout), retorna um valor alto padrão (100cm),
  // o que resultará em nível baixo para a lixeira.
  if (duration == 0) return 100.0;
  // Calcula a distância em cm. A velocidade do som é ~0.034 cm/microssegundo.
  // Divide por 2 porque o som viaja até o objeto e volta.
  return duration * 0.034 / 2;
}

// Função para simular a leitura do sensor de peso
float lerPesoSimulado() {
  // Lê o valor analógico do pino do sensor de peso (PINO_PESO)
  int leitura = analogRead(PINO_PESO);
  // Mapeia a leitura analógica (0-4095 para ESP32 ADC) para um intervalo de peso simulado (0-5000 gramas)
  // Divide por 100.0 para converter gramas para quilogramas (ex: 0-50 kg)
  return map(leitura, 0, 4095, 0, 5000) / 100.0;
}

// Nova função para simular a leitura do sensor de gás (ex: MQ-4 para metano, etc.)
float lerGasSimulado() {
  int leitura = analogRead(PINO_GAS);
  // Mapeia a leitura analógica (0-4095) para um valor de PPM (partes por milhão),
  // um intervalo comum para sensores de gás (ex: 0 a 1000 PPM)
  return map(leitura, 0, 4095, 0, 1000);
}

// Função para obter a data e hora formatada
String obterDataHora() {
  time_t now = time(nullptr);         // Obtém a hora atual do servidor NTP (já configurado no setup)
  struct tm* timeinfo = localtime(&now); // Converte para a estrutura de tempo local
  char buffer[25];                    // Buffer para armazenar a string formatada
  // Formata a hora em uma string (DD/MM/YYYY HH:MM)
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", timeinfo);
  return String(buffer);              // Retorna a string com a data e hora
}

// --- Publicar Nível (usando ArduinoJson para formatar os dados) ---
void publicarNivel(float nivel, float distancia, float peso) {
  // StaticJsonDocument é eficiente e aloca memória estaticamente.
  // O tamanho (512 bytes) deve ser suficiente para o payload JSON.
  StaticJsonDocument<512> doc;

  // Adiciona os campos essenciais ao documento JSON
  doc["device_id"] = device_id;
  // Capacidade com 1 casa decimal para maior precisão
  doc["capacidade"] = String(nivel, 1);
  // Define o status da lixeira com base no nível
  doc["status"] = (nivel >= 90) ? "Cheia" : (nivel >= 60 ? "Alerta" : "Normal");
  doc["ultima_leitura"] = obterDataHora(); // Data e hora da leitura atual

  // Campos adicionais de peso e gás
  doc["peso_kg"] = String(peso, 2);  // Peso com 2 casas decimais
  doc["gas_ppm"] = lerGasSimulado(); // Leitura simulada do sensor de gás
  doc["led"] = ledAtual();           // Qual LED está aceso no momento
  doc["localizacao"] = LOCALIZACAO;  // Metadado: localização da lixeira
  doc["tipo_residuo"] = TIPO_RESIDUO; // Metadado: tipo de resíduo

  // Serializa o documento JSON para uma string (payload)
  char payload[512]; // Buffer para a string JSON
  size_t jsonSize = serializeJson(doc, payload); // Converte o JSON em string e retorna o tamanho

  // Verifica se a serialização foi bem-sucedida e publica a mensagem MQTT
  if (jsonSize > 0) {
    client.publish((String(base_topic) + "/nivel").c_str(), payload); // Publica no tópico "/nivel"
    Serial.println("Publicado (capacidade com decimais): " + String(payload)); // Imprime o payload enviado
  } else {
    Serial.println("ERRO: Falha ao serializar JSON ou buffer muito pequeno!"); // Erro se o JSON não couber
  }
}

// Função para publicar o status "online" da lixeira
void publicarStatus() {
  StaticJsonDocument<128> doc; // Documento JSON para o status
  doc["device_id"] = device_id;
  doc["status"] = "online";
  doc["timestamp"] = millis(); // Tempo de atividade atual em milissegundos

  char payload[128];
  serializeJson(doc, payload); // Serializa o JSON
  client.publish((String(base_topic) + "/status").c_str(), payload); // Publica no tópico "/status"
}

// Função de callback acionada quando uma mensagem é recebida via MQTT
void tratarComando(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic); // Imprime o tópico da mensagem
  Serial.print("Payload: ");
  for (int i = 0; i < length; i++) { // Imprime o conteúdo do payload
    Serial.print((char)payload[i]);
  }
  Serial.println();

  StaticJsonDocument<128> doc; // Documento JSON para deserialização do comando
  // Deserializa o payload JSON recebido
  DeserializationError error = deserializeJson(doc, payload, length);

  // Se a deserialização falhar, imprime um erro e retorna
  if (error) {
    Serial.print(F("Falha ao deserializar JSON: "));
    Serial.println(error.f_str());
    return;
  }

  // Extrai "led" e "estado" (on/off) do documento JSON
  String led = doc["led"];
  String estado = doc["estado"];

  // Controla os LEDs com base no comando recebido
  if (led == "verde") {
    digitalWrite(LED_VERDE, estado == "on" ? HIGH : LOW); // Liga/desliga LED verde
  } else if (led == "amarelo") {
    digitalWrite(LED_AMARELO, estado == "on" ? HIGH : LOW); // Liga/desliga LED amarelo
  } else if (led == "vermelho") {
    digitalWrite(LED_VERMELHO, estado == "on" ? HIGH : LOW); // Liga/desliga LED vermelho
  }
}

// --- Função de Configuração Inicial (Executada uma vez ao ligar o ESP32) ---
void setup() {
  Serial.begin(115200);      // Inicializa a comunicação serial para depuração
  pinMode(TRIG_PIN, OUTPUT); // Define o pino Trig como saída
  pinMode(ECHO_PIN, INPUT);  // Define o pino Echo como entrada
  // Não é necessário pinMode para PINO_PESO e PINO_GAS, pois analogRead já os configura como INPUT.
  configurarLEDs();          // Inicializa os pinos dos LEDs
  conectaWiFi();             // Conecta ao Wi-Fi
  // Configura o cliente NTP para sincronização de tempo.
  // -3 * 3600 é o offset para o fuso horário de Brasília (-3 horas em segundos).
  // "pool.ntp.org" é um servidor NTP público.
  configTime(-3 * 3600, 0, "pool.ntp.org");
  client.setServer(mqtt_server, mqtt_port); // Define o endereço e porta do broker MQTT
  client.setCallback(tratarComando);        // Define a função que será chamada quando uma mensagem MQTT for recebida
}

// --- Função Principal de Loop (Executada repetidamente após o setup) ---
void loop() {
  // Reconecta ao MQTT se a conexão cair
  if (!client.connected()) {
    conectaMQTT();
  }
  client.loop(); // Processa mensagens MQTT (mantém a conexão viva, lida com inscrições e callbacks)

  unsigned long agora = millis(); // Obtém o tempo atual em milissegundos desde o início

  // Publica o status "online" periodicamente
  if (agora - ultimoStatus > intervaloStatus) {
    publicarStatus(); // Chama a função para publicar o status
    ultimoStatus = agora; // Reinicia o temporizador do status
  }

  // Publica os dados dos sensores periodicamente
  if (agora - ultimoEnvio > intervaloEnvio) {
    float distancia = constrain(medirDistancia(), 0.0, 100.0); // Mede a distância, restringe a 0-100cm
    float nivel = constrain(100.0 - distancia, 0.0, 100.0);   // Calcula o nível de preenchimento (0-100%)
                                                             // (assumindo que 0cm é lixeira cheia e 100cm é vazia)
    float peso = lerPesoSimulado();      // Lê o peso simulado
    float gas_ppm = lerGasSimulado();    // Lê a simulação do gás

    atualizarLEDs(nivel);              // Atualiza os LEDs com base no nível
    publicarNivel(nivel, distancia, peso); // Publica todos os dados do sensor via MQTT

    // Imprime as leituras do sensor no monitor serial para depuração
    Serial.printf("Distância: %.1f cm | Nível: %.1f%% | Peso: %.2f kg | Gás: %.0f PPM\n", distancia, nivel, peso, gas_ppm);
    ultimoEnvio = agora; // Reinicia o temporizador de envio de dados
  }
}
