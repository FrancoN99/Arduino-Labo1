#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <DHT.h>
#include "RTClib.h"

// =====================================================
// RTC DS3231 / ZS-042
// =====================================================

RTC_DS3231 rtc;

// EEPROM AT24C32 del modulo ZS-042
#define EEPROM_I2C_ADDR 0x57
#define EEPROM_MAGIC_ADDR 0
#define EEPROM_MAGIC_VALUE 123
#define EEPROM_ALARMAS_ADDR 10

// =====================================================
// DHT11
// =====================================================

#define DHTPIN 10
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// =====================================================
// SENSOR DE LUZ KY-018
// =====================================================

#define SENSOR_LUZ A0

const int UMBRAL_LUZ = 120;

unsigned long previoMillisLuz = 0;
const unsigned long intervaloLuz = 500;

int valorLuz = 0;

// =====================================================
// BUZZER
// =====================================================

#define BUZZER_PIN 11

/*
  true:
  5V -> buzzer -> pin 11

  false:
  pin 11 -> buzzer -> GND
*/
const bool BUZZER_INVERTIDO = true;

// =====================================================
// LCD I2C
// =====================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// KEYPAD 4x4
// =====================================================

const byte FILAS = 4;
const byte COLUMNAS = 4;

char keys[FILAS][COLUMNAS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pinesFilas[FILAS] = {9, 8, 7, 6};
byte pinesColumnas[COLUMNAS] = {5, 4, 3, 2};

Keypad teclado = Keypad(
  makeKeymap(keys),
  pinesFilas,
  pinesColumnas,
  FILAS,
  COLUMNAS
);

// =====================================================
// VARIABLES DEL RELOJ
// =====================================================

int horas = 0;
int minutos = 0;
int segundos = 0;

int ultimoSegundoLeido = -1;

// =====================================================
// VARIABLES DE LAS ALARMAS
// =====================================================

const int MAX_ALARMAS = 5;

int alarmaHoras[MAX_ALARMAS];
int alarmaMinutos[MAX_ALARMAS];

bool alarmaActiva[MAX_ALARMAS];
bool alarmaYaDisparoEsteMinuto[MAX_ALARMAS];

bool alarmaSonando = false;

unsigned long previoMillisBuzzer = 0;
bool estadoBuzzer = false;

// Índice usado para recorrer las alarmas en la pantalla
int indiceVistaAlarma = -1;

// =====================================================
// VARIABLES DEL DHT11
// =====================================================

float temperatura = NAN;
float humedad = NAN;

unsigned long previoMillisDHT = 0;
const unsigned long intervaloDHT = 2000;

// =====================================================
// ESTADOS DEL MENU
// =====================================================

enum Estados {
  MOSTRAR_RELOJ,
  MENU_PRINCIPAL,
  MENU_ALARMAS,
  SET_HORA,
  SET_ALARMA,
  VER_ALARMAS,
  BORRAR_ALARMA,
  MOSTRAR_CLIMA
};

Estados estadoActual = MOSTRAR_RELOJ;

// =====================================================
// SETUP
// =====================================================

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

  // Comprobar RTC
  if (!rtc.begin()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No detecta RTC");
    lcd.setCursor(0, 1);
    lcd.print("Revisar cables");

    while (true) {
      apagarBuzzer();
    }
  }

  // Comprobar EEPROM del ZS-042
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

  // Si el RTC perdió la hora, obliga a configurarla
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

// =====================================================
// LOOP
// =====================================================

void loop() {
  actualizarHoraDesdeRTC();
  leerSensorDHT();
  controlarLuzLCD();

  char tecla = teclado.getKey();

  // Apagar alarma tocando cualquier tecla
  if (alarmaSonando && tecla != NO_KEY) {
    alarmaSonando = false;
    apagarBuzzer();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Alarma apagada");
    delay(1000);
    lcd.clear();

    tecla = NO_KEY;
  }

  gestionarSonidoAlarma();

  // Pantalla prioritaria mientras suena
  if (alarmaSonando) {
    lcd.setCursor(0, 0);
    lcd.print("!! ALARMA !!   ");
    lcd.setCursor(0, 1);
    lcd.print("Toque una tecla");
    return;
  }

  switch (estadoActual) {

    // -------------------------------------------------
    case MOSTRAR_RELOJ:
      pantallaPrincipal();

      if (tecla == 'A') {
        estadoActual = MENU_PRINCIPAL;
        lcd.clear();
      }
      break;

    // -------------------------------------------------
    case MENU_PRINCIPAL:
      mostrarMenuPrincipal();

      if (tecla == '1') {
        estadoActual = SET_HORA;
        lcd.clear();
      }

      if (tecla == '2') {
        estadoActual = MENU_ALARMAS;
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

    // -------------------------------------------------
    case MENU_ALARMAS:
      mostrarMenuAlarmas();

      if (tecla == '1') {
        estadoActual = SET_ALARMA;
        lcd.clear();
      }

      if (tecla == '2') {
        abrirVistaAlarmas();
      }

      if (tecla == '3') {
        estadoActual = BORRAR_ALARMA;
        lcd.clear();
      }

      if (tecla == '*') {
        estadoActual = MENU_PRINCIPAL;
        lcd.clear();
      }
      break;

    // -------------------------------------------------
    case SET_HORA:
      configurarHora();
      break;

    // -------------------------------------------------
    case SET_ALARMA:
      configurarOReemplazarAlarma();
      break;

    // -------------------------------------------------
    case VER_ALARMAS:
      gestionarVistaAlarmas(tecla);
      break;

    // -------------------------------------------------
    case BORRAR_ALARMA:
      borrarAlarma();
      break;

    // -------------------------------------------------
    case MOSTRAR_CLIMA:
      mostrarClima();

      if (tecla == '*') {
        estadoActual = MENU_PRINCIPAL;
        lcd.clear();
      }
      break;
  }
}

// =====================================================
// MENÚS
// =====================================================

void mostrarMenuPrincipal() {
  lcd.setCursor(0, 0);
  lcd.print("1:Hora 2:Alarmas");

  lcd.setCursor(0, 1);
  lcd.print("3:Clima *:Salir");
}

void mostrarMenuAlarmas() {
  lcd.setCursor(0, 0);
  lcd.print("1:Guardar 2:Ver");

  lcd.setCursor(0, 1);
  lcd.print("3:Borrar *:Atras");
}

// =====================================================
// SENSOR DE LUZ
// =====================================================

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

// =====================================================
// RTC
// =====================================================

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
  DateTime ahora = rtc.now();

  // Conserva la fecha existente y modifica solo la hora
  rtc.adjust(
    DateTime(
      ahora.year(),
      ahora.month(),
      ahora.day(),
      h,
      m,
      s
    )
  );
}

// =====================================================
// EEPROM DEL ZS-042
// =====================================================

bool eepromPresente() {
  Wire.beginTransmission(EEPROM_I2C_ADDR);
  return Wire.endTransmission() == 0;
}

void eepromWriteByte(unsigned int direccion, byte dato) {
  Wire.beginTransmission(EEPROM_I2C_ADDR);

  Wire.write((byte)(direccion >> 8));
  Wire.write((byte)(direccion & 0xFF));
  Wire.write(dato);

  Wire.endTransmission();

  delay(5);
}

byte eepromReadByte(unsigned int direccion) {
  Wire.beginTransmission(EEPROM_I2C_ADDR);

  Wire.write((byte)(direccion >> 8));
  Wire.write((byte)(direccion & 0xFF));

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
    unsigned int base = EEPROM_ALARMAS_ADDR + (i * 3);

    alarmaActiva[i] = eepromReadByte(base) == 1;
    alarmaHoras[i] = eepromReadByte(base + 1);
    alarmaMinutos[i] = eepromReadByte(base + 2);

    if (alarmaHoras[i] > 23 || alarmaMinutos[i] > 59) {
      alarmaActiva[i] = false;
      alarmaHoras[i] = 0;
      alarmaMinutos[i] = 0;

      guardarAlarmaEnEEPROM(i);
    }

    alarmaYaDisparoEsteMinuto[i] = false;
  }
}

