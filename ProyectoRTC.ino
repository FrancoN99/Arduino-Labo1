#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <DHT.h>
#include "RTClib.h"

// ---------------- RTC DS3231 / ZS-042 ----------------

RTC_DS3231 rtc;

// EEPROM AT24C32 del modulo ZS-042
#define EEPROM_I2C_ADDR 0x57
#define EEPROM_MAGIC_ADDR 0
#define EEPROM_MAGIC_VALUE 123
#define EEPROM_ALARMAS_ADDR 10

// ---------------- DHT11 ----------------

#define DHTPIN 10
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---------------- SENSOR DE LUZ KY-018 ----------------

#define SENSOR_LUZ A0
const int UMBRAL_LUZ = 100;

unsigned long previoMillisLuz = 0;
const long intervaloLuz = 500;
int valorLuz = 0;

// ---------------- BUZZER ----------------

#define BUZZER_PIN 11

// true si tu buzzer esta conectado asi: 5V -> buzzer -> pin 11
// false si esta conectado asi: pin 11 -> buzzer -> GND
const bool BUZZER_INVERTIDO = true;

// ---------------- LCD I2C ----------------

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------------- KEYPAD 4x4 ----------------

const byte FILAS = 4;
const byte COLUMNAS = 4;

char keys[FILAS][COLUMNAS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte pinesFilas[FILAS] = {9, 8, 7, 6};
byte pinesColumnas[COLUMNAS] = {5, 4, 3, 2};

Keypad teclado = Keypad(makeKeymap(keys), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// ---------------- VARIABLES DE HORA ----------------

int horas = 0;
int minutos = 0;
int segundos = 0;

int ultimoSegundoLeido = -1;

// ---------------- VARIABLES DE ALARMAS ----------------

const int MAX_ALARMAS = 5;

int alarmaHoras[MAX_ALARMAS];
int alarmaMinutos[MAX_ALARMAS];
bool alarmaActiva[MAX_ALARMAS];
bool alarmaYaDisparoEsteMinuto[MAX_ALARMAS];

bool alarmaSonando = false;

unsigned long previoMillisBuzzer = 0;
bool estadoBuzzer = false;

// ---------------- VARIABLES DE CLIMA ----------------

float temperatura = 0.0;
float humedad = 0.0;

unsigned long previoMillisDHT = 0;
const long intervaloDHT = 2000;

// ---------------- ESTADOS DEL MENU ----------------

enum Estados { MOSTRAR_RELOJ, MENU_PRINCIPAL, SET_HORA, SET_ALARMA, MOSTRAR_CLIMA };
Estados estadoActual = MOSTRAR_RELOJ;

// ---------------- SETUP ----------------

void setup() {
  Wire.begin();

  lcd.init();
  lcd.backlight();
  lcd.clear();

  Serial.begin(9600);

  dht.begin();

  pinMode(BUZZER_PIN, OUTPUT);
  apagarBuzzer();

  inicializarAlarmasEnRAM();

  if (!rtc.begin()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No detecta RTC");
    lcd.setCursor(0, 1);
    lcd.print("Revisar cables");
    while (1);
  }

  if (!eepromPresente()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No detecta MEM");
    lcd.setCursor(0, 1);
    lcd.print("EEPROM 0x57");
    delay(2000);
  } else {
    cargarAlarmasDesdeEEPROM();
  }

  if (rtc.lostPower()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC sin hora");
    lcd.setCursor(0, 1);
    lcd.print("Configure hora");
    delay(2000);
    lcd.clear();

    estadoActual = SET_HORA;
  } else {
    leerHoraRTC();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC funcionando");
    lcd.setCursor(0, 1);
    lcd.print("A para menu");
    delay(1500);
    lcd.clear();
  }
}

// ---------------- LOOP ----------------

void loop() {
  actualizarHoraDesdeRTC();
  leerSensorDHT();
  controlarLuzLCD();

  char tecla = teclado.getKey();

  if (alarmaSonando && tecla != NO_KEY) {
    alarmaSonando = false;
    apagarBuzzer();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Alarma apagada");
    delay(1000);
    lcd.clear();
  }

  gestionarSonidoAlarma();

  if (alarmaSonando) {
    lcd.setCursor(0, 0);
    lcd.print("!! ALARMA !!    ");
    lcd.setCursor(0, 1);
    lcd.print("Toque una tecla ");
    return;
  }

  switch (estadoActual) {

    case MOSTRAR_RELOJ:
      pantallaPrincipal();

      if (tecla == 'A') {
        estadoActual = MENU_PRINCIPAL;
        lcd.clear();
      }
      break;

    case MENU_PRINCIPAL:
      lcd.setCursor(0, 0);
      lcd.print("1:Hora 2:Alarma");
      lcd.setCursor(0, 1);
      lcd.print("3:Clima *:Volver");

      if (tecla == '1') {
        estadoActual = SET_HORA;
        lcd.clear();
      }

      if (tecla == '2') {
        estadoActual = SET_ALARMA;
        lcd.clear();
      }

      if (tecla == '3') {
        estadoActual = MOSTRAR_CLIMA;
        lcd.clear();
      }

      if (tecla == '*') {
        estadoActual = MOSTRAR_RELOJ;
        lcd.clear();
      }
      break;

    case SET_HORA:
      configurarHora();
      break;

    case SET_ALARMA:
      configurarAlarma();
      break;

    case MOSTRAR_CLIMA:
      mostrarClima();

      if (tecla == '*') {
        estadoActual = MENU_PRINCIPAL;
        lcd.clear();
      }
      break;
  }
}

// ---------------- SENSOR DE LUZ Y BACKLIGHT ----------------

void controlarLuzLCD() {
  if (millis() - previoMillisLuz >= intervaloLuz) {
    previoMillisLuz = millis();

    valorLuz = analogRead(SENSOR_LUZ);

    Serial.print("Luz KY-018: ");
    Serial.println(valorLuz);

    if (valorLuz > UMBRAL_LUZ) {
      lcd.noBacklight();
    } else {
      lcd.backlight();
    }
  }
}

// ---------------- RTC ----------------

void leerHoraRTC() {
  DateTime ahora = rtc.now();

  horas = ahora.hour();
  minutos = ahora.minute();
  segundos = ahora.second();
}

void actualizarHoraDesdeRTC() {
  DateTime ahora = rtc.now();

  horas = ahora.hour();
  minutos = ahora.minute();
  segundos = ahora.second();

  if (segundos != ultimoSegundoLeido) {
    ultimoSegundoLeido = segundos;
    revisarAlarmas();
  }
}

void guardarHoraEnRTC(int h, int m, int s) {
  rtc.adjust(DateTime(2026, 6, 9, h, m, s));
}

// ---------------- EEPROM DEL ZS-042 ----------------

bool eepromPresente() {
  Wire.beginTransmission(EEPROM_I2C_ADDR);
  return Wire.endTransmission() == 0;
}

void eepromWriteByte(unsigned int direccion, byte dato) {
  Wire.beginTransmission(EEPROM_I2C_ADDR);
  Wire.write((int)(direccion >> 8));
  Wire.write((int)(direccion & 0xFF));
  Wire.write(dato);
  Wire.endTransmission();

  delay(5);
}

byte eepromReadByte(unsigned int direccion) {
  Wire.beginTransmission(EEPROM_I2C_ADDR);
  Wire.write((int)(direccion >> 8));
  Wire.write((int)(direccion & 0xFF));
  Wire.endTransmission();

  Wire.requestFrom(EEPROM_I2C_ADDR, 1);

  if (Wire.available()) {
    return Wire.read();
  }

  return 0;
}

void inicializarAlarmasEnRAM() {
  for (int i = 0; i < MAX_ALARMAS; i++) {
    alarmaHoras[i] = 0;
    alarmaMinutos[i] = 0;
    alarmaActiva[i] = false;
    alarmaYaDisparoEsteMinuto[i] = false;
  }
}

void cargarAlarmasDesdeEEPROM() {
  byte magic = eepromReadByte(EEPROM_MAGIC_ADDR);

  if (magic != EEPROM_MAGIC_VALUE) {
    inicializarAlarmasEnRAM();
    guardarTodasLasAlarmasEnEEPROM();
    eepromWriteByte(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    return;
  }

  for (int i = 0; i < MAX_ALARMAS; i++) {
    int base = EEPROM_ALARMAS_ADDR + i * 3;

    alarmaActiva[i] = eepromReadByte(base) == 1;
    alarmaHoras[i] = eepromReadByte(base + 1);
    alarmaMinutos[i] = eepromReadByte(base + 2);

    if (alarmaHoras[i] > 23 || alarmaMinutos[i] > 59) {
      alarmaActiva[i] = false;
      alarmaHoras[i] = 0;
      alarmaMinutos[i] = 0;
    }

    alarmaYaDisparoEsteMinuto[i] = false;
  }
}

void guardarAlarmaEnEEPROM(int numeroAlarma) {
  int i = numeroAlarma;

  int base = EEPROM_ALARMAS_ADDR + i * 3;

  eepromWriteByte(base, alarmaActiva[i] ? 1 : 0);
  eepromWriteByte(base + 1, alarmaHoras[i]);
  eepromWriteByte(base + 2, alarmaMinutos[i]);

  eepromWriteByte(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
}

void guardarTodasLasAlarmasEnEEPROM() {
  for (int i = 0; i < MAX_ALARMAS; i++) {
    guardarAlarmaEnEEPROM(i);
  }
}

// ---------------- ALARMAS ----------------

void revisarAlarmas() {
  for (int i = 0; i < MAX_ALARMAS; i++) {

    if (alarmaActiva[i] == true &&
        horas == alarmaHoras[i] &&
        minutos == alarmaMinutos[i] &&
        segundos == 0 &&
        alarmaYaDisparoEsteMinuto[i] == false) {

      alarmaSonando = true;
      alarmaYaDisparoEsteMinuto[i] = true;
    }

    if (horas != alarmaHoras[i] || minutos != alarmaMinutos[i]) {
      alarmaYaDisparoEsteMinuto[i] = false;
    }
  }
}

int cantidadAlarmasActivas() {
  int cantidad = 0;

  for (int i = 0; i < MAX_ALARMAS; i++) {
    if (alarmaActiva[i]) {
      cantidad++;
    }
  }

  return cantidad;
}

// ---------------- BUZZER ----------------

void encenderBuzzer() {
  if (BUZZER_INVERTIDO) {
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    digitalWrite(BUZZER_PIN, HIGH);
  }
}

void apagarBuzzer() {
  if (BUZZER_INVERTIDO) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void gestionarSonidoAlarma() {
  if (alarmaSonando) {
    if (millis() - previoMillisBuzzer >= 300) {
      previoMillisBuzzer = millis();
      estadoBuzzer = !estadoBuzzer;

      if (estadoBuzzer) {
        encenderBuzzer();
      } else {
        apagarBuzzer();
      }
    }
  } else {
    apagarBuzzer();
    estadoBuzzer = false;
  }
}

// ---------------- SENSOR DHT11 ----------------

void leerSensorDHT() {
  if (millis() - previoMillisDHT >= intervaloDHT) {
    previoMillisDHT = millis();

    float nuevaHumedad = dht.readHumidity();
    float nuevaTemperatura = dht.readTemperature();

    if (!isnan(nuevaHumedad) && !isnan(nuevaTemperatura)) {
      humedad = nuevaHumedad;
      temperatura = nuevaTemperatura;
    }
  }
}

// ---------------- PANTALLAS ----------------

void pantallaPrincipal() {
  lcd.setCursor(0, 0);
  lcd.print("Reloj: ");

  if (horas < 10) lcd.print("0");
  lcd.print(horas);
  lcd.print(":");

  if (minutos < 10) lcd.print("0");
  lcd.print(minutos);
  lcd.print(":");

  if (segundos < 10) lcd.print("0");
  lcd.print(segundos);
  lcd.print("  ");

  lcd.setCursor(0, 1);

  int cant = cantidadAlarmasActivas();

  if (cant > 0) {
    lcd.print("A:Menu ALM:");
    lcd.print(cant);
    lcd.print("   ");
  } else {
    lcd.print("A para Menu    ");
  }
}

void mostrarClima() {
  lcd.setCursor(0, 0);

  if (isnan(temperatura) || isnan(humedad)) {
    lcd.print("Error sensor DHT");
    lcd.setCursor(0, 1);
    lcd.print("                ");
  } else {
    lcd.print("Temp: ");
    lcd.print(temperatura, 1);
    lcd.print(" C    ");

    lcd.setCursor(0, 1);
    lcd.print("Humedad: ");
    lcd.print(humedad, 0);
    lcd.print("%    ");
  }
}

// ---------------- CONFIGURAR HORA ----------------

void configurarHora() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ingresa Hora HH");

  int nuevaHora = ingresarValorDosDigitos();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ingresa Min MM");

  int nuevosMin = ingresarValorDosDigitos();

  if (nuevaHora >= 0 && nuevaHora < 24 && nuevosMin >= 0 && nuevosMin < 60) {
    horas = nuevaHora;
    minutos = nuevosMin;
    segundos = 0;

    guardarHoraEnRTC(horas, minutos, segundos);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hora guardada");
    lcd.setCursor(0, 1);
    lcd.print("en RTC");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hora invalida");
  }

  delay(1500);
  estadoActual = MOSTRAR_RELOJ;
  lcd.clear();
}

// ---------------- CONFIGURAR ALARMA ----------------

void configurarAlarma() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Alarma nro 1-5");
  lcd.setCursor(0, 1);
  lcd.print("* cancelar");

  int numero = ingresarNumeroAlarma();

  if (numero == -1) {
    estadoActual = MENU_PRINCIPAL;
    lcd.clear();
    return;
  }

  int indice = numero - 1;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("A");
  lcd.print(numero);
  lcd.print(" Hora HH");

  int hh = ingresarValorDosDigitos();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("A");
  lcd.print(numero);
  lcd.print(" Min MM");

  int mm = ingresarValorDosDigitos();

  if (hh >= 0 && hh < 24 && mm >= 0 && mm < 60) {
    alarmaHoras[indice] = hh;
    alarmaMinutos[indice] = mm;
    alarmaActiva[indice] = true;
    alarmaYaDisparoEsteMinuto[indice] = false;

    alarmaSonando = false;
    apagarBuzzer();

    guardarAlarmaEnEEPROM(indice);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Alarma ");
    lcd.print(numero);
    lcd.print(" guardada");

    lcd.setCursor(0, 1);

    if (hh < 10) lcd.print("0");
    lcd.print(hh);
    lcd.print(":");

    if (mm < 10) lcd.print("0");
    lcd.print(mm);

  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Datos invalidos");
  }

  delay(1500);
  estadoActual = MENU_PRINCIPAL;
  lcd.clear();
}

// ---------------- INGRESO DE DATOS ----------------

int ingresarNumeroAlarma() {
  while (true) {
    char t = teclado.getKey();

    if (t >= '1' && t <= '5') {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Elegida alarma:");
      lcd.setCursor(0, 1);
      lcd.print(t);
      delay(700);
      return t - '0';
    }

    if (t == '*') {
      return -1;
    }
  }
}

int ingresarValorDosDigitos() {
  String valor = "";

  lcd.setCursor(0, 1);
  lcd.print("__              ");
  lcd.setCursor(0, 1);

  while (valor.length() < 2) {
    char t = teclado.getKey();

    if (t >= '0' && t <= '9') {
      valor += t;
      lcd.print(t);
    }
  }

  return valor.toInt();
}