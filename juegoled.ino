/**
 * ==================================================================================
 * PROYECTO: CONSOLA DE 4 JUEGOS ARCADE LED VERTICAL
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

#define PIN_LED      13  // Pin de datos para la tira SK6812 (con resistencia 330Ω)
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
int selectedGame = 1; // 1: Defender, 2: Runner, 3: Stacker, 4: Simon
int score = 0;
int highScoreGame1 = 0;
int highScoreGame2 = 0;
int highScoreGame3 = 0;
int highScoreGame4 = 0;
int lives = 3;

// --- VARIABLES ESPECÍFICAS DE JUEGOS ---

// Juego 1: Defender
#define MAX_METEORITES 3
#define SHIELD_START 10
#define SHIELD_END 18
Meteorite meteorites[MAX_METEORITES];
unsigned long lastMeteoriteSpawn = 0;
unsigned long lastMeteoriteMove = 0;
unsigned long spawnInterval = 2500;
unsigned long meteoriteSpeed = 100;

// Juego 2: Runner
#define NUM_GATES 3
Gate gates[NUM_GATES];
float runnerPosition = PLAY_AREA_START;
unsigned long lastRunnerMove = 0;
unsigned long runnerSpeed = 120;
bool isStoppedAtGate = false;
int currentGateIndex = -1;
unsigned long gateWaitTime = 0;

// Juego 3: LED Stacker
int targetStart = 55;
int targetEnd = 65; // Ancho inicial 11 LEDs en el centro
int blockWidth = 11;
float stackerPosition = PLAY_AREA_START;
bool stackerDirectionUp = true;
unsigned long lastStackerMove = 0;
unsigned long stackerSpeed = 60; // ms por paso

// Juego 4: Simon Says
#define SIMON_MAX_LENGTH 32
enum SimonSubState {
  SIMON_SHOW,
  SIMON_INPUT
};
SimonSubState simonState = SIMON_SHOW;
int simonSequence[SIMON_MAX_LENGTH];
int simonLength = 0;
int simonIndex = 0;
unsigned long simonTimer = 0;
int simonShowIndex = 0;
bool simonShowOn = false;

// Variables generales de timers
unsigned long stateTimer = 0;
bool hasAudio = false;

// Prototipos
void updateButton(Button &btn);
void playSound(int track);
void drawGame();
uint32_t getColorRGBW(BallColor color, uint8_t brightness);

// Inicializadores
void initGame1();
void initGame2();
void initGame3();
void initGame4();
void spawnMeteorite();
void handleGame1Playing();
void handleGame2Playing();
void handleGame3Playing();
void handleGame4Playing();
void showSimonSequence();

void setup() {
  Serial.begin(115200);
  Serial.println(F("--- Iniciando Consola Multi-Juegos 4-en-1 ---"));

  // Configurar botones
  pinMode(btnBlue.pin, INPUT_PULLUP);
  pinMode(btnGreen.pin, INPUT_PULLUP);
  pinMode(btnRed.pin, INPUT_PULLUP);

  // Inicializar tira LED
  strip.begin();
  strip.show();

  // Cargar Récords
  preferences.begin("consolaled", false);
  highScoreGame1 = preferences.getInt("highscore1", 0);
  highScoreGame2 = preferences.getInt("highscore2", 0);
  highScoreGame3 = preferences.getInt("highscore3", 0);
  highScoreGame4 = preferences.getInt("highscore4", 0);

  // Inicializar MP3
  Serial2.begin(9600, SERIAL_8N1, MP3_RX, MP3_TX);
  if (myDFPlayer.begin(Serial2)) {
    hasAudio = true;
    myDFPlayer.volume(18);
    delay(500);
    playSound(1); // Melodía del menú principal
  } else {
    Serial.println(F("ADVERTENCIA: Módulo MP3 no detectado."));
  }

  randomSeed(analogRead(0));
}

void loop() {
  updateButton(btnBlue);
  updateButton(btnGreen);
  updateButton(btnRed);

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
      else if (selectedGame == 3) handleGame3Playing();
      else if (selectedGame == 4) handleGame4Playing();
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

// --- ACTUALIZACIÓN DE BOTONES CON DEBOUNCE ---
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

// --- RETORNAR COLOR RGBW ---
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
// CONTROLADOR DE MENÚ PRINCIPAL NAVEGABLE (3 BOTONES)
// ==================================================================================

void handleMenu() {
  static unsigned long lastMenuTick = 0;
  static int pulseVal = 0;
  static bool ascending = true;

  // Botón Azul = Subir cursor
  if (btnBlue.pressed) {
    selectedGame--;
    if (selectedGame < 1) selectedGame = 4;
    playSound(10); // Tono de navegación
  }
  // Botón Rojo = Bajar cursor
  if (btnRed.pressed) {
    selectedGame++;
    if (selectedGame > 4) selectedGame = 1;
    playSound(12); // Tono de navegación
  }
  // Botón Verde = Seleccionar/Confirmar
  if (btnGreen.pressed) {
    playSound(11); // Tono de confirmación
    currentState = STATE_START;
    return;
  }

  // Animación del menú
  if (millis() - lastMenuTick > 20) {
    lastMenuTick = millis();
    if (ascending) {
      pulseVal += 4;
      if (pulseVal >= 130) ascending = false;
    } else {
      pulseVal -= 4;
      if (pulseVal <= 20) ascending = true;
    }

    strip.clear();
    
    // LEDs de vidas fijos
    strip.setPixelColor(0, strip.Color(0, 0, 0, 15));
    strip.setPixelColor(1, strip.Color(0, 0, 0, 15));
    strip.setPixelColor(2, strip.Color(0, 0, 0, 15));

    // Juego 1: LEDs 15-25 (Azul)
    int j1Val = (selectedGame == 1) ? pulseVal : 10;
    for (int i = 15; i <= 25; i++) strip.setPixelColor(i, strip.Color(0, 0, j1Val, 0));

    // Juego 2: LEDs 45-55 (Verde)
    int j2Val = (selectedGame == 2) ? pulseVal : 10;
    for (int i = 45; i <= 55; i++) strip.setPixelColor(i, strip.Color(0, j2Val, 0, 0));

    // Juego 3: LEDs 75-85 (Rojo)
    int j3Val = (selectedGame == 3) ? pulseVal : 10;
    for (int i = 75; i <= 85; i++) strip.setPixelColor(i, strip.Color(j3Val, 0, 0, 0));

    // Juego 4: LEDs 105-115 (Magenta)
    int j4Val = (selectedGame == 4) ? pulseVal : 10;
    for (int i = 105; i <= 115; i++) strip.setPixelColor(i, strip.Color(j4Val, 0, j4Val, 0));

    strip.show();
  }
}

// 2. INICIO DE JUEGO
void handleStart() {
  uint32_t flashCol = strip.Color(0, 0, 0, 120);
  if (selectedGame == 1) flashCol = strip.Color(0, 0, 150, 0);
  else if (selectedGame == 2) flashCol = strip.Color(0, 150, 0, 0);
  else if (selectedGame == 3) flashCol = strip.Color(150, 0, 0, 0);
  else if (selectedGame == 4) flashCol = strip.Color(150, 0, 150, 0);

  // Destello de inicio
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

  if (selectedGame == 1) initGame1();
  else if (selectedGame == 2) initGame2();
  else if (selectedGame == 3) initGame3();
  else if (selectedGame == 4) initGame4();

  playSound(1); // Reproducir melodía ambiental
  currentState = STATE_PLAYING;
}

// 3. ACIERTO (HIT)
void handleHit() {
  score++;

  if (selectedGame == 1) {
    playSound(2);
    currentState = STATE_PLAYING;
  } 
  else if (selectedGame == 2) {
    playSound(4);
    runnerPosition += 4.0;
    if (runnerPosition > PLAY_AREA_END) runnerPosition = PLAY_AREA_END;
    currentState = STATE_PLAYING;
  } 
  else if (selectedGame == 3) {
    playSound(7); // Encaje
    currentState = STATE_PLAYING;
  } 
  else if (selectedGame == 4) {
    playSound(13); // Nivel de Simon superado
    currentState = STATE_PLAYING;
  }
}

// 4. FALLO / DAÑO (MISS)
void handleMiss() {
  lives--;

  if (selectedGame == 1) playSound(3);
  else if (selectedGame == 2) {
    playSound(5);
    runnerPosition = max((float)PLAY_AREA_START, runnerPosition - 15.0f);
  }
  else if (selectedGame == 3) playSound(8); // Corte/Miss
  else if (selectedGame == 4) playSound(14); // Simon Fallo

  // Destello rojo de daño
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
    
    // Asignación de sonidos de derrota y guardado de récords
    if (selectedGame == 1) {
      playSound(3);
      if (score > highScoreGame1) { highScoreGame1 = score; preferences.putInt("highscore1", highScoreGame1); isNewRecord = true; }
    } else if (selectedGame == 2) {
      playSound(5);
      if (score > highScoreGame2) { highScoreGame2 = score; preferences.putInt("highscore2", highScoreGame2); isNewRecord = true; }
    } else if (selectedGame == 3) {
      playSound(8);
      if (score > highScoreGame3) { highScoreGame3 = score; preferences.putInt("highscore3", highScoreGame3); isNewRecord = true; }
    } else if (selectedGame == 4) {
      playSound(14);
      if (score > highScoreGame4) { highScoreGame4 = score; preferences.putInt("highscore4", highScoreGame4); isNewRecord = true; }
    }
  }

  strip.clear();

  // Barra dorada de score
  int fillLeds = min(score, 100);
  for (int i = 0; i < fillLeds; i++) {
    strip.setPixelColor(i + 10, strip.Color(100, 80, 0, 0));
  }

  // Mostrar marca de récord
  int recVal = 0;
  if (selectedGame == 1) recVal = highScoreGame1;
  else if (selectedGame == 2) recVal = highScoreGame2;
  else if (selectedGame == 3) recVal = highScoreGame3;
  else if (selectedGame == 4) recVal = highScoreGame4;

  int recPos = min(recVal, 100) + 10;
  if ((millis() / 300) % 2 == 0) {
    strip.setPixelColor(recPos, strip.Color(120, 0, 120, 0)); // Magenta
  }

  // Vidas en rojo
  for (int i = 0; i < 3; i++) {
    strip.setPixelColor(i, strip.Color(120, 0, 0, 0));
  }

  strip.show();

  if (btnBlue.pressed || btnGreen.pressed || btnRed.pressed) {
    playSound(1);
    saved = false;
    isNewRecord = false;
    currentState = STATE_MENU;
    delay(400);
  }
}


// ==================================================================================
// JUEGO 1: COLOR SHIELD DEFENDER
// ==================================================================================

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
      break;
    }
  }
}

void handleGame1Playing() {
  unsigned long now = millis();

  spawnInterval = max(1000UL, 2500UL - (score * 80UL));
  meteoriteSpeed = max(35UL, 100UL - (score * 3UL));

  if (now - lastMeteoriteSpawn >= spawnInterval) {
    lastMeteoriteSpawn = now;
    spawnMeteorite();
  }

  if (now - lastMeteoriteMove >= meteoriteSpeed) {
    lastMeteoriteMove = now;
    for (int i = 0; i < MAX_METEORITES; i++) {
      if (meteorites[i].active) {
        meteorites[i].position -= 1.0;
        if (meteorites[i].position < PLAY_AREA_START) {
          meteorites[i].active = false;
          currentState = STATE_MISS;
          return;
        }
      }
    }
  }

  if (btnBlue.pressed || btnGreen.pressed || btnRed.pressed) {
    BallColor pressedColor = COLOR_BLUE;
    if (btnGreen.pressed) pressedColor = COLOR_GREEN;
    else if (btnRed.pressed) pressedColor = COLOR_RED;

    for (int i = 0; i < MAX_METEORITES; i++) {
      if (meteorites[i].active && meteorites[i].color == pressedColor) {
        int pos = (int)meteorites[i].position;
        if (pos >= SHIELD_START && pos <= SHIELD_END) {
          meteorites[i].active = false;
          currentState = STATE_HIT;
          
          // Explosión
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

  strip.clear();

  for (int i = 0; i < 3; i++) {
    strip.setPixelColor(i, (i < lives) ? strip.Color(0, 50, 0, 0) : strip.Color(50, 0, 0, 0));
  }
  strip.setPixelColor(3, strip.Color(0, 0, 0, 0));

  for (int i = SHIELD_START; i <= SHIELD_END; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0, 20));
  }

  for (int i = 0; i < MAX_METEORITES; i++) {
    if (meteorites[i].active) {
      int pos = (int)meteorites[i].position;
      strip.setPixelColor(pos, getColorRGBW(meteorites[i].color, 220));
      if (pos + 1 <= PLAY_AREA_END) {
        strip.setPixelColor(pos + 1, getColorRGBW(meteorites[i].color, 60));
      }
    }
  }

  strip.show();
}


// ==================================================================================
// JUEGO 2: COLOR DASH RUNNER
// ==================================================================================

void initGame2() {
  runnerPosition = PLAY_AREA_START;
  runnerSpeed = 120;
  isStoppedAtGate = false;
  lastRunnerMove = millis();

  gates[0] = {35, 37, COLOR_BLUE, true};
  gates[1] = {65, 67, COLOR_GREEN, true};
  gates[2] = {95, 97, COLOR_RED, true};

  for (int i = 0; i < NUM_GATES; i++) {
    gates[i].color = (BallColor)random(0, COLOR_COUNT);
    gates[i].active = true;
  }
}

void handleGame2Playing() {
  unsigned long now = millis();
  runnerSpeed = max(35UL, 120UL - (score * 5UL));

  if (!isStoppedAtGate) {
    if (now - lastRunnerMove >= runnerSpeed) {
      lastRunnerMove = now;
      runnerPosition += 1.0;

      if (runnerPosition >= PLAY_AREA_END) {
        playSound(6); // Victoria
        score += 5;
        for (int i = 0; i < 4; i++) {
          strip.fill(strip.Color(0, 0, 0, 180));
          strip.show();
          delay(80);
          strip.clear();
          strip.show();
          delay(80);
        }
        
        runnerPosition = PLAY_AREA_START;
        for (int i = 0; i < NUM_GATES; i++) {
          gates[i].color = (BallColor)random(0, COLOR_COUNT);
          gates[i].active = true;
        }
        lastRunnerMove = millis();
        return;
      }

      for (int i = 0; i < NUM_GATES; i++) {
        if (gates[i].active && (runnerPosition + 1 >= gates[i].startPos)) {
          isStoppedAtGate = true;
          currentGateIndex = i;
          gateWaitTime = now;
          break;
        }
      }
    }
  } else {
    if (now - gateWaitTime > 1500) {
      isStoppedAtGate = false;
      currentState = STATE_MISS;
      return;
    }
  }

  if (btnBlue.pressed || btnGreen.pressed || btnRed.pressed) {
    BallColor pressedColor = COLOR_BLUE;
    if (btnGreen.pressed) pressedColor = COLOR_GREEN;
    else if (btnRed.pressed) pressedColor = COLOR_RED;

    if (isStoppedAtGate && currentGateIndex != -1) {
      if (gates[currentGateIndex].color == pressedColor) {
        gates[currentGateIndex].active = false;
        isStoppedAtGate = false;
        currentState = STATE_HIT;
      } else {
        isStoppedAtGate = false;
        currentState = STATE_MISS;
      }
    } else {
      runnerPosition = max((float)PLAY_AREA_START, runnerPosition - 3.0f);
    }
  }

  strip.clear();

  for (int i = 0; i < 3; i++) {
    strip.setPixelColor(i, (i < lives) ? strip.Color(0, 50, 0, 0) : strip.Color(50, 0, 0, 0));
  }
  strip.setPixelColor(3, strip.Color(0, 0, 0, 0));

  for (int i = 0; i < NUM_GATES; i++) {
    if (gates[i].active) {
      for (int p = gates[i].startPos; p <= gates[i].endPos; p++) {
        strip.setPixelColor(p, getColorRGBW(gates[i].color, 120));
      }
    }
  }

  int runPos = (int)runnerPosition;
  if (runPos >= PLAY_AREA_START && runPos <= PLAY_AREA_END) {
    strip.setPixelColor(runPos, strip.Color(0, 0, 0, 200));
    if (runPos - 1 >= PLAY_AREA_START) strip.setPixelColor(runPos - 1, strip.Color(0, 0, 0, 80));
    if (runPos - 2 >= PLAY_AREA_START) strip.setPixelColor(runPos - 2, strip.Color(0, 0, 0, 30));
  }

  strip.show();
}


// ==================================================================================
// JUEGO 3: LED STACKER
// ==================================================================================

void initGame3() {
  targetStart = 52;
  targetEnd = 62; // Ancho inicial de 11 LEDs
  blockWidth = 11;
  stackerPosition = PLAY_AREA_START;
  stackerDirectionUp = true;
  stackerSpeed = 65;
  lastStackerMove = millis();
}

void handleGame3Playing() {
  unsigned long now = millis();

  // Velocidad de movimiento del bloque oscilante
  stackerSpeed = max(25UL, 65UL - (score * 3UL));

  // 1. Movimiento del bloque de arriba a abajo
  if (now - lastStackerMove >= stackerSpeed) {
    lastStackerMove = now;
    if (stackerDirectionUp) {
      stackerPosition += 1.0;
      if (stackerPosition + blockWidth - 1 >= PLAY_AREA_END) {
        stackerDirectionUp = false;
      }
    } else {
      stackerPosition -= 1.0;
      if (stackerPosition <= PLAY_AREA_START) {
        stackerDirectionUp = true;
      }
    }
  }

  // 2. Comprobar disparo/fijación (Botón Verde)
  // El botón verde fija la posición de la barra
  if (btnBlue.pressed || btnGreen.pressed || btnRed.pressed) {
    int stopStart = (int)stackerPosition;
    int stopEnd = stopStart + blockWidth - 1;

    // Calcular solapamiento con el objetivo [targetStart, targetEnd]
    int newStart = max(stopStart, targetStart);
    int newEnd = min(stopEnd, targetEnd);

    if (newEnd >= newStart) {
      // ¡Acierto! Se actualiza la zona de apilado
      targetStart = newStart;
      targetEnd = newEnd;
      blockWidth = targetEnd - targetStart + 1;

      // Destello de acierto en el bloque encajado
      for (int f = 0; f < 3; f++) {
        for (int p = targetStart; p <= targetEnd; p++) {
          strip.setPixelColor(p, (f % 2 == 0) ? strip.Color(120, 100, 0, 0) : strip.Color(0, 0, 0, 0));
        }
        strip.show();
        delay(40);
      }

      // Comprobar victoria (Bloque muy pequeño o llegó a dificultad extrema)
      if (blockWidth <= 1 || score >= 20) {
        playSound(9); // Fanfarria Victoria Stacker
        // Destello de victoria total
        for (int f = 0; f < 5; f++) {
          strip.fill(strip.Color(0, 0, 0, 200));
          strip.show();
          delay(80);
          strip.clear();
          strip.show();
          delay(80);
        }
        currentState = STATE_GAMEOVER;
        return;
      }

      currentState = STATE_HIT;
    } else {
      // ¡Fallo! No hubo coincidencia (la barra se cayó completa)
      currentState = STATE_MISS;
      if (lives <= 0) return;
    }
  }

  // 3. Renderizar tira LED
  strip.clear();

  // Vidas
  for (int i = 0; i < 3; i++) {
    strip.setPixelColor(i, (i < lives) ? strip.Color(0, 50, 0, 0) : strip.Color(50, 0, 0, 0));
  }
  strip.setPixelColor(3, strip.Color(0, 0, 0, 0));

  // Dibujar base objetivo (en color amarillo tenue)
  for (int i = targetStart; i <= targetEnd; i++) {
    strip.setPixelColor(i, strip.Color(25, 20, 0, 0));
  }

  // Dibujar bloque en movimiento (Rojo brillante)
  int blockPos = (int)stackerPosition;
  for (int i = 0; i < blockWidth; i++) {
    int p = blockPos + i;
    if (p >= PLAY_AREA_START && p <= PLAY_AREA_END) {
      strip.setPixelColor(p, strip.Color(150, 0, 0, 0));
    }
  }

  strip.show();
}


// ==================================================================================
// JUEGO 4: SIMON LED PATTERN
// ==================================================================================

void initGame4() {
  simonLength = 1;
  simonIndex = 0;
  simonState = SIMON_SHOW;
  simonTimer = millis();
  simonShowIndex = 0;
  simonShowOn = false;

  // Rellenar secuencia inicial
  for (int i = 0; i < SIMON_MAX_LENGTH; i++) {
    simonSequence[i] = random(0, 3); // 0: Blue, 1: Green, 2: Red
  }
}

void handleGame4Playing() {
  unsigned long now = millis();

  if (simonState == SIMON_SHOW) {
    // Velocidad de la secuencia según la ronda (cada vez más rápido)
    unsigned long showSpeed = max(150UL, 400UL - (simonLength * 12UL));
    unsigned long offSpeed = max(70UL, 180UL - (simonLength * 6UL));

    if (simonShowOn) {
      if (now - simonTimer >= showSpeed) {
        simonShowOn = false;
        simonTimer = now;
        simonShowIndex++;
        if (simonShowIndex >= simonLength) {
          simonState = SIMON_INPUT;
          simonIndex = 0;
          Serial.println(F("Simon: Esperando entrada del jugador"));
        }
      }
    } else {
      if (now - simonTimer >= offSpeed && simonShowIndex < simonLength) {
        simonShowOn = true;
        simonTimer = now;
        // Reproducir sonido e iluminar tira
        BallColor color = (BallColor)simonSequence[simonShowIndex];
        playSound(10 + color); // Sonidos 10 (Blue), 11 (Green), 12 (Red)
      }
    }

    // Renderizar secuencia Simon
    strip.clear();
    
    // Vidas
    for (int i = 0; i < 3; i++) {
      strip.setPixelColor(i, (i < lives) ? strip.Color(0, 50, 0, 0) : strip.Color(50, 0, 0, 0));
    }

    if (simonShowOn && simonShowIndex < simonLength) {
      BallColor col = (BallColor)simonSequence[simonShowIndex];
      // Iluminar toda la zona de juego con el color correspondiente
      for (int p = PLAY_AREA_START; p <= PLAY_AREA_END; p++) {
        strip.setPixelColor(p, getColorRGBW(col, 100));
      }
    }
    strip.show();

  } else {
    // MODO ENTRADA JUGADOR (INPUT)
    if (btnBlue.pressed || btnGreen.pressed || btnRed.pressed) {
      BallColor pressedColor = COLOR_BLUE;
      if (btnGreen.pressed) pressedColor = COLOR_GREEN;
      else if (btnRed.pressed) pressedColor = COLOR_RED;

      // Destello acústico y visual de la tecla presionada
      playSound(10 + pressedColor);
      strip.clear();
      for (int i = 0; i < 3; i++) {
        strip.setPixelColor(i, (i < lives) ? strip.Color(0, 50, 0, 0) : strip.Color(50, 0, 0, 0));
      }
      for (int p = PLAY_AREA_START; p <= PLAY_AREA_END; p++) {
        strip.setPixelColor(p, getColorRGBW(pressedColor, 120));
      }
      strip.show();
      delay(120);

      // Comprobar coincidencia
      if (pressedColor == simonSequence[simonIndex]) {
        simonIndex++;
        if (simonIndex >= simonLength) {
          // Ronda superada
          currentState = STATE_HIT;
          simonLength++;
          if (simonLength >= SIMON_MAX_LENGTH) {
            // Completado al máximo
            currentState = STATE_GAMEOVER;
            return;
          }
          simonState = SIMON_SHOW;
          simonShowIndex = 0;
          simonShowOn = false;
          simonTimer = millis() + 600; // Breve pausa antes de mostrar la secuencia
        }
      } else {
        // Falló la secuencia
        currentState = STATE_MISS;
        if (lives > 0) {
          // Repetir secuencia desde el inicio de la misma ronda
          simonState = SIMON_SHOW;
          simonShowIndex = 0;
          simonShowOn = false;
          simonTimer = millis() + 800;
        }
      }
    }

    // Dibujar en espera de entrada
    strip.clear();
    for (int i = 0; i < 3; i++) {
      strip.setPixelColor(i, (i < lives) ? strip.Color(0, 50, 0, 0) : strip.Color(50, 0, 0, 0));
    }
    // Encender zona objetivo tenuemente en blanco
    for (int p = 58; p <= 68; p++) {
      strip.setPixelColor(p, strip.Color(0, 0, 0, 10));
    }
    strip.show();
  }
}
