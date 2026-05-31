/**
 * ==================================================================================
 * PROYECTO: JUEGO LED REFLEX (RGB REFLEX HERO)
 * Placa: ESP32 DevKit V1
 * Tira LED: SK6812 RGBW (120 LEDs, Data Pin: GPIO 13)
 * Botones: Azul (GPIO 26), Verde (GPIO 25), Rojo (GPIO 12) - Activos en LOW
 * Reproductor de Audio: MP3-TF-16P V3.0 (RX2: GPIO 16, TX2: GPIO 17)
 * ==================================================================================
 */

#include <Adafruit_NeoPixel.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <Preferences.h>

// --- CONFIGURACIÓN DE PINES ---
#define BOTON_AZUL   26
#define BOTON_VERDE  25
#define BOTON_ROJO   12

#define MP3_RX       16  // Conectado al TX del módulo MP3
#define MP3_TX       17  // Conectado al RX del módulo MP3

#define PIN_LED      13  // Pin de datos para la tira SK6812
#define NUM_LEDS     120

// --- PARÁMETROS DEL JUEGO ---
#define PLAY_AREA_START 10
#define PLAY_AREA_END   119
#define TARGET_START    60
#define TARGET_END      66

#define INITIAL_SPEED   120  // Velocidad inicial (ms por paso)
#define MIN_SPEED       35   // Velocidad máxima permitida (ms por paso)
#define SPEED_INCREMENT 4    // Cuánto acelera con cada acierto (se restan ms)

// --- ESTADOS DEL JUEGO ---
enum GameState {
  STATE_IDLE,
  STATE_START,
  STATE_PLAYING,
  STATE_HIT,
  STATE_MISS,
  STATE_GAMEOVER
};

// --- COLORES DEL JUEGO ---
enum BallColor {
  COLOR_BLUE,
  COLOR_GREEN,
  COLOR_RED,
  COLOR_COUNT
};

// --- ESTRUCTURA PARA BOTONES ---
struct Button {
  int pin;
  bool state;
  bool lastState;
  unsigned long lastDebounceTime;
  bool pressed;
};

// Instancias globales
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED, NEO_GRBW + NEO_KHZ800);
DFRobotDFPlayerMini myDFPlayer;
Preferences preferences;

// Variables de botones
Button btnBlue  = {BOTON_AZUL, HIGH, HIGH, 0, false};
Button btnGreen = {BOTON_VERDE, HIGH, HIGH, 0, false};
Button btnRed   = {BOTON_ROJO, HIGH, HIGH, 0, false};

// Estado general del juego
GameState currentState = STATE_IDLE;
int score = 0;
int highScore = 0;
int lives = 3;

// Propiedades de la bola
float ballPosition = PLAY_AREA_START;
BallColor currentBallColor = COLOR_BLUE;
float ballSpeed = INITIAL_SPEED; // Intervalo de tiempo en ms por cada avance de 1 LED
unsigned long lastBallMoveTime = 0;

// Variables de efectos visuales
unsigned long stateTimer = 0;
bool hasAudio = false;

// Prototipos de funciones
void updateButton(Button &btn);
void handleIdle();
void handleStart();
void handlePlaying();
void handleHit();
void handleMiss();
void handleGameOver();
void generateNewBall();
void drawGame();
void playSound(int track);
uint32_t getBallRGBW(BallColor color, uint8_t brightness);

void setup() {
  Serial.begin(115200);
  Serial.println(F("--- Iniciando Juego LED Reflex ---"));

  // Configuración de botones físicos
  pinMode(btnBlue.pin, INPUT_PULLUP);
  pinMode(btnGreen.pin, INPUT_PULLUP);
  pinMode(btnRed.pin, INPUT_PULLUP);

  // Inicializar tira LED SK6812 RGBW
  strip.begin();
  strip.show(); // Apagar todos los LEDs al inicio

  // Inicializar preferencias (guardar High Score permanentemente)
  preferences.begin("juegoled", false);
  highScore = preferences.getInt("highscore", 0);
  Serial.print(F("Record actual: "));
  Serial.println(highScore);

  // Inicializar comunicación con módulo MP3-TF-16P (Serial 2)
  Serial2.begin(9600, SERIAL_8N1, MP3_RX, MP3_TX);
  Serial.println(F("Conectando con modulo MP3-TF-16P..."));
  
  if (myDFPlayer.begin(Serial2)) {
    Serial.println(F("Modulo MP3 conectado correctamente."));
    hasAudio = true;
    myDFPlayer.volume(18); // Volumen inicial (0 a 30)
    delay(500);
    // Reproducir pista de introducción suave
    playSound(1);
  } else {
    Serial.println(F("ADVERTENCIA: No se detecto el modulo MP3. El juego funcionara sin audio."));
  }

  randomSeed(analogRead(0));
}

