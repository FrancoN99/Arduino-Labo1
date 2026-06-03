#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "RTClib.h"

// LCD I2C
// Si no funciona con 0x27, probar con 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Modulo RTC ZS-042 / DS3231
RTC_DS3231 rtc;

// Configuracion del keypad 4x4
const byte FILAS = 4;
const byte COLUMNAS = 4;

char teclas[FILAS][COLUMNAS] =
{
  {'D', 'C', 'A', 'B'},
  {'#', '9', '3', '6'},
  {'0', '8', '2', '5'},
  {'*', '7', '1', '4'}
};

byte pinesFilas[FILAS] = {9, 8, 7, 6};
byte pinesColumnas[COLUMNAS] = {5, 4, 2, 3};

Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// Para cargar la hora
String horaIngresada = "";
bool pedirNuevaHora = false;

void setup()
{
  Wire.begin();

  lcd.init();
  lcd.backlight();
  lcd.clear();

  if (!rtc.begin())
  {
    lcd.setCursor(0, 0);
    lcd.print("No detecta RTC");
    lcd.setCursor(0, 1);
    lcd.print("Revisar cables");
    while (1);
  }

  /*
    Si el RTC perdio la hora, pide que la cargues.
    Esto puede pasar si no tiene pila o si la pila esta gastada.
  */
  if (rtc.lostPower())
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC sin hora");
    lcd.setCursor(0, 1);
    lcd.print("Cargar hora");
    delay(1500);

    pedirNuevaHora = true;
    pedirHora();
  }
  else
  {
    pedirNuevaHora = false;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hora RTC lista");
    delay(1000);
    lcd.clear();
  }
}

void loop()
{
  if (pedirNuevaHora == true)
  {
    leerTecladoParaHora();
  }
  else
  {
    mostrarReloj();

    char tecla = teclado.getKey();

    // Si apretas * mientras el reloj funciona, podes cambiar la hora
    if (tecla == '*')
    {
      pedirNuevaHora = true;
      horaIngresada = "";
      pedirHora();
    }
  }
}

void pedirHora()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ingrese HHMMSS");
  lcd.setCursor(0, 1);
  lcd.print("Luego #");
}

void leerTecladoParaHora()
{
  char tecla = teclado.getKey();

  if (tecla)
  {
    // Si toca un numero
    if (tecla >= '0' && tecla <= '9')
    {
      if (horaIngresada.length() < 6)
      {
        horaIngresada += tecla;
        mostrarHoraIngresada();
      }
    }

    // Borrar con *
    if (tecla == '*')
    {
      horaIngresada = "";
      pedirHora();
    }

    // Confirmar con #
    if (tecla == '#')
    {
      if (horaIngresada.length() == 6)
      {
        cargarHoraEnRTC();
      }
      else
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Faltan numeros");
        lcd.setCursor(0, 1);
        lcd.print("Use HHMMSS");
        delay(1500);

        horaIngresada = "";
        pedirHora();
      }
    }
  }
}

void mostrarHoraIngresada()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hora:");

  lcd.setCursor(0, 1);

  for (int i = 0; i < horaIngresada.length(); i++)
  {
    lcd.print(horaIngresada[i]);

    if (i == 1 || i == 3)
    {
      lcd.print(":");
    }
  }
}

void cargarHoraEnRTC()
{
  int horas = horaIngresada.substring(0, 2).toInt();
  int minutos = horaIngresada.substring(2, 4).toInt();
  int segundos = horaIngresada.substring(4, 6).toInt();

  if (horas > 23 || minutos > 59 || segundos > 59)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hora invalida");
    lcd.setCursor(0, 1);
    lcd.print("Intente de nuevo");
    delay(1500);

    horaIngresada = "";
    pedirHora();
  }
  else
  {
    /*
      Guardamos la hora en el RTC.

      Como vos solo estas ingresando HHMMSS,
      dejamos una fecha fija: 02/06/2026.
      La fecha no importa si solo queres mostrar hora.
    */
    rtc.adjust(DateTime(2026, 6, 2, horas, minutos, segundos));

    pedirNuevaHora = false;
    horaIngresada = "";

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hora guardada");
    lcd.setCursor(0, 1);
    lcd.print("en el RTC");
    delay(1500);

    lcd.clear();
  }
}

void mostrarReloj()
{
  DateTime ahora = rtc.now();

  lcd.setCursor(0, 0);
  lcd.print("Reloj Arduino  ");

  lcd.setCursor(0, 1);

  if (ahora.hour() < 10) lcd.print("0");
  lcd.print(ahora.hour());
  lcd.print(":");

  if (ahora.minute() < 10) lcd.print("0");
  lcd.print(ahora.minute());
  lcd.print(":");

  if (ahora.second() < 10) lcd.print("0");
  lcd.print(ahora.second());

  lcd.print("        ");

  delay(500);
}