Sí, Luna. Te dejo un **README.md listo para copiar y pegar en GitHub**.

# Reloj despertador inteligente con Arduino

Este proyecto consiste en un reloj despertador realizado con Arduino UNO. El sistema permite mostrar la hora actual, configurar alarmas, guardar hasta cinco alarmas en memoria, medir temperatura y humedad ambiente, y controlar automáticamente la luz de fondo del LCD según la iluminación del ambiente.

La hora se mantiene guardada gracias al módulo RTC DS3231 / ZS-042, incluso cuando el Arduino se apaga o se desconecta. Las alarmas se almacenan en la memoria EEPROM AT24C32 incluida en el módulo ZS-042.

## ¿Para qué sirve el proyecto?

El proyecto sirve como un reloj despertador programable con funciones adicionales de monitoreo ambiental.

Permite:

* Mostrar la hora actual en una pantalla LCD.
* Mantener la hora aunque Arduino se apague.
* Configurar hasta cinco alarmas.
* Ver las alarmas activas.
* Reemplazar una alarma existente por otra.
* Borrar alarmas guardadas.
* Hacer sonar un buzzer cuando llega el horario de una alarma.
* Apagar la alarma presionando cualquier tecla.
* Mostrar temperatura y humedad ambiente.
* Apagar automáticamente la luz de fondo del LCD según el valor detectado por un sensor de luz.

## Componentes utilizados

* Arduino UNO
* Pantalla LCD 16x2 con módulo I2C
* Módulo RTC DS3231 / ZS-042
* Memoria EEPROM AT24C32 integrada en el módulo ZS-042
* Teclado matricial 4x4
* Sensor DHT11
* Sensor de luz KY-018
* Buzzer
* Protoboard
* Cables Dupont
* Pila CR2032 para el módulo RTC

## Librerías utilizadas

El código utiliza las siguientes librerías:

```cpp
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <DHT.h>
#include "RTClib.h"
```

### Función de cada librería

* `Wire.h`: permite la comunicación I2C entre Arduino, LCD, RTC y EEPROM.
* `LiquidCrystal_I2C.h`: controla la pantalla LCD 16x2 mediante I2C.
* `Keypad.h`: permite leer las teclas del teclado matricial 4x4.
* `DHT.h`: permite leer temperatura y humedad desde el sensor DHT11.
* `RTClib.h`: permite usar el módulo RTC DS3231.

##

En este proyecto el buzzer está conectado de forma invertida, por eso se activa con `LOW` y se apaga con `HIGH`.

## Funcionamiento general

Al iniciar, Arduino comprueba si el RTC DS3231 está conectado. Si el RTC funciona correctamente, el sistema lee la hora guardada en el módulo.

La hora se muestra en el LCD. Desde la pantalla principal, el usuario puede presionar la tecla `A` para ingresar al menú.

## Menú principal

```text
1: Hora
2: Alarmas
3: Clima
*: Volver
```

### Opción 1: Hora

Permite configurar la hora actual.
La hora ingresada se guarda en el RTC DS3231, por lo que se mantiene aunque Arduino se apague.

### Opción 2: Alarmas

Permite administrar las alarmas.

El menú de alarmas incluye:

```text
1: Guardar
2: Ver
3: Borrar
*: Atrás
```

Desde este menú se puede:

* Guardar una nueva alarma.
* Reemplazar una alarma ya existente.
* Ver las alarmas activas.
* Borrar una alarma.

### Opción 3: Clima

Muestra en pantalla:

* Temperatura ambiente.
* Humedad ambiente.

Estos datos son obtenidos por el sensor DHT11.

## Guardado de alarmas

El sistema permite guardar hasta cinco alarmas.

Cada alarma almacena tres datos:

1. Si la alarma está activa o no.
2. La hora de la alarma.
3. El minuto de la alarma.

Estos datos se guardan en la memoria EEPROM AT24C32 del módulo ZS-042.

Cada alarma utiliza 3 bytes:

```text
Alarma 1 → posiciones 10, 11 y 12
Alarma 2 → posiciones 13, 14 y 15
Alarma 3 → posiciones 16, 17 y 18
Alarma 4 → posiciones 19, 20 y 21
Alarma 5 → posiciones 22, 23 y 24
```

Además, el programa utiliza una posición de memoria para guardar un valor de verificación llamado `magic`, que permite saber si la EEPROM ya fue inicializada.

## Activación de las alarmas

El programa compara constantemente la hora actual del RTC con las alarmas activas.

Una alarma suena cuando:

* La alarma está activa.
* La hora actual coincide con la hora programada.
* Los minutos actuales coinciden con los minutos programados.
* Los segundos son iguales a cero.

Cuando se cumple esa condición, el buzzer comienza a sonar de forma intermitente. Para apagarlo, se puede presionar cualquier tecla del keypad.

## Sensor de luz

El sensor KY-018 mide la iluminación del ambiente y entrega un valor analógico entre 0 y 1023.

Si el valor supera el umbral configurado en el código, se apaga la luz de fondo del LCD:

```cpp
lcd.noBacklight();
```

Si el valor vuelve a estar por debajo del umbral, la luz se enciende nuevamente:

```cpp
lcd.backlight();
```

Esto permite que el LCD se adapte automáticamente a la luz ambiental.

## Sensor DHT11

El sensor DHT11 se utiliza para medir:

* Temperatura.
* Humedad.

El programa lee el sensor cada dos segundos para evitar lecturas demasiado rápidas, ya que el DHT11 es un sensor lento.

## Comunicación I2C

El proyecto utiliza comunicación I2C para conectar varios dispositivos con solo dos cables de datos:

```text
SDA → A4
SCL → A5
```

Los dispositivos I2C utilizados son:

| Dispositivo    | Dirección |
| -------------- | --------- |
| LCD I2C        | 0x27      |
| RTC DS3231     | 0x68      |
| EEPROM AT24C32 | 0x57      |

Aunque comparten los mismos cables, Arduino puede comunicarse con cada uno porque cada dispositivo tiene una dirección diferente.

## Lógica del programa

El código utiliza una máquina de estados para organizar las distintas pantallas del sistema.

Los estados principales son:

```cpp
MOSTRAR_RELOJ
MENU_PRINCIPAL
MENU_ALARMAS
SET_HORA
SET_ALARMA
VER_ALARMAS
BORRAR_ALARMA
MOSTRAR_CLIMA
```

Cada estado representa una pantalla o función diferente. Esto permite que el programa sea más ordenado y fácil de entender.

## Resumen del proyecto

Este proyecto integra distintos módulos electrónicos para crear un sistema de reloj despertador completo. El RTC DS3231 mantiene la hora real, la EEPROM guarda las alarmas, el LCD muestra la información, el keypad permite controlar el menú, el buzzer avisa cuando llega una alarma, el DHT11 mide el clima y el KY-018 controla la luz de fondo.

El resultado es un reloj despertador inteligente, programable y con funciones ambientales.

## Autores

* Nombre del alumno 1
* Nombre del alumno 2
* Nombre del alumno 3

## Institución

* Nombre de la institución
* Materia
* Profesor/a
* Año