void loop() {
  // Actualizar lectura de los tres botones
  updateButton(btnBlue);
  updateButton(btnGreen);
  updateButton(btnRed);

  // Máquina de estados
  switch (currentState) {
    case STATE_IDLE:
      handleIdle();
      break;
    case STATE_START:
      handleStart();
      break;
    case STATE_PLAYING:
      handlePlaying();
      break;
    case STATE_HIT:
      handleHit();
      break;
    case STATE_MISS:
      handleMiss();
      break;
    case STATE_GAMEOVER:
      handleGameOver();
      break;
  }
}

// --- ACTUALIZAR BOTÓN CON DEBOUNCE (NO BLOQUEANTE) ---
void updateButton(Button &btn) {
  bool reading = digitalRead(btn.pin);
  btn.pressed = false;

  if (reading != btn.lastState) {
    btn.lastDebounceTime = millis();
  }

  if ((millis() - btn.lastDebounceTime) > 30) { // 30ms filtro debounce
    if (reading != btn.state) {
      btn.state = reading;
      if (btn.state == LOW) { // El pin pasa a GND al presionar
        btn.pressed = true;
      }
    }
  }
  btn.lastState = reading;
}

// --- REPRODUCIR SONIDO CON SEGURIDAD ---
void playSound(int track) {
  if (hasAudio) {
    myDFPlayer.play(track);
  }
}

// --- RETORNAR COLOR RGBW SEGÚN TIPO Y BRILLO ---
uint32_t getBallRGBW(BallColor color, uint8_t brightness) {
  uint16_t b = brightness;
  switch (color) {
    case COLOR_BLUE:  return strip.Color(0, 0, b, 0);       // Azul puro
    case COLOR_GREEN: return strip.Color(0, b, 0, 0);       // Verde puro
    case COLOR_RED:   return strip.Color(b, 0, 0, 0);       // Rojo puro
    default:          return strip.Color(0, 0, 0, b);       // Blanco puro
  }
}

// --- GENERAR NUEVA BOLA CON COLOR ALEATORIO ---
void generateNewBall() {
  ballPosition = PLAY_AREA_START;
  currentBallColor = (BallColor)random(0, COLOR_COUNT);
}

// --- DIBUJAR PANTALLA Y RENDERIZAR TIRA ---
void drawGame() {
  strip.clear();

  // 1. Dibujar Vidas (Píxeles 0, 1, 2)
  for (int i = 0; i < 3; i++) {
    if (i < lives) {
      strip.setPixelColor(i, strip.Color(0, 50, 0, 0)); // LED verde si hay vida
    } else {
      strip.setPixelColor(i, strip.Color(50, 0, 0, 0)); // LED rojo si se perdió
    }
  }
  strip.setPixelColor(3, strip.Color(0, 0, 0, 0)); // Separador apagado

  // 2. Dibujar Zona Objetivo (Target Zone) en el centro (Píxeles 60 a 66)
  // Usamos el canal blanco (RGBW) para una luz pura y distinguible
  for (int i = TARGET_START; i <= TARGET_END; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0, 15)); // Blanco tenue
  }

  // 3. Dibujar la bola con gradiente/blur de movimiento
  int pos = (int)ballPosition;
  if (pos >= PLAY_AREA_START && pos <= PLAY_AREA_END) {
    // Centro de la bola (Brillo fuerte)
    strip.setPixelColor(pos, getBallRGBW(currentBallColor, 200));
    
    // Bordes suavizados
    if (pos - 1 >= PLAY_AREA_START) {
      strip.setPixelColor(pos - 1, getBallRGBW(currentBallColor, 70));
    }
    if (pos + 1 <= PLAY_AREA_END) {
      strip.setPixelColor(pos + 1, getBallRGBW(currentBallColor, 70));
    }
  }

  strip.show();
}

