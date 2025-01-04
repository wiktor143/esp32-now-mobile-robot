#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// Definicje pinów dla mostka H (do sterowania dwoma silnikami)
#define MOTOR_A1 33     // Pin A1 mostka H dla silnika 1
#define MOTOR_A2 25     // Pin A2 mostka H dla silnika 1
#define MOTOR_B1 26     // Pin B1 mostka H dla silnika 2
#define MOTOR_B2 27     // Pin B2 mostka H dla silnika 2
#define MOTOR_PWM_A 32  // Pin PWM dla silnika 1
#define MOTOR_PWM_B 13  // Pin PWM dla silnika 2
#define LED 2

// Struktura danych odbieranych
typedef struct {
    int xValue;  // Pozycja osi X joysticka
    int yValue;  // Pozycja osi Y joysticka
    int speed;   // Prędkość pojazdu
} JoystickData;

JoystickData joystickData;

// Offset dla położenia wyjściowego joysticka
#define JOYSTICK_OFFSET 300

// Prędkości sterowania silnikiem
#define MAX_SPEED 255  // Maksymalna prędkość silnika (możesz dostosować w zależności od mostka H)
#define MIN_SPEED 100     // Minimalna prędkość (bardzo wolna jazda)
#define NORMAL_SPEED 110  // Prędkość normalna (gdy joystick w pozycji wyjściowej)
#define TURN_SPEED 85     // Predkość jednego z silników podczas skręcania

// Enum dla kierunków
enum Direction {
    FORWARD,   // Jazda do przodu
    BACKWARD,  // Jazda do tyłu
    IDLE,      // Na postoju (neutralny stan)
    LEFT_F,    // Skręt w lewo do przodu
    RIGHT_F,   // Skręt w prawo do przodu
    LEFT_B,    // Skręt w lewo do tyłu
    RIGHT_B,   // Skręt w prawo do tyłu
    RIGHT,
    LEFT
};

// Funkcja, która się automatycznie wykonuje w tle, gdy odbiornik otrzyma dane przez ESP-NOW
// Prototyp funkcji
void on_receive(const uint8_t *mac_addr, const uint8_t *data, int data_len);
void debug();

void setup() {
    Serial.begin(115200);
    pinMode(LED, OUTPUT);

    // Inicjalizacja WiFi w trybie STA (station)
    WiFi.mode(WIFI_STA);
    while (esp_now_init() != ESP_OK) {
        digitalWrite(LED, HIGH);
        Serial.println("Błąd inicjalizacji ESP-NOW");
        delay(500);
        digitalWrite(LED, LOW);
        delay(500);
    }
    digitalWrite(LED, LOW);
    // Rejestracja callbacka - funkcja onReceive wywołana, gdy odbiornik otrzyma dane
    esp_now_register_recv_cb(on_receive);

    // Ustawienie pinów mostka H
    pinMode(MOTOR_A1, OUTPUT);
    pinMode(MOTOR_A2, OUTPUT);
    pinMode(MOTOR_B1, OUTPUT);
    pinMode(MOTOR_B2, OUTPUT);
    pinMode(MOTOR_PWM_A, OUTPUT);
    pinMode(MOTOR_PWM_B, OUTPUT);
    pinMode(LED, OUTPUT);
}

