#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// Definicje pinów dla joysticków
#define VRX_DIR_PIN 39  // X-axis dla joysticka kierunku
#define VRY_DIR_PIN 36  // Y-axis dla joysticka kierunku

#define VRY_SPEED_PIN 34  // X-axis dla joysticka prędkości

#define LED 2

// Struktura danych do wysyłania
typedef struct {
    int xDir;   // Kierunek: lewo/prawo
    int yDir;   // Kierunek: przód/tył
    int speed;  // Prędkość obrotów
} JoystickData;

JoystickData joystickData;

// Callback dla statusu wysyłania
void onSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void debug();

void setup() {
    Serial.begin(115200);
    pinMode(LED,OUTPUT);

    // Inicjalizacja WiFi w trybie STA
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Błąd inicjalizacji ESP-NOW");
    }
    esp_now_register_send_cb(onSent);

    // Dodanie odbiornika (adres MAC)
    uint8_t broadcastAddress[] = {0xec, 0x62, 0x60, 0x77, 0x32, 0x3c};
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Łączenie w pare
    while (esp_now_add_peer(&peerInfo) != ESP_OK) {
        digitalWrite(LED,HIGH);
        Serial.println("Nie udało się dodać odbiornika");
        delay(500);
        digitalWrite(LED,LOW);
        delay(500);
    }
}

void loop() {
    // Odczyt joysticka kierunku
    joystickData.xDir = analogRead(VRX_DIR_PIN);
    joystickData.yDir = analogRead(VRY_DIR_PIN);

    // Odczyt joysticka prędkości
    joystickData.speed = analogRead(VRY_SPEED_PIN);

    debug();

    // Wysyłanie danych
    esp_now_send(NULL, (uint8_t *)&joystickData, sizeof(joystickData));
}

// Callback dla statusu wysyłania
void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Wysłano dane, status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sukces" : "Niepowodzenie");
}
void debug() {
    // Wyświetlanie danych w monitorze szeregowym
    Serial.print("Kierunek - X: ");
    Serial.print(joystickData.xDir);
    Serial.print(", Y: ");
    Serial.print(joystickData.yDir);
    Serial.print(" | Prędkość: ");
    Serial.println(joystickData.speed);
}