// ==================================================================================
// CONTROLADORES DE ESTADO (MÁQUINA DE ESTADOS)
// ==================================================================================

// 1. MODO ESPERA (IDLE) - Animaciones suaves hasta pulsar un botón
void handleIdle() {
  static unsigned long lastAnimTick = 0;
  static int hue = 0;

  // Si se presiona cualquier botón, inicia el juego
  if (btnBlue.pressed || btnGreen.pressed || btnRed.pressed) {
    currentState = STATE_START;
    return;
  }

  // Animación Rainbow en toda la tira (Tono Neón suave)
  if (millis() - lastAnimTick > 30) {
    lastAnimTick = millis();
    hue += 256; // Avanzar en la rueda de color
    
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t pixelHue = hue + (i * 65536L / NUM_LEDS);
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue, 255, 60)));
    }
    
    // Pulsar botones virtuales en el panel de LEDs para llamar la atención
    strip.setPixelColor(0, strip.Color(0, 0, 120, 0)); // Azul
    strip.setPixelColor(1, strip.Color(0, 120, 0, 0)); // Verde
    strip.setPixelColor(2, strip.Color(120, 0, 0, 0)); // Rojo
    
    strip.show();
  }
}

// 2. INICIO DE JUEGO - Destello y sonido de inicio
void handleStart() {
  Serial.println(F("Iniciando partida..."));
  playSound(1); // Sonido de inicio (Música de fondo)

  // Efecto visual de cuenta regresiva/flash
  for (int f = 0; f < 3; f++) {
    strip.fill(strip.Color(0, 0, 0, 150)); // Destello blanco puro (RGBW)
    strip.show();
    delay(150);
    strip.clear();
    strip.show();
    delay(150);
  }

  score = 0;
  lives = 3;
  ballSpeed = INITIAL_SPEED;
  generateNewBall();
  
  lastBallMoveTime = millis();
  currentState = STATE_PLAYING;
}

// 3. JUEGO ACTIVO (PLAYING) - El runner se mueve y se evalúan botones
void handlePlaying() {
  unsigned long now = millis();

  // Movimiento de la bola
  if (now - lastBallMoveTime >= ballSpeed) {
    lastBallMoveTime = now;
    ballPosition += 1.0; // Avanzar de a un LED

    // Si la bola sobrepasa la zona de juego sin ser golpeada -> ¡Fallo!
    if (ballPosition > PLAY_AREA_END) {
      currentState = STATE_MISS;
      stateTimer = millis();
      return;
    }
  }

  // Comprobar pulsaciones
  bool anyButtonPressed = btnBlue.pressed || btnGreen.pressed || btnRed.pressed;
  
  if (anyButtonPressed) {
    int pos = (int)ballPosition;
    bool insideTarget = (pos >= TARGET_START && pos <= TARGET_END);
    
    if (insideTarget) {
      // Verificar si el botón presionado coincide con el color de la bola
      bool correctPress = false;
      if (currentBallColor == COLOR_BLUE && btnBlue.pressed) correctPress = true;
      else if (currentBallColor == COLOR_GREEN && btnGreen.pressed) correctPress = true;
      else if (currentBallColor == COLOR_RED && btnRed.pressed) correctPress = true;

      if (correctPress) {
        currentState = STATE_HIT;
        stateTimer = millis();
      } else {
        // Presionó el botón equivocado dentro de la zona
        currentState = STATE_MISS;
        stateTimer = millis();
      }
    } else {
      // Presionó antes de tiempo (fuera de la zona objetivo)
      currentState = STATE_MISS;
      stateTimer = millis();
    }
  }

  drawGame();
}

