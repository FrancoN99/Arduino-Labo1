

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


* UNSAM
* Laboratorio de computacion I
* Pedro, Rodrigo y Federico
* 2026
