# Proyecto Juego LED Vertical
## Configuración de Hardware

### Controlador Principal

- Placa: ESP32 DevKit V1
- Programación: Arduino IDE
- Monitor Serie: 115200 baudios

---

# Tira LED

## Modelo

- SK6812 RGBW
- Longitud: 2 metros
- Cantidad total de LEDs: 120

## Conexiones

| Señal | ESP32 |
|---------|---------|
| DIN (DATA) | GPIO 13 |
| +5V | Fuente 5V |
| GND | GND común |

## Recomendaciones

- Instalar resistencia de 330 Ω entre GPIO 13 y DIN.
- Compartir GND entre fuente, ESP32 y tira LED.

---

# Botones

Configurados con INPUT_PULLUP.

## Botón Azul

| Señal | ESP32 |
|---------|---------|
| Botón Azul | GPIO 26 |
| Otro terminal | GND |

## Botón Verde

| Señal | ESP32 |
|---------|---------|
| Botón Verde | GPIO 25 |
| Otro terminal | GND |

## Botón Rojo

| Señal | ESP32 |
|---------|---------|
| Botón Rojo | GPIO 12 |
| Otro terminal | GND |

### Nota

GPIO 12 es un pin especial de arranque del ESP32.

---

# Módulo de Audio

## Modelo

MP3-TF-16P V3.0 (Compatible con DFPlayer Mini)

## Conexiones

| MP3-TF-16P | ESP32 |
|------------|---------|
| RX | GPIO 17 (TX2) |
| TX | GPIO 16 (RX2) |
| VCC | 5V |
| GND | GND |

---

# Configuración de Software

```cpp
#define LED_PIN 13
#define NUM_LEDS 120

#define BOTON_AZUL 26
#define BOTON_VERDE 25
#define BOTON_ROJO 12

#define MP3_RX 16
#define MP3_TX 17
```

---

# Librerías Arduino

- Adafruit NeoPixel
- DFRobotDFPlayerMini

---

# Tarjeta MicroSD

/MP3/0001.mp3
/MP3/0002.mp3
/MP3/0003.mp3

---

# Estado Actual

- ESP32 configurada y funcionando.
- Monitor serie operativo.
- Tira SK6812 conectada a GPIO 13.
- Botones definidos.
- MP3-TF-16P definido.