// 4. ACIERTO (HIT) - Destello verde/azul/rojo del color acertado
void handleHit() {
  score++;
  Serial.print(F("¡Acierto! Puntuacion: "));
  Serial.println(score);
  
  playSound(2); // Sonido de golpe exitoso (Acierto)

  // Aumentar velocidad reduciendo el paso (min 35ms)
  ballSpeed -= SPEED_INCREMENT;
  if (ballSpeed < MIN_SPEED) ballSpeed = MIN_SPEED;

  // Destello rápido en la zona objetivo del color correspondiente
  uint32_t flashColor = getBallRGBW(currentBallColor, 255) | strip.Color(0, 0, 0, 100);
  
  for (int i = 0; i < 4; i++) {
    for (int j = TARGET_START; j <= TARGET_END; j++) {
      strip.setPixelColor(j, (i % 2 == 0) ? flashColor : strip.Color(0, 0, 0, 0));
    }
    strip.show();
    delay(40);
  }

  generateNewBall();
  lastBallMoveTime = millis();
  currentState = STATE_PLAYING;
}

// 5. FALLO (MISS) - Destello rojo y pérdida de una vida
void handleMiss() {
  lives--;
  Serial.print(F("¡Fallo! Vidas restantes: "));
  Serial.println(lives);
  
  playSound(3); // Sonido de error (Fallo)

  // Destello rojo intenso en toda la zona objetivo
  for (int i = 0; i < 3; i++) {
    for (int j = TARGET_START; j <= TARGET_END; j++) {
      strip.setPixelColor(j, strip.Color(255, 0, 0, 0));
    }
    strip.show();
    delay(100);
    for (int j = TARGET_START; j <= TARGET_END; j++) {
      strip.setPixelColor(j, strip.Color(0, 0, 0, 0));
    }
    strip.show();
    delay(100);
  }

  if (lives <= 0) {
    currentState = STATE_GAMEOVER;
    stateTimer = millis();
  } else {
    generateNewBall();
    lastBallMoveTime = millis();
    currentState = STATE_PLAYING;
  }
}

// 6. FIN DE JUEGO (GAME OVER) - Animación roja y puntuaciones
void handleGameOver() {
  static bool hasSavedRecord = false;
  static bool newRecordState = false;
  
  if (!hasSavedRecord) {
    hasSavedRecord = true;
    playSound(3); // Sonido de Game Over (Fallo)

    if (score > highScore) {
      highScore = score;
      preferences.putInt("highscore", highScore);
      Serial.print(F("¡NUEVO RECORD!: "));
      Serial.println(highScore);
      newRecordState = true;
      delay(200);
      playSound(2); // Tono de nuevo récord (Celebración acierto)
    }
  }

  // Visualizar resultado en los LEDs
  strip.clear();
  
  // 1. Mostrar puntaje en barra amarilla/dorada (hasta 100 leds)
  int ledsToLight = min(score, 100);
  for (int i = 0; i < ledsToLight; i++) {
    // Gradiente de verde a amarillo según puntaje
    strip.setPixelColor(i + 10, strip.Color(120, 90, 0, 0)); 
  }

  // 2. Mostrar la marca del récord anterior en magenta intermitente
  int recordPos = min(highScore, 100) + 10;
  if ((millis() / 300) % 2 == 0) {
    strip.setPixelColor(recordPos, strip.Color(150, 0, 150, 0)); // Magenta
  }

  // Si es nuevo récord, hacer parpadear toda la tira en colores festivos
  if (newRecordState) {
    if ((millis() / 200) % 2 == 0) {
      for (int i = 0; i < 10; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0, 100)); // Flash blanco en vidas
      }
    }
  } else {
    // Indicar vidas perdidas (3 LEDs rojos fijos)
    for (int i = 0; i < 3; i++) {
      strip.setPixelColor(i, strip.Color(150, 0, 0, 0));
    }
  }

  strip.show();

  // Esperar a que se presione cualquier botón para volver a IDLE
  if (btnBlue.pressed || btnGreen.pressed || btnRed.pressed) {
    // Sonido de confirmación y volver a empezar
    playSound(1);
    hasSavedRecord = false;
    newRecordState = false;
    currentState = STATE_IDLE;
    delay(300); // Evitar doble lectura
  }
}