void loop() {
    debug();

    // Obliczanie kierunku jazdy
    Direction forwardBackward = IDLE;  // Domyślnie stan spoczynku dla przód-tył
    Direction leftRight = IDLE;        // Domyślnie stan spoczynku dla lewo-prawo

    // Mapowanie wartości speed na prędkość silników
    int motorSpeed = map(joystickData.speed, 4095, 0, MIN_SPEED, MAX_SPEED);
    int motorSpeedA = motorSpeed;  // Prędkość jest już określona przez speed
    int motorSpeedB = motorSpeed;

    // Gałka w pozycji neutralnej z uwzględnionym offsetem
    if (joystickData.yValue < (1750 + JOYSTICK_OFFSET) &&
        joystickData.yValue > (1750 - JOYSTICK_OFFSET) &&
        joystickData.xValue < (1750 + JOYSTICK_OFFSET) &&
        joystickData.xValue > (1750 - JOYSTICK_OFFSET)) {
        forwardBackward = IDLE;
        leftRight = IDLE;
        Serial.println("Na postoju");
    }

    // Kierunek jazdy w zależności od wartości yValue i xValue, prawo przód albo prawo tył
    if (joystickData.xValue < 100 && joystickData.yValue <= 2050 && joystickData.yValue >= 1750) {
        leftRight = RIGHT_F;  // prawo przód
        Serial.println("Skręt w prawo przód");
    } else if (joystickData.xValue < 100 && joystickData.yValue < 1750 && joystickData.yValue >= 1450) {
        leftRight = RIGHT_B;  // prawo tył
        Serial.println("Skręt w prawo tył");
    }
    // Kierunek jazdy w zależności od wartości yValue i xValue (lewo-prawo do tyłu)
    if (joystickData.xValue > 4000 && joystickData.yValue <= 2050 && joystickData.yValue >= 1750) {
        leftRight = LEFT_F;  // Skręt w lewo
        Serial.println("Skręt w lewo przód ");
    } else if (joystickData.xValue > 4000 && joystickData.yValue < 1750 && joystickData.yValue >= 1450) {
        leftRight = LEFT_B;  // Skręt w prawo
        Serial.println("Skręt w lewo tył");
    }

    // Kierunek skrętu w zależności od wartości yValue (lewo-prawo)
    if (joystickData.xValue > (1750 + JOYSTICK_OFFSET) && (joystickData.yValue < 1450 || joystickData.yValue > 2050)) {
            leftRight = LEFT;  // Skręt w lewo
            Serial.println("Skręt w lewo");
        }
    else if (joystickData.xValue < (1750 - JOYSTICK_OFFSET) && (joystickData.yValue < 1450 || joystickData.yValue > 2050)) {
            leftRight = RIGHT;  // Skręt w prawo
            Serial.println("Skręt w prawo");
        }

    // Kierunek jazdy w zależności od wartości yValue (przód-tył)
    if (joystickData.yValue > (1750 + JOYSTICK_OFFSET)) {
        forwardBackward = FORWARD;
        Serial.println("Jazda do przodu");
    } else if (joystickData.yValue < (1750 - JOYSTICK_OFFSET)) {
        forwardBackward = BACKWARD;
        Serial.println("Jazda do tyłu");
    }

    // Jazda przód tył, nie skręca
    if (forwardBackward != IDLE && leftRight == IDLE) {
        if (forwardBackward == FORWARD) {
            digitalWrite(MOTOR_A1, LOW);
            digitalWrite(MOTOR_A2, HIGH);
            digitalWrite(MOTOR_B1, LOW);
            digitalWrite(MOTOR_B2, HIGH);

        } else if (forwardBackward == BACKWARD) {  // Jazda do tyłu: oba silniki do tyłu
            digitalWrite(MOTOR_A1, HIGH);
            digitalWrite(MOTOR_A2, LOW);
            digitalWrite(MOTOR_B1, HIGH);
            digitalWrite(MOTOR_B2, LOW);
        }
        // Jazda przód tył i skręca
    } else if (forwardBackward != IDLE && leftRight != IDLE) {
        if (forwardBackward == FORWARD && leftRight == RIGHT) {
            digitalWrite(MOTOR_A1, LOW);
            digitalWrite(MOTOR_A2, HIGH);
            digitalWrite(MOTOR_B1, LOW);
            digitalWrite(MOTOR_B2, HIGH);
            motorSpeedB = TURN_SPEED;  // Silnik 1 jedzie wolniej;

        } else if (forwardBackward == FORWARD &&
                   leftRight == LEFT) {  // Jazda do tyłu: oba silniki do tyłu
            digitalWrite(MOTOR_A1, LOW);
            digitalWrite(MOTOR_A2, HIGH);
            digitalWrite(MOTOR_B1, LOW);
            digitalWrite(MOTOR_B2, HIGH);
            motorSpeedA = TURN_SPEED;
        } else if (forwardBackward == BACKWARD &&
                   leftRight == RIGHT) {  // Jazda do tyłu: oba silniki do tyłu
            digitalWrite(MOTOR_A1, HIGH);
            digitalWrite(MOTOR_A2, LOW);
            digitalWrite(MOTOR_B1, HIGH);
            digitalWrite(MOTOR_B2, LOW);
            motorSpeedB = TURN_SPEED;
        } else if (forwardBackward == BACKWARD &&
                   leftRight == LEFT) {  // Jazda do tyłu: oba silniki do tyłu
            digitalWrite(MOTOR_A1, HIGH);
            digitalWrite(MOTOR_A2, LOW);
            digitalWrite(MOTOR_B1, HIGH);
            digitalWrite(MOTOR_B2, LOW);
            motorSpeedA = TURN_SPEED;
        }
        // Brak ruchu przód tył natomiast skręca
    } else if (forwardBackward == IDLE && leftRight != IDLE) {
        if (leftRight == RIGHT_F) {
            digitalWrite(MOTOR_A1, LOW);
            digitalWrite(MOTOR_A2, HIGH);
            digitalWrite(MOTOR_B1, LOW);
            digitalWrite(MOTOR_B2, LOW);
        } else if (leftRight == LEFT_F) {
            digitalWrite(MOTOR_A1, LOW);
            digitalWrite(MOTOR_A2, LOW);
            digitalWrite(MOTOR_B1, LOW);
            digitalWrite(MOTOR_B2, HIGH);
        } else if (leftRight == RIGHT_B) {
            digitalWrite(MOTOR_A1, HIGH);
            digitalWrite(MOTOR_A2, LOW);
            digitalWrite(MOTOR_B1, LOW);
            digitalWrite(MOTOR_B2, LOW);
        } else if (leftRight == LEFT_B) {
            digitalWrite(MOTOR_A1, LOW);
            digitalWrite(MOTOR_A2, LOW);
            digitalWrite(MOTOR_B1, HIGH);
            digitalWrite(MOTOR_B2, LOW);
        }
        // Inaczej oba silniki stoją
    } else {
        digitalWrite(MOTOR_A1, LOW);
        digitalWrite(MOTOR_A2, LOW);
        digitalWrite(MOTOR_B1, LOW);
        digitalWrite(MOTOR_B2, LOW);
    }
    analogWrite(MOTOR_PWM_A, motorSpeedA);
    analogWrite(MOTOR_PWM_B, motorSpeedB);
}

// Funkcja, która się automatycznie wykonuje w tle, gdy odbiornik otrzyma dane przez ESP-NOW
void on_receive(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    // Kopiowanie danych do zmiennej lokalnej joystickData
    memcpy(&joystickData, data, sizeof(joystickData));
}

void debug() {
    // Wypisanie odebranych danych
    Serial.print("Odebrano dane - ");
    Serial.print("X: ");
    Serial.print(joystickData.xValue);
    Serial.print(" | Y: ");
    Serial.print(joystickData.yValue);
    Serial.print(" | Speed: ");
    Serial.println(joystickData.speed);
}