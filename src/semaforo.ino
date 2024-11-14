#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
// Configurações WiFi
const char* ssid = "Net";
const char* password = "19062006";
// Configurações do MQTT no HiveMQ Cloud
const char* mqttServer = "b8cd02201bde4d4abe8a9e7d490f36fb.s1.eu.hivemq.cloud";
const int mqttPort = 8883;
const char* mqttUser = "esp32-credentials";
const char* mqttPassword = "@GolDoCorinthians1";
const char* clientID = "9079532";
int lastLdrReadStateMillis = millis();
String initTopic = "traffic-light/" + String(clientID) + "/new"; // Tópico de inicialização
String stateTopic = "traffic-light/state/" + String(clientID); // Tópico para receber o estado do semáforo
String stateLdrTopic = "traffic-light/" + String(clientID) + "/ldr-state"; // Tópico para enviar o estado do LDR
String buttonStateTopic = "traffic-light/" + String(clientID) + "/walker-button";
WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);
// Definição dos pinos dos LEDs e do LDR
const int greenLedPin = 25;
const int yellowLedPin = 33;
const int redLedPin = 32;
const int ldrPin = 34; // Pino analógico para o sensor LDR
const int buttonPin = 26;
int buttonPressStartTime = 0;
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
const int debounceDelay = 50; // 50 ms de debounce
// Variável de controle do estado do semáforo
enum TrafficLightState { RED, YELLOW, GREEN };
TrafficLightState currentState = RED;  // Estado inicial padrão
void setup() {
  Serial.begin(115200);
  // Configuração dos pinos dos LEDs como saída
  pinMode(greenLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(ldrPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  // Conexão WiFi
  Serial.print("Conectando ao WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Conectado ao WiFi!");
  wifiClient.setInsecure();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback); // Define a função de callback para receber mensagens MQTT
  connectMQTT();
  // Publicar uma mensagem de inicialização
  sendInitializationMessage();
  // Inscrever-se no tópico para receber o estado do semáforo
  client.subscribe(stateTopic.c_str());
}
void loop() {
  client.loop(); // Mantém a conexão MQTT ativa
  checkLdrState(); // Verifica o estado do sensor LDR e envia se necessário
  checkButtonState();
}
// Função para conectar ao HiveMQ
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando ao HiveMQ...");
    if (client.connect(clientID, mqttUser, mqttPassword)) {
      Serial.println(" Conectado ao HiveMQ!");
      client.subscribe(stateTopic.c_str()); // Inscreve-se no tópico de estado
    } else {
      Serial.print(" Falha de conexão: ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}
// Função para enviar a mensagem de inicialização ao HiveMQ
void sendInitializationMessage() {
  if (client.connected()) {
    const char* initMessage = "{ \"status\": \"initialized\", \"device\": \"ESP32\" }";
    client.publish(initTopic.c_str(), initMessage);
    Serial.println("Mensagem de inicialização enviada!");
  }
}
// Função de callback para receber o estado do semáforo via MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Mensagem recebida no tópico ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);
  // Verifica o tópico e atualiza o estado conforme a mensagem recebida
  if (String(topic) == stateTopic) {
    if (message == "red") {
      currentState = RED;
      updateTrafficLight();
    } else if (message == "yellow") {
      currentState = YELLOW;
      updateTrafficLight();
    } else if (message == "green") {
      currentState = GREEN;
      updateTrafficLight();
    }
  }
}
// Função para atualizar os LEDs do semáforo
void updateTrafficLight() {
  // Apaga todos os LEDs primeiro
  digitalWrite(greenLedPin, LOW);
  digitalWrite(yellowLedPin, LOW);
  digitalWrite(redLedPin, LOW);
  // Acende o LED correspondente ao estado atual
  switch (currentState) {
    case RED:
      digitalWrite(redLedPin, HIGH);
      Serial.println("Semáforo alterado para VERMELHO");
      break;
    case YELLOW:
      digitalWrite(yellowLedPin, HIGH);
      Serial.println("Semáforo alterado para AMARELO");
      break;
    case GREEN:
      digitalWrite(greenLedPin, HIGH);
      Serial.println("Semáforo alterado para VERDE");
      break;
  }
}
// Função para verificar o estado do LDR e publicar se é HIGH ou LOW
String lastLdrState = "";
void checkLdrState() {
  if (millis() - lastLdrReadStateMillis > 500) {
    int ldrValue = analogRead(ldrPin); // Lê o valor do LDR
    String ldrState = (ldrValue < 50) ? "HIGH" : "LOW"; // Determina o estado baseado no limite
    if (lastLdrState != ldrState ){
      String ldrMessage = "{ \"ldrState\": \"" + ldrState + "\" }";
      // Publica o estado do LDR no tópico MQTT
      client.publish(stateLdrTopic.c_str(), ldrMessage.c_str());
      Serial.print("Estado do LDR publicado: ");
      Serial.println(ldrMessage);
      lastLdrState = ldrState;
    }
    lastLdrReadStateMillis = millis();  
  }
}

void checkButtonState() {
  int buttonState = digitalRead(buttonPin);

  // Implementação de debounce
  if (buttonState == LOW && (millis() - lastDebounceTime) > debounceDelay) {
    lastDebounceTime = millis(); // Atualiza o tempo do debounce

    if (!buttonPressed) { // Botão pressionado pela primeira vez
      buttonPressStartTime = millis();
      buttonPressed = true;
    }

    // Se o botão estiver pressionado por pelo menos 1 segundo
    if (millis() - buttonPressStartTime >= 1000 && buttonPressed) {
      String buttonMessage = "{ \"buttonState\": \"HIGH\" }";
      client.publish(buttonStateTopic.c_str(), buttonMessage.c_str());
      Serial.println("Estado do botão publicado: " + buttonMessage);
      buttonPressed = false; // Reseta para próxima leitura
    }
  } else if (buttonState == HIGH && buttonPressed) { 
    buttonPressed = false; // Reseta se o botão for liberado antes de 1 segundo
  }
}