void guardarAlarmaEnEEPROM(int indice) {
  unsigned int base = EEPROM_ALARMAS_ADDR + (indice * 3);

  eepromWriteByte(base, alarmaActiva[indice] ? 1 : 0);
  eepromWriteByte(base + 1, alarmaHoras[indice]);
  eepromWriteByte(base + 2, alarmaMinutos[indice]);

  eepromWriteByte(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
}

void guardarTodasLasAlarmasEnEEPROM() {
  for (int i = 0; i < MAX_ALARMAS; i++) {
    guardarAlarmaEnEEPROM(i);
  }
}

// =====================================================
// COMPROBAR ALARMAS
// =====================================================

void revisarAlarmas() {
  for (int i = 0; i < MAX_ALARMAS; i++) {

    if (
      alarmaActiva[i] &&
      horas == alarmaHoras[i] &&
      minutos == alarmaMinutos[i] &&
      segundos == 0 &&
      !alarmaYaDisparoEsteMinuto[i]
    ) {
      alarmaSonando = true;
      alarmaYaDisparoEsteMinuto[i] = true;
    }

    if (
      horas != alarmaHoras[i] ||
      minutos != alarmaMinutos[i]
    ) {
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

// =====================================================
// GUARDAR O REEMPLAZAR ALARMA
// =====================================================

void configurarOReemplazarAlarma() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Elegir alarma");
  lcd.setCursor(0, 1);
  lcd.print("1-5  *:Cancelar");

  int numero = ingresarNumeroAlarma();

  if (numero == -1) {
    estadoActual = MENU_ALARMAS;
    lcd.clear();
    return;
  }

  int indice = numero - 1;

  // Si ya existe, pedir confirmación para reemplazar
  if (alarmaActiva[indice]) {
    lcd.clear();
    lcd.setCursor(0, 0);

    lcd.print("A");
    lcd.print(numero);
    lcd.print(" ");

    imprimirHora(
      alarmaHoras[indice],
      alarmaMinutos[indice]
    );

    lcd.setCursor(0, 1);
    lcd.print("#:Reemp *:Salir");

    char confirmacion = esperarTeclaValida('#', '*');

    if (confirmacion == '*') {
      estadoActual = MENU_ALARMAS;
      lcd.clear();
      return;
    }
  }

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
  lcd.print(" Minuto MM");

  int mm = ingresarValorDosDigitos();

  if (
    hh >= 0 && hh <= 23 &&
    mm >= 0 && mm <= 59
  ) {
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
    imprimirHora(hh, mm);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Datos invalidos");
    lcd.setCursor(0, 1);
    lcd.print("Hora no guardada");
  }

  delay(1500);

  estadoActual = MENU_ALARMAS;
  lcd.clear();
}

// =====================================================
// VER ALARMAS ACTIVAS
// =====================================================

void abrirVistaAlarmas() {
  if (cantidadAlarmasActivas() == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No hay alarmas");
    lcd.setCursor(0, 1);
    lcd.print("activas");
    delay(1500);

    estadoActual = MENU_ALARMAS;
    lcd.clear();
    return;
  }

  indiceVistaAlarma = buscarSiguienteAlarmaActiva(-1);
  estadoActual = VER_ALARMAS;
  lcd.clear();
}

void gestionarVistaAlarmas(char tecla) {
  if (indiceVistaAlarma < 0) {
    estadoActual = MENU_ALARMAS;
    lcd.clear();
    return;
  }

  mostrarAlarmaEnVista(indiceVistaAlarma);

  // Siguiente alarma activa
  if (tecla == 'A') {
    indiceVistaAlarma =
      buscarSiguienteAlarmaActiva(indiceVistaAlarma);

    lcd.clear();
  }

  // Alarma activa anterior
  if (tecla == 'B') {
    indiceVistaAlarma =
      buscarAnteriorAlarmaActiva(indiceVistaAlarma);

    lcd.clear();
  }

  if (tecla == '*') {
    estadoActual = MENU_ALARMAS;
    lcd.clear();
  }
}

void mostrarAlarmaEnVista(int indice) {
  lcd.setCursor(0, 0);

  lcd.print("Alarma ");
  lcd.print(indice + 1);
  lcd.print(": ");

  imprimirHora(
    alarmaHoras[indice],
    alarmaMinutos[indice]
  );

  lcd.setCursor(0, 1);
  lcd.print("A> B< *:Volver ");
}

int buscarSiguienteAlarmaActiva(int indiceActual) {
  for (int paso = 1; paso <= MAX_ALARMAS; paso++) {
    int indice = (indiceActual + paso) % MAX_ALARMAS;

    if (indice < 0) {
      indice += MAX_ALARMAS;
    }

    if (alarmaActiva[indice]) {
      return indice;
    }
  }

  return -1;
}

int buscarAnteriorAlarmaActiva(int indiceActual) {
  for (int paso = 1; paso <= MAX_ALARMAS; paso++) {
    int indice = indiceActual - paso;

    while (indice < 0) {
      indice += MAX_ALARMAS;
    }

    indice %= MAX_ALARMAS;

    if (alarmaActiva[indice]) {
      return indice;
    }
  }

  return -1;
}

// =====================================================
// BORRAR ALARMA
// =====================================================

void borrarAlarma() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Borrar alarma");
  lcd.setCursor(0, 1);
  lcd.print("1-5  *:Cancelar");

  int numero = ingresarNumeroAlarma();

  if (numero == -1) {
    estadoActual = MENU_ALARMAS;
    lcd.clear();
    return;
  }

  int indice = numero - 1;

  if (!alarmaActiva[indice]) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Alarma ");
    lcd.print(numero);
    lcd.setCursor(0, 1);
    lcd.print("ya esta vacia");

    delay(1500);

    estadoActual = MENU_ALARMAS;
    lcd.clear();
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);

  lcd.print("Borrar A");
  lcd.print(numero);
  lcd.print(" ");

  imprimirHora(
    alarmaHoras[indice],
    alarmaMinutos[indice]
  );

  lcd.setCursor(0, 1);
  lcd.print("#:Si  *:No");

  char confirmacion = esperarTeclaValida('#', '*');

  if (confirmacion == '#') {
    alarmaActiva[indice] = false;
    alarmaHoras[indice] = 0;
    alarmaMinutos[indice] = 0;
    alarmaYaDisparoEsteMinuto[indice] = false;

    guardarAlarmaEnEEPROM(indice);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Alarma ");
    lcd.print(numero);
    lcd.setCursor(0, 1);
    lcd.print("eliminada");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No se elimino");
  }

  delay(1500);

  estadoActual = MENU_ALARMAS;
  lcd.clear();
}

// =====================================================
// BUZZER
// =====================================================

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

// =====================================================
// SENSOR DHT11
// =====================================================

void leerSensorDHT() {
  if (millis() - previoMillisDHT >= intervaloDHT) {
    previoMillisDHT = millis();

    float nuevaHumedad = dht.readHumidity();
    float nuevaTemperatura = dht.readTemperature();

    if (
      !isnan(nuevaHumedad) &&
      !isnan(nuevaTemperatura)
    ) {
      humedad = nuevaHumedad;
      temperatura = nuevaTemperatura;
    }
  }
}

// =====================================================
// PANTALLA PRINCIPAL
// =====================================================

void pantallaPrincipal() {
  lcd.setCursor(0, 0);
  lcd.print("Reloj: ");

  imprimirHoraCompleta(
    horas,
    minutos,
    segundos
  );

  lcd.print("  ");

  lcd.setCursor(0, 1);

  int cantidad = cantidadAlarmasActivas();

  if (cantidad > 0) {
    lcd.print("A:Menu Alarm:");
    lcd.print(cantidad);
    lcd.print(" ");
  } else {
    lcd.print("A para Menu    ");
  }
}

// =====================================================
// MOSTRAR CLIMA
// =====================================================

void mostrarClima() {
  lcd.setCursor(0, 0);

  if (isnan(temperatura) || isnan(humedad)) {
    lcd.print("Error sensor DHT");
    lcd.setCursor(0, 1);
    lcd.print("* para volver   ");
    return;
  }

  lcd.print("Temp: ");
  lcd.print(temperatura, 1);
  lcd.print(" C    ");

  lcd.setCursor(0, 1);
  lcd.print("Humedad: ");
  lcd.print(humedad, 0);
  lcd.print("%   ");
}

// =====================================================
// CONFIGURAR HORA
// =====================================================

void configurarHora() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ingresa Hora HH");

  int nuevaHora = ingresarValorDosDigitos();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ingresa Min MM");

  int nuevosMinutos = ingresarValorDosDigitos();

  if (
    nuevaHora >= 0 && nuevaHora <= 23 &&
    nuevosMinutos >= 0 && nuevosMinutos <= 59
  ) {
    guardarHoraEnRTC(
      nuevaHora,
      nuevosMinutos,
      0
    );

    leerHoraRTC();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hora guardada");
    lcd.setCursor(0, 1);
    lcd.print("en el RTC");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hora invalida");
  }

  delay(1500);

  estadoActual = MOSTRAR_RELOJ;
  lcd.clear();
}

