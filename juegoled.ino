/**
 * ==================================================================================
 * PROYECTO: CONSOLA MULTI-JUEGOS LED VERTICAL
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

#define PIN_LED      13  // Pin de datos para la tira SK6812 (resistencia 330Ω)
#define NUM_LEDS     120

// --- PARÁMETROS GENERALES ---
#define PLAY_AREA_START 10
#define PLAY_AREA_END   119

// --- ESTADOS GENERALES DE LA CONSOLA ---
enum GameState {
  STATE_MENU,
  STATE_START,
  STATE_PLAYING,
  STATE_HIT,
  STATE_MISS,
  STATE_GAMEOVER
};

// --- COLORES ---
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

// --- ESTRUCTURAS DE JUEGOS ---

// Juego 1: Defender
struct Meteorite {
  float position;
  BallColor color;
  bool active;
};

// Juego 2: Runner
struct Gate {
  int startPos;
  int endPos;
  BallColor color;
  bool active;
};

// Instancias globales
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED, NEO_GRBW + NEO_KHZ800);
DFRobotDFPlayerMini myDFPlayer;
Preferences preferences;

// Variables de botones
Button btnBlue  = {BOTON_AZUL, HIGH, HIGH, 0, false};
Button btnGreen = {BOTON_VERDE, HIGH, HIGH, 0, false};
Button btnRed   = {BOTON_ROJO, HIGH, HIGH, 0, false};

// Variables generales de la consola
GameState currentState = STATE_MENU;
int selectedGame = 0; // 1: Defender, 2: Runner
int score = 0;
int highScoreGame1 = 0;
int highScoreGame2 = 0;
int lives = 3;

// --- VARIABLES ESPECÍFICAS DE JUEGOS ---

// Juego 1: Defender
#define MAX_METEORITES 3
#define SHIELD_START 10
#define SHIELD_END 18
Meteorite meteorites[MAX_METEORITES];
unsigned long lastMeteoriteSpawn = 0;
unsigned long lastMeteoriteMove = 0;
unsigned long spawnInterval = 2500; // ms
unsigned long meteoriteSpeed = 100; // ms por paso

// Juego 2: Runner
#define NUM_GATES 3
Gate gates[NUM_GATES];
float runnerPosition = PLAY_AREA_START;
unsigned long lastRunnerMove = 0;
unsigned long runnerSpeed = 120; // ms por paso
unsigned long gateWaitTime = 0;  // Tiempo de espera en la barrera actual
bool isStoppedAtGate = false;
int currentGateIndex = -1;

// Variables generales
unsigned long stateTimer = 0;
bool hasAudio = false;

// Prototipos
void updateButton(Button &btn);
void playSound(int track);
void drawGame();
uint32_t getColorRGBW(BallColor color, uint8_t brightness);

// Inicializadores de juegos
void initGame1();
void initGame2();
void spawnMeteorite();
void handleGame1Playing();
void handleGame2Playing();

void setup() {
  Serial.begin(115200);
  Serial.println(F("--- Iniciando Consola LED Vertical ---"));

  // Configurar pines de botones
  pinMode(btnBlue.pin, INPUT_PULLUP);
  pinMode(btnGreen.pin, INPUT_PULLUP);
  pinMode(btnRed.pin, INPUT_PULLUP);

  // Inicializar tira LED
  strip.begin();
  strip.show();

  // Cargar Récords desde memoria persistente
  preferences.begin("consolaled", false);
  highScoreGame1 = preferences.getInt("highscore1", 0);
  highScoreGame2 = preferences.getInt("highscore2", 0);
  Serial.print(F("Record Juego 1: ")); Serial.println(highScoreGame1);
  Serial.print(F("Record Juego 2: ")); Serial.println(highScoreGame2);

  // Inicializar Reproductor MP3
  Serial2.begin(9600, SERIAL_8N1, MP3_RX, MP3_TX);
  if (myDFPlayer.begin(Serial2)) {
    hasAudio = true;
    myDFPlayer.volume(18); // Volumen 0-30
    delay(500);
    playSound(1); // Iniciar melodía de fondo en el menú
  } else {
    Serial.println(F("ADVERTENCIA: MP3 no conectado."));
  }

  randomSeed(analogRead(0));
}

void loop() {
  // Escanear botones
  updateButton(btnBlue);
  updateButton(btnGreen);
  updateButton(btnRed);

  // Máquina de estados
  switch (currentState) {
    case STATE_MENU:
      handleMenu();
      break;
    case STATE_START:
      handleStart();
      break;
    case STATE_PLAYING:
      if (selectedGame == 1) handleGame1Playing();
      else if (selectedGame == 2) handleGame2Playing();
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

// --- ACTUALIZACIÓN DE BOTONES (FILTRO ANTIRREBOTE) ---
void updateButton(Button &btn) {
  bool reading = digitalRead(btn.pin);
  btn.pressed = false;

  if (reading != btn.lastState) {
    btn.lastDebounceTime = millis();
  }

  if ((millis() - btn.lastDebounceTime) > 30) {
    if (reading != btn.state) {
      btn.state = reading;
      if (btn.state == LOW) {
        btn.pressed = true;
      }
    }
  }
  btn.lastState = reading;
}

// --- REPRODUCIR SONIDO ---
void playSound(int track) {
  if (hasAudio) {
    myDFPlayer.play(track);
  }
}

// --- COLOR RGBW ---
uint32_t getColorRGBW(BallColor color, uint8_t brightness) {
  uint16_t b = brightness;
  switch (color) {
    case COLOR_BLUE:  return strip.Color(0, 0, b, 0);
    case COLOR_GREEN: return strip.Color(0, b, 0, 0);
    case COLOR_RED:   return strip.Color(b, 0, 0, 0);
    default:          return strip.Color(0, 0, 0, b);
  }
}

// ==================================================================================
// CONTROLADOR DE MENÚ Y ESTADOS DE CONSOLA
// ==================================================================================

// 1. MODO SELECCIÓN DE JUEGO
void handleMenu() {
  static unsigned long lastMenuTick = 0;
  static int pulseVal = 0;
  static bool ascending = true;

  // Botón Azul = Selecciona Juego 1
  if (btnBlue.pressed) {
    selectedGame = 1;
    currentState = STATE_START;
    return;
  }
  // Botón Verde = Selecciona Juego 2
  if (btnGreen.pressed) {
    selectedGame = 2;
    currentState = STATE_START;
    return;
  }

  // Animación del menú (parpadeo de las zonas de menú en la tira)
  if (millis() - lastMenuTick > 20) {
    lastMenuTick = millis();
    if (ascending) {
      pulseVal += 4;
      if (pulseVal >= 120) ascending = false;
    } else {
      pulseVal -= 4;
      if (pulseVal <= 20) ascending = true;
    }

    strip.clear();
    
    // Iluminar la base (vidas) en blanco para indicar sistema encendido
    strip.setPixelColor(0, strip.Color(0, 0, 0, 30));
    strip.setPixelColor(1, strip.Color(0, 0, 0, 30));
    strip.setPixelColor(2, strip.Color(0, 0, 0, 30));

    // Indicador Juego 1 (Azul, parte inferior)
    for (int i = 15; i <= 25; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, pulseVal, 0));
    }

    // Indicador Juego 2 (Verde, parte superior)
    for (int i = 85; i <= 95; i++) {
      strip.setPixelColor(i, strip.Color(0, pulseVal, 0, 0));
    }

    strip.show();
  }
}

// 2. INICIO DE JUEGO - Cuenta regresiva
void handleStart() {
  Serial.print(F("Iniciando Juego: "));
  Serial.println(selectedGame);

  // Destello inicial de confirmación
  uint32_t flashCol = (selectedGame == 1) ? strip.Color(0, 0, 150, 0) : strip.Color(0, 150, 0, 0);
  for (int f = 0; f < 3; f++) {
    strip.fill(flashCol);
    strip.show();
    delay(100);
    strip.clear();
    strip.show();
    delay(100);
  }

  score = 0;
  lives = 3;

  if (selectedGame == 1) {
    initGame1();
  } else {
    initGame2();
  }

  // Reproducir melodía ambiental / loop
  playSound(1);

  currentState = STATE_PLAYING;
}

// 3. ACIERTO (HIT)
void handleHit() {
  score++;
  Serial.print(F("¡Acierto! Score: ")); Serial.println(score);

  if (selectedGame == 1) {
    playSound(2); // Explosión
    // El juego 1 reanuda directo
    currentState = STATE_PLAYING;
  } else {
    playSound(4); // Impulso
    // Para el juego 2, le damos un avance rápido hacia arriba como premio
    runnerPosition += 4.0;
    if (runnerPosition > PLAY_AREA_END) runnerPosition = PLAY_AREA_END;
    currentState = STATE_PLAYING;
  }
}

// 4. DAÑO O CHOQUE (MISS)
void handleMiss() {
  lives--;
  Serial.print(F("¡Daño recibido! Vidas: ")); Serial.println(lives);

  if (selectedGame == 1) {
    playSound(3); // Daño
  } else {
    playSound(5); // Choque
    // Retroceder corredor
    runnerPosition = max((float)PLAY_AREA_START, runnerPosition - 15.0f);
  }

  // Flash rojo de daño
  for (int f = 0; f < 2; f++) {
    strip.fill(strip.Color(180, 0, 0, 0));
    strip.show();
    delay(100);
    strip.clear();
    strip.show();
    delay(80);
  }

  if (lives <= 0) {
    currentState = STATE_GAMEOVER;
  } else {
    currentState = STATE_PLAYING;
    if (selectedGame == 2) {
      isStoppedAtGate = false;
      lastRunnerMove = millis();
    }
  }
}

// 5. FIN DEL JUEGO (GAME OVER)
void handleGameOver() {
  static bool saved = false;
  static bool isNewRecord = false;

  if (!saved) {
    saved = true;
    
    // Sonido según el juego
    if (selectedGame == 1) {
      playSound(3); // Sonido trágico Juego 1
      if (score > highScoreGame1) {
        highScoreGame1 = score;
        preferences.putInt("highscore1", highScoreGame1);
        isNewRecord = true;
        playSound(2); // Sonido celebración
      }
    } else {
      playSound(5); // Sonido trágico Juego 2
      if (score > highScoreGame2) {
        highScoreGame2 = score;
        preferences.putInt("highscore2", highScoreGame2);
        isNewRecord = true;
        playSound(4); // Sonido celebración
      }
    }
  }

  // Mostrar estadísticas
  strip.clear();

  // Barra dorada de score
  int fillLeds = min(score, 100);
  for (int i = 0; i < fillLeds; i++) {
    strip.setPixelColor(i + 10, strip.Color(100, 80, 0, 0));
  }

  // Marca de Récord
  int recVal = (selectedGame == 1) ? highScoreGame1 : highScoreGame2;
  int recPos = min(recVal, 100) + 10;
  if ((millis() / 300) % 2 == 0) {
    strip.setPixelColor(recPos, strip.Color(120, 0, 120, 0)); // Magenta
  }

  // Vidas en rojo
  for (int i = 0; i < 3; i++) {
    strip.setPixelColor(i, strip.Color(120, 0, 0, 0));
  }

  strip.show();

  // Pulsar cualquier botón para volver al Menú Principal
  if (btnBlue.pressed || btnGreen.pressed || btnRed.pressed) {
    playSound(1); // Sonido menú
    saved = false;
    isNewRecord = false;
    currentState = STATE_MENU;
    delay(400);
  }
}


// ==================================================================================
// LÓGICA DE JUEGOS INDIVIDUALES
// ==================================================================================

// --- JUEGO 1: COLOR SHIELD DEFENDER ---

void initGame1() {
  spawnInterval = 2500;
  meteoriteSpeed = 100;
  lastMeteoriteSpawn = millis();
  lastMeteoriteMove = millis();

  for (int i = 0; i < MAX_METEORITES; i++) {
    meteorites[i].active = false;
  }
  spawnMeteorite();
}

void spawnMeteorite() {
  for (int i = 0; i < MAX_METEORITES; i++) {
    if (!meteorites[i].active) {
      meteorites[i].position = PLAY_AREA_END;
      meteorites[i].color = (BallColor)random(0, COLOR_COUNT);
      meteorites[i].active = true;
      Serial.print(F("Meteorito generado en slot: ")); Serial.println(i);
      break;
    }
  }
}

void handleGame1Playing() {
  unsigned long now = millis();

  // 1. Dificultad dinámica según score
  spawnInterval = max(1000UL, 2500UL - (score * 80UL));
  meteoriteSpeed = max(35UL, 100UL - (score * 3UL));

  // 2. Generar meteoritos
  if (now - lastMeteoriteSpawn >= spawnInterval) {
    lastMeteoriteSpawn = now;
    spawnMeteorite();
  }

  // 3. Desplazar meteoritos hacia abajo
  if (now - lastMeteoriteMove >= meteoriteSpeed) {
    lastMeteoriteMove = now;
    for (int i = 0; i < MAX_METEORITES; i++) {
      if (meteorites[i].active) {
        meteorites[i].position -= 1.0;
        
        // Si impacta la base del juego (LED 10)
        if (meteorites[i].position < PLAY_AREA_START) {
          meteorites[i].active = false;
          currentState = STATE_MISS; // ¡Fallo!
          return;
        }
      }
    }
  }

  // 4. Evaluar pulsaciones de botones
  if (btnBlue.pressed || btnGreen.pressed || btnRed.pressed) {
    // Determinar qué color se pulsó
    BallColor pressedColor = COLOR_BLUE;
    if (btnGreen.pressed) pressedColor = COLOR_GREEN;
    else if (btnRed.pressed) pressedColor = COLOR_RED;

    // Buscar si hay algún meteorito del color correcto en la zona del escudo
    for (int i = 0; i < MAX_METEORITES; i++) {
      if (meteorites[i].active && meteorites[i].color == pressedColor) {
        int pos = (int)meteorites[i].position;
        if (pos >= SHIELD_START && pos <= SHIELD_END) {
          // ¡Destruido!
          meteorites[i].active = false;
          currentState = STATE_HIT;
          
          // Efecto de explosión rápida en la zona
          for (int e = 0; e < 3; e++) {
            strip.setPixelColor(pos, strip.Color(255, 255, 255, 255));
            if (pos-1 >= PLAY_AREA_START) strip.setPixelColor(pos-1, getColorRGBW(pressedColor, 180));
            if (pos+1 <= PLAY_AREA_END) strip.setPixelColor(pos+1, getColorRGBW(pressedColor, 180));
            strip.show();
            delay(20);
          }
          return;
        }
      }
    }
  }

  // 5. Renderizar Tira LED
  strip.clear();

  // Vidas
  for (int i = 0; i < 3; i++) {
    strip.setPixelColor(i, (i < lives) ? strip.Color(0, 50, 0, 0) : strip.Color(50, 0, 0, 0));
  }
  strip.setPixelColor(3, strip.Color(0, 0, 0, 0));

  // Dibujar zona del escudo blanca translúcida (LEDs 10 a 18)
  for (int i = SHIELD_START; i <= SHIELD_END; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0, 20));
  }

  // Dibujar meteoritos activos
  for (int i = 0; i < MAX_METEORITES; i++) {
    if (meteorites[i].active) {
      int pos = (int)meteorites[i].position;
      strip.setPixelColor(pos, getColorRGBW(meteorites[i].color, 220));
      // Estela
      if (pos + 1 <= PLAY_AREA_END) {
        strip.setPixelColor(pos + 1, getColorRGBW(meteorites[i].color, 60));
      }
    }
  }

  strip.show();
}


// --- JUEGO 2: COLOR DASH RUNNER ---

void initGame2() {
  runnerPosition = PLAY_AREA_START;
  runnerSpeed = 120;
  isStoppedAtGate = false;
  lastRunnerMove = millis();

  // Inicializar posiciones de las 3 barreras de color
  gates[0] = {35, 37, COLOR_BLUE, true};
  gates[1] = {65, 67, COLOR_GREEN, true};
  gates[2] = {95, 97, COLOR_RED, true};

  // Aleatorizar los colores de las barreras
  for (int i = 0; i < NUM_GATES; i++) {
    gates[i].color = (BallColor)random(0, COLOR_COUNT);
    gates[i].active = true;
  }
}

void handleGame2Playing() {
  unsigned long now = millis();

  // Dificultad basada en score (velocidad de ascenso del corredor)
  runnerSpeed = max(35UL, 120UL - (score * 5UL));

  // 1. Movimiento del corredor
  if (!isStoppedAtGate) {
    if (now - lastRunnerMove >= runnerSpeed) {
      lastRunnerMove = now;
      runnerPosition += 1.0;

      // Llegar a la cima (Victoria de piso)
      if (runnerPosition >= PLAY_AREA_END) {
        playSound(6); // Sonido Victoria
        score += 5;   // Puntos bonus
        
        // Destello de victoria
        for (int i = 0; i < 4; i++) {
          strip.fill(strip.Color(0, 0, 0, 180));
          strip.show();
          delay(80);
          strip.clear();
          strip.show();
          delay(80);
        }
        
        // Reiniciar posición y barreras
        runnerPosition = PLAY_AREA_START;
        for (int i = 0; i < NUM_GATES; i++) {
          gates[i].color = (BallColor)random(0, COLOR_COUNT);
          gates[i].active = true;
        }
        lastRunnerMove = millis();
        return;
      }

      // Comprobar colisión con barreras activas
      for (int i = 0; i < NUM_GATES; i++) {
        if (gates[i].active && (runnerPosition + 1 >= gates[i].startPos)) {
          // Detener corredor
          isStoppedAtGate = true;
          currentGateIndex = i;
          gateWaitTime = millis();
          Serial.print(F("Corredor detenido en barrera: ")); Serial.println(i);
          break;
        }
      }
    }
  } else {
    // Si se detiene y pasa demasiado tiempo sin pulsar (1.5 segundos) -> Choque/Fallo
    if (now - gateWaitTime > 1500) {
      isStoppedAtGate = false;
      currentState = STATE_MISS;
      return;
    }
  }

  // 2. Comprobar pulsaciones
  if (btnBlue.pressed || btnGreen.pressed || btnRed.pressed) {
    BallColor pressedColor = COLOR_BLUE;
    if (btnGreen.pressed) pressedColor = COLOR_GREEN;
    else if (btnRed.pressed) pressedColor = COLOR_RED;

    if (isStoppedAtGate && currentGateIndex != -1) {
      // Si pulsa el color correcto de la barrera
      if (gates[currentGateIndex].color == pressedColor) {
        gates[currentGateIndex].active = false;
        isStoppedAtGate = false;
        currentState = STATE_HIT;
      } else {
        // Equivocación de botón -> Choque
        isStoppedAtGate = false;
        currentState = STATE_MISS;
      }
    } else {
      // Pulsar sin haber barrera activa adelante (Se penaliza con pequeña caída)
      runnerPosition = max((float)PLAY_AREA_START, runnerPosition - 3.0f);
    }
  }

  // 3. Renderizar Tira LED
  strip.clear();

  // Vidas
  for (int i = 0; i < 3; i++) {
    strip.setPixelColor(i, (i < lives) ? strip.Color(0, 50, 0, 0) : strip.Color(50, 0, 0, 0));
  }
  strip.setPixelColor(3, strip.Color(0, 0, 0, 0));

  // Dibujar barreras activas
  for (int i = 0; i < NUM_GATES; i++) {
    if (gates[i].active) {
      for (int p = gates[i].startPos; p <= gates[i].endPos; p++) {
        strip.setPixelColor(p, getColorRGBW(gates[i].color, 120));
      }
    }
  }

  // Dibujar corredor (Blanco cálido de 3 LEDs)
  int runPos = (int)runnerPosition;
  if (runPos >= PLAY_AREA_START && runPos <= PLAY_AREA_END) {
    strip.setPixelColor(runPos, strip.Color(0, 0, 0, 200)); // Centro blanco puro
    if (runPos - 1 >= PLAY_AREA_START) {
      strip.setPixelColor(runPos - 1, strip.Color(0, 0, 0, 80)); // Cola
    }
    if (runPos - 2 >= PLAY_AREA_START) {
      strip.setPixelColor(runPos - 2, strip.Color(0, 0, 0, 30));
    }
  }

  strip.show();
}
