# Comunicación Bluetooth: Arduino + HC-06 ↔ ESP32

Guía detallada para comunicar un **Arduino con módulo HC-06** y un **ESP32** por Bluetooth clásico (SPP). Explica qué hace cada parte del código, cómo viajan los datos y cómo resolver el problema más común (mensajes que llegan en una sola dirección).

## Tabla de contenidos

- [Hardware necesario](#hardware-necesario)
- [Arquitectura: quién manda y quién obedece](#arquitectura-quién-manda-y-quién-obedece)
- [Cableado (Arduino ↔ HC-06)](#cableado-arduino--hc-06)
- [Código del ESP32 (maestro)](#código-del-esp32-maestro)
- [Código del Arduino (con HC-06)](#código-del-arduino-con-hc-06)
- [MAC y password del HC-06](#mac-y-password-del-hc-06)
- [Solución de problemas](#solución-de-problemas)
- [Flujo completo](#flujo-completo)

## Hardware necesario

| Componente | Notas |
|---|---|
| ESP32 | Debe ser un ESP32 "clásico" (WROOM-32). **Los S2, S3 y C3 NO sirven** porque solo tienen BLE, no Bluetooth clásico. |
| Arduino | Uno, Nano, Mega, etc. |
| Módulo HC-06 | Solo funciona como esclavo. |
| 2 resistencias | ~1 kΩ y ~2 kΩ para el divisor de voltaje. |
| Cables / protoboard | Para las conexiones. |

## Arquitectura: quién manda y quién obedece

En Bluetooth clásico siempre hay un **maestro** (inicia la conexión) y un **esclavo** (espera ser encontrado).

El punto clave: **el HC-06 solo puede ser esclavo**. Por eso el ESP32 debe ser obligatoriamente el **maestro**.

```text
[ ESP32 ]  ──── busca y se conecta a ────▶  [ HC-06 ] ── [ Arduino ]
 (MAESTRO)                                   (ESCLAVO)
```

> [!NOTE]
> Que el HC-06 sea "esclavo" **solo** significa que no inicia la conexión. Una vez conectados, los datos viajan en **ambas direcciones** por igual.

## Cableado (Arduino ↔ HC-06)

El TX de un lado va al RX del otro (cruzados), **nunca** TX con TX.

| Pin del HC-06 | Va a | Detalle |
|---|---|---|
| `VCC` | 5V del Arduino | Alimentación. |
| `GND` | GND del Arduino | Tierra común (imprescindible). |
| `TXD` | Pin 10 del Arduino (RX) | El módulo envía, el Arduino recibe. |
| `RXD` | Pin 11 del Arduino (TX) | El Arduino envía. **Aquí va el divisor de voltaje.** |

> [!WARNING]
> **Divisor de voltaje:** el pin `RXD` del HC-06 trabaja a 3.3 V, pero el Arduino envía 5 V. Conectar 5 V directo puede dañar el módulo y causar errores. El pin `TXD` del HC-06 **no** necesita divisor.

```text
Pin 11 (TX, 5V) ──[ 1kΩ ]──┬──[ 2kΩ ]── GND
                           │
                    (~3.3V) ──▶ RXD del HC-06
```

## Código del ESP32 (maestro)

```cpp
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Master", true);  // true = modo maestro
  Serial.println("Conectando al HC-06...");

  bool conectado = SerialBT.connect("HC-06");

  if (conectado) {
    Serial.println("¡Conectado!");
  } else {
    Serial.println("No se pudo conectar, revisa el nombre/MAC");
  }
}

void loop() {
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());   // del Arduino -> a la PC
  }
  if (Serial.available()) {
    SerialBT.write(Serial.read());   // de la PC -> al Arduino
  }
}
```

### Explicación línea por línea

| Código | Qué hace |
|---|---|
| `#include "BluetoothSerial.h"` | Carga la librería de Bluetooth clásico (SPP) del ESP32. |
| `BluetoothSerial SerialBT;` | Crea el objeto `SerialBT`, el "canal" por el que se envía y recibe. |
| `Serial.begin(115200);` | Abre la comunicación con la PC por USB (Monitor Serie). No es Bluetooth. |
| `SerialBT.begin("ESP32_Master", true);` | Enciende el Bluetooth. El nombre es libre; el `true` significa **modo maestro** (con `false` sería esclavo y no podría conectar). |
| `SerialBT.connect("HC-06");` | Busca el dispositivo "HC-06" y se conecta. Devuelve `true`/`false`. |
| `if (conectado) {...}` | Informa por el Monitor Serie si la conexión salió bien. |

El `loop()` arma dos puentes de datos:

- **Puente 1:** lo que llega del Arduino → se muestra en la PC.
- **Puente 2:** lo que escribís en la PC → se envía al Arduino.

> [!CAUTION]
> Conectar por nombre es riesgoso: "HC-06" es el nombre de fábrica de **todos** los módulos. Si hay más de uno cerca, el ESP32 puede enlazar el equivocado. Conectar por **MAC** es la forma segura (ver abajo).

## Código del Arduino (con HC-06)

Como los pines 0 y 1 del Arduino los ocupa el USB, se usa `SoftwareSerial` para crear un segundo puerto serie.

```cpp
#include <SoftwareSerial.h>

SoftwareSerial BT(10, 11); // RX, TX

void setup() {
  Serial.begin(9600);
  BT.begin(9600);          // velocidad por defecto del HC-06
}

void loop() {
  if (BT.available()) {
    Serial.write(BT.read());   // del ESP32 -> a la PC
  }
  if (Serial.available()) {
    BT.write(Serial.read());   // de la PC -> al ESP32
  }
}
```

| Código | Qué hace |
|---|---|
| `SoftwareSerial BT(10, 11);` | Crea el puerto `BT`. Pin **10 = RX** (recibe), pin **11 = TX** (envía). |
| `Serial.begin(9600);` | Comunicación con la PC por USB. |
| `BT.begin(9600);` | Comunicación con el HC-06. **9600 es la velocidad de fábrica.** |
| `BT.read()` / `BT.write()` | Leen y escriben datos hacia el HC-06. |

### Ejemplo: enviar la lectura de un sensor

```cpp
void loop() {
  int valor = analogRead(A0);
  BT.print("Sensor: ");
  BT.println(valor);
  delay(1000);
}
```

Envía algo como `Sensor: 512` cada segundo, y el ESP32 lo muestra en su monitor.

## MAC y password del HC-06

Cada HC-06 tiene dos datos de identidad:

- **MAC:** dirección única (ej. `00:14:03:05:59:6B`). Conectar por MAC te enlaza con **tu** módulo exacto.
- **Password (PIN):** normalmente `1234` (a veces `0000`). Se configura **antes** de conectar.

```cpp
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// La MAC de tu HC-06, cada par en hexadecimal y en orden
uint8_t direccionHC06[6] = {0x00, 0x14, 0x03, 0x05, 0x59, 0x6B};

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Master", true);

  SerialBT.setPin("1234");               // PIN del HC-06
  Serial.println("Conectando al HC-06...");

  bool conectado = SerialBT.connect(direccionHC06);

  if (conectado) Serial.println("¡Conectado!");
  else           Serial.println("Fallo. Revisa MAC y PIN.");
}

void loop() {
  if (SerialBT.available()) Serial.write(SerialBT.read());
  if (Serial.available())   SerialBT.write(Serial.read());
}
```

> [!TIP]
> Cada par de la MAC va con prefijo `0x` y en el mismo orden de izquierda a derecha. Si `setPin("1234")` da error al compilar, probá `setPin("1234", 4)`: una de las dos compila según la versión de tu librería.

## Solución de problemas

### Los mensajes solo llegan en una dirección

Recibir y transmitir usan **caminos físicos distintos**. Si una dirección funciona y la otra no, el fallo está solo en la línea de la dirección que falla.

| Dirección | Camino físico | Si falla, revisá... |
|---|---|---|
| ESP32 → Arduino (recibir) | `TXD` HC-06 → pin 10 Arduino | Si funciona, este cable está bien. |
| Arduino → ESP32 (enviar) | Pin 11 Arduino → `RXD` HC-06 | El divisor de voltaje y el cable del pin 11. |

**Pasos para diagnosticar, en orden:**

1. **El divisor de voltaje** (sospechoso #1). Si está mal armado o invertido, la señal llega tan débil que el HC-06 no la entiende. *Prueba rápida:* conectá el pin 11 directo al `RXD` (sin divisor) por un momento; si empieza a llegar, el divisor era el problema.
2. **El cable del pin 11.** Confirmá que va al `RXD` (no al `TXD`) y que no esté flojo.
3. **Cómo probás.** Con el código de eco, escribí en el Monitor Serie con **"Nueva línea"** activada y baudios en **9600**. Para descartar dudas, usá el código del sensor que envía solo.

> [!TIP]
> Regla mental: *"recibo pero no envío"* → revisar la línea de envío (pin 11 → RXD + divisor). *"envío pero no recibo"* → revisar la línea de recepción (TXD → pin 10).

### Llegan caracteres raros / basura

Casi siempre es una **velocidad (baud rate) que no coincide**. Asegurate de que ambos lados usen 9600 con el HC-06.

### El ESP32 no compila con Bluetooth

Verificá que la placa seleccionada en el IDE sea un ESP32 con Bluetooth clásico (WROOM-32). Los modelos **S2, S3 y C3 no lo soportan**.

## Flujo completo

```text
TU PC ↔ (USB) ↔ ESP32 ↔ (Bluetooth) ↔ HC-06 ↔ (pines) ↔ ARDUINO
```

Los datos recorren este camino en **ambos sentidos**. Cada placa usa el mismo trío de funciones, apuntando a su propio canal (`SerialBT` en el ESP32, `BT` en el Arduino):

- `available()` → ¿hay datos para leer?
- `read()` → leé el dato.
- `write()` → enviá el dato.