// =====================================================
// INGRESO DE DATOS
// =====================================================

int ingresarNumeroAlarma() {
  while (true) {
    actualizarHoraDesdeRTC();
    controlarLuzLCD();

    char tecla = teclado.getKey();

    if (tecla >= '1' && tecla <= '5') {
      return tecla - '0';
    }

    if (tecla == '*') {
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
    actualizarHoraDesdeRTC();
    controlarLuzLCD();

    char tecla = teclado.getKey();

    if (tecla >= '0' && tecla <= '9') {
      valor += tecla;
      lcd.print(tecla);
    }
  }

  return valor.toInt();
}

char esperarTeclaValida(char opcion1, char opcion2) {
  while (true) {
    actualizarHoraDesdeRTC();
    controlarLuzLCD();

    char tecla = teclado.getKey();

    if (tecla == opcion1 || tecla == opcion2) {
      return tecla;
    }
  }
}

// =====================================================
// FUNCIONES PARA IMPRIMIR HORAS
// =====================================================

void imprimirHora(int h, int m) {
  if (h < 10) {
    lcd.print("0");
  }

  lcd.print(h);
  lcd.print(":");

  if (m < 10) {
    lcd.print("0");
  }

  lcd.print(m);
}

void imprimirHoraCompleta(int h, int m, int s) {
  imprimirHora(h, m);
  lcd.print(":");

  if (s < 10) {
    lcd.print("0");
  }

  lcd.print(s);
}