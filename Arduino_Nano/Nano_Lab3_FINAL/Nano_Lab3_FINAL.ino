#include <Wire.h>
#include <U8x8lib.h>
#include <DHT.h>
#include <string.h>

// LAB 3 - NODO EDGE
// ARDUINO NANO = I2C SLAVE 0x08

//
// Funciones:
// - Corriente por A0
// - LDR por A1
// - DHT11 por D4
// - OLED por I2C software D2/D3
// - Selector D6/D7/D8/D9
// - I2C hardware A4/A5 hacia ESP32-C3
// - Muestreo asincrono sin delay()
// - Tiempos configurables desde la nube via ESP32-C3
// - Confirmacion de configuracion aplicada
//
// Paquete Nano -> ESP32-C3: 20 bytes
//   0..1   corriente [mA]          int16
//   2..3   temperatura [x10 C]     int16
//   4..5   humedad [x10 %RH]       uint16
//   6..7   luminosidad [x10 %]     uint16
//   8..9   secuencia corriente     uint16
//   10..11 secuencia temperatura   uint16
//   12..13 secuencia humedad       uint16
//   14..15 secuencia luminosidad   uint16
//   16..19 ID ultima config aplicada uint32
//
// Comando ESP32-C3 -> Nano: 21 bytes
//   byte 0     = 0xA1
//   bytes 1-4  = ID configuracion
//   bytes 5-8  = corriente_ms
//   bytes 9-12 = temperatura_ms
//   bytes 13-16= humedad_ms
//   bytes 17-20= luminosidad_ms
//
// Todos los enteros se envian MSB primero.

// PINES

const byte PIN_CORRIENTE = A0;
const byte PIN_LDR       = A1;
const byte PIN_DHT       = 4;

const byte SEL_CORRIENTE   = 6;
const byte SEL_TEMPERATURA = 7;
const byte SEL_HUMEDAD     = 8;
const byte SEL_LUZ         = 9;

// OLED POR I2C SOFTWARE

U8X8_SSD1306_128X64_NONAME_SW_I2C oled(
  3,               // SCL
  2,               // SDA
  U8X8_PIN_NONE
);

// DHT11

#define DHTTYPE DHT11
DHT dht(PIN_DHT, DHTTYPE);

const unsigned long MIN_LECTURA_FISICA_DHT = 2000;

// I2C HARDWARE HACIA ESP32-C3

const byte DIRECCION_I2C = 0x08;
const byte CMD_CONFIGURACION = 0xA1;

// CALIBRACION DEL AMPERIMETRO

const float VREF = 5.00;

const float FACTOR_BASE = 1.8007;
const float PENDIENTE   = 0.914;
const float INTERCEPTO  = 0.051;
const float UMBRAL_CERO = 0.025;

// LIMITES DE CONFIGURACION

const unsigned long MIN_CORRIENTE_MS   = 50;
const unsigned long MIN_TEMPERATURA_MS = 2000;
const unsigned long MIN_HUMEDAD_MS     = 2000;
const unsigned long MIN_LUMINOSIDAD_MS = 50;
const unsigned long MAX_INTERVALO_MS   = 600000;

// INTERVALOS INICIALES

//
// Coinciden con los valores usados durante las pruebas
// del sistema de software.
//

unsigned long intervaloCorriente   = 500;
unsigned long intervaloTemperatura = 2000;
unsigned long intervaloHumedad     = 2000;
unsigned long intervaloLDR         = 500;

const unsigned long INTERVALO_PANTALLA = 100;
const unsigned long INTERVALO_SERIAL   = 500;

// VARIABLES MEDIDAS

float corriente   = 0.0;
float temperatura = 0.0;
float humedad     = 0.0;
float luminosidad = 0.0;

int ldrRaw = 0;

// DATOS COMPARTIDOS POR I2C

volatile int16_t  i2cCorriente_mA      = 0;
volatile int16_t  i2cTemperatura_x10   = 0;
volatile uint16_t i2cHumedad_x10       = 0;
volatile uint16_t i2cLuminosidad_x10   = 0;

volatile uint16_t seqCorriente   = 0;
volatile uint16_t seqTemperatura = 0;
volatile uint16_t seqHumedad     = 0;
volatile uint16_t seqLuminosidad = 0;

volatile uint32_t idConfigAplicada = 0;

// CONFIGURACION RECIBIDA POR I2C

//
// El callback I2C solo copia los datos.
// La configuracion se valida y aplica dentro de loop().
//

volatile bool configuracionPendiente = false;

volatile uint32_t cfgId = 0;
volatile uint32_t cfgCorrienteMs = 0;
volatile uint32_t cfgTemperaturaMs = 0;
volatile uint32_t cfgHumedadMs = 0;
volatile uint32_t cfgLuminosidadMs = 0;

// PROMEDIADO ASINCRONO DE CORRIENTE

const byte MUESTRAS_CORRIENTE = 50;

unsigned long sumaCorrienteADC = 0;
byte contadorCorriente = 0;

unsigned long anteriorMuestraCorriente = 0;

// TEMPORIZADORES

unsigned long anteriorLDR = 0;

unsigned long anteriorTemperatura = 0;
unsigned long anteriorHumedad = 0;
unsigned long anteriorLecturaFisicaDHT = 0;

unsigned long anteriorPantalla = 0;
unsigned long anteriorSerial = 0;

// SELECTOR

enum VariablePantalla {
  MOSTRAR_CORRIENTE,
  MOSTRAR_TEMPERATURA,
  MOSTRAR_HUMEDAD,
  MOSTRAR_LUZ,
  SIN_SELECCION
};

// UTILIDADES I2C

uint32_t leerU32Wire() {

  uint32_t valor = 0;

  for (byte i = 0; i < 4; i++) {

    valor <<= 8;

    valor |= (uint8_t)Wire.read();
  }

  return valor;
}


void escribirU16Wire(uint16_t valor) {

  Wire.write(
    (uint8_t)((valor >> 8) & 0xFF)
  );

  Wire.write(
    (uint8_t)(valor & 0xFF)
  );
}


void escribirU32Wire(uint32_t valor) {

  Wire.write(
    (uint8_t)((valor >> 24) & 0xFF)
  );

  Wire.write(
    (uint8_t)((valor >> 16) & 0xFF)
  );

  Wire.write(
    (uint8_t)((valor >> 8) & 0xFF)
  );

  Wire.write(
    (uint8_t)(valor & 0xFF)
  );
}

// LEER SELECTOR

VariablePantalla leerSelector() {

  if (digitalRead(SEL_CORRIENTE) == LOW) {
    return MOSTRAR_CORRIENTE;
  }

  if (digitalRead(SEL_TEMPERATURA) == LOW) {
    return MOSTRAR_TEMPERATURA;
  }

  if (digitalRead(SEL_HUMEDAD) == LOW) {
    return MOSTRAR_HUMEDAD;
  }

  if (digitalRead(SEL_LUZ) == LOW) {
    return MOSTRAR_LUZ;
  }

  return SIN_SELECCION;
}

// PUBLICAR NUEVO VALOR EN EL PAQUETE I2C

void publicarCorrienteI2C() {

  int16_t valor =
    (int16_t)(corriente * 1000.0);

  noInterrupts();

  i2cCorriente_mA = valor;
  seqCorriente++;

  interrupts();
}


void publicarTemperaturaI2C() {

  int16_t valor =
    (int16_t)(temperatura * 10.0);

  noInterrupts();

  i2cTemperatura_x10 = valor;
  seqTemperatura++;

  interrupts();
}


void publicarHumedadI2C() {

  uint16_t valor =
    (uint16_t)(humedad * 10.0);

  noInterrupts();

  i2cHumedad_x10 = valor;
  seqHumedad++;

  interrupts();
}


void publicarLuminosidadI2C() {

  uint16_t valor =
    (uint16_t)(luminosidad * 10.0);

  noInterrupts();

  i2cLuminosidad_x10 = valor;
  seqLuminosidad++;

  interrupts();
}

// CORRIENTE - 50 MUESTRAS SIN BLOQUEO

//
// intervaloCorriente representa el tiempo aproximado
// entre dos valores nuevos de corriente.
//
// Ejemplo:
// intervaloCorriente = 500 ms
// 50 muestras -> una muestra ADC cada 10 ms.
//

void actualizarCorriente(unsigned long ahora) {

  unsigned long paso =
    intervaloCorriente / MUESTRAS_CORRIENTE;

  if (paso < 1) {
    paso = 1;
  }

  if (
    ahora - anteriorMuestraCorriente
    < paso
  ) {
    return;
  }

  anteriorMuestraCorriente = ahora;

  sumaCorrienteADC +=
    analogRead(PIN_CORRIENTE);

  contadorCorriente++;

  if (
    contadorCorriente
    < MUESTRAS_CORRIENTE
  ) {
    return;
  }

  float adcPromedio =
    sumaCorrienteADC /
    (float)MUESTRAS_CORRIENTE;

  float voltaje =
    adcPromedio
    * VREF
    / 1023.0;

  float corrienteBase =
    voltaje
    * FACTOR_BASE;

  if (
    corrienteBase
    < UMBRAL_CERO
  ) {

    corriente = 0.0;

  } else {

    corriente =
      (PENDIENTE * corrienteBase)
      + INTERCEPTO;
  }

  if (corriente < 0.0) {
    corriente = 0.0;
  }

  sumaCorrienteADC = 0;
  contadorCorriente = 0;

  publicarCorrienteI2C();
}

// LDR

void actualizarLDR(unsigned long ahora) {

  if (
    ahora - anteriorLDR
    < intervaloLDR
  ) {
    return;
  }

  anteriorLDR = ahora;

  ldrRaw =
    analogRead(PIN_LDR);

  luminosidad =
    (ldrRaw / 1023.0)
    * 100.0;

  if (luminosidad > 100.0) {
    luminosidad = 100.0;
  }

  if (luminosidad < 0.0) {
    luminosidad = 0.0;
  }

  publicarLuminosidadI2C();
}

// DHT11

//
// Temperatura y humedad tienen intervalos logicos
// independientes.
//
// Como ambas salen del mismo DHT11, la lectura fisica
// se limita a una cada 2000 ms.
//
// Si una variable todavia no esta vencida, no se marca
// como nueva aunque el DHT haya sido consultado para la otra.
//

void actualizarDHT(unsigned long ahora) {

  bool temperaturaVencida =
    ahora - anteriorTemperatura
    >= intervaloTemperatura;

  bool humedadVencida =
    ahora - anteriorHumedad
    >= intervaloHumedad;

  if (
    !temperaturaVencida
    && !humedadVencida
  ) {
    return;
  }

  if (
    ahora - anteriorLecturaFisicaDHT
    < MIN_LECTURA_FISICA_DHT
  ) {
    return;
  }

  anteriorLecturaFisicaDHT = ahora;

  float nuevaTemperatura =
    dht.readTemperature();

  float nuevaHumedad =
    dht.readHumidity();

  if (
    temperaturaVencida
    && !isnan(nuevaTemperatura)
  ) {

    temperatura =
      nuevaTemperatura;

    anteriorTemperatura = ahora;

    publicarTemperaturaI2C();
  }

  if (
    humedadVencida
    && !isnan(nuevaHumedad)
  ) {

    humedad =
      nuevaHumedad;

    anteriorHumedad = ahora;

    publicarHumedadI2C();
  }
}

// APLICAR CONFIGURACION RECIBIDA

void aplicarConfiguracionPendiente() {

  if (!configuracionPendiente) {
    return;
  }

  uint32_t id;
  uint32_t corrienteMs;
  uint32_t temperaturaMs;
  uint32_t humedadMs;
  uint32_t luminosidadMs;

  noInterrupts();

  id            = cfgId;
  corrienteMs   = cfgCorrienteMs;
  temperaturaMs = cfgTemperaturaMs;
  humedadMs     = cfgHumedadMs;
  luminosidadMs = cfgLuminosidadMs;

  configuracionPendiente = false;

  interrupts();

  bool valida = true;

  if (
    corrienteMs < MIN_CORRIENTE_MS
    || corrienteMs > MAX_INTERVALO_MS
  ) {
    valida = false;
  }

  if (
    temperaturaMs < MIN_TEMPERATURA_MS
    || temperaturaMs > MAX_INTERVALO_MS
  ) {
    valida = false;
  }

  if (
    humedadMs < MIN_HUMEDAD_MS
    || humedadMs > MAX_INTERVALO_MS
  ) {
    valida = false;
  }

  if (
    luminosidadMs < MIN_LUMINOSIDAD_MS
    || luminosidadMs > MAX_INTERVALO_MS
  ) {
    valida = false;
  }

  if (!valida) {

    Serial.println(
      F("CONFIG I2C INVALIDA - NO APLICADA")
    );

    return;
  }

  intervaloCorriente   = corrienteMs;
  intervaloTemperatura = temperaturaMs;
  intervaloHumedad     = humedadMs;
  intervaloLDR         = luminosidadMs;

  unsigned long ahora = millis();

  // Reiniciar temporizadores para que los nuevos
  // intervalos comiencen a contar desde este momento.

  sumaCorrienteADC = 0;
  contadorCorriente = 0;
  anteriorMuestraCorriente = ahora;

  anteriorLDR = ahora;
  anteriorTemperatura = ahora;
  anteriorHumedad = ahora;

  noInterrupts();

  idConfigAplicada = id;

  interrupts();

  Serial.println();
  Serial.println(
    F("================================")
  );

  Serial.println(
    F("CONFIGURACION REMOTA APLICADA")
  );

  Serial.println(
    F("================================")
  );

  Serial.print(F("ID: "));
  Serial.println(id);

  Serial.print(F("Corriente: "));
  Serial.print(intervaloCorriente);
  Serial.println(F(" ms"));

  Serial.print(F("Temperatura: "));
  Serial.print(intervaloTemperatura);
  Serial.println(F(" ms"));

  Serial.print(F("Humedad: "));
  Serial.print(intervaloHumedad);
  Serial.println(F(" ms"));

  Serial.print(F("Luminosidad: "));
  Serial.print(intervaloLDR);
  Serial.println(F(" ms"));

  Serial.println(
    F("================================")
  );
}

// OLED

void actualizarPantalla(unsigned long ahora) {

  if (
    ahora - anteriorPantalla
    < INTERVALO_PANTALLA
  ) {
    return;
  }

  anteriorPantalla = ahora;

  VariablePantalla seleccion =
    leerSelector();

  static VariablePantalla seleccionAnterior =
    SIN_SELECCION;

  static char textoAnterior[12] = "";

  char numero[8];
  char textoNuevo[12];

  if (
    seleccion
    != seleccionAnterior
  ) {

    oled.clearDisplay();

    oled.setFont(
      u8x8_font_chroma48medium8_r
    );

    if (
      seleccion
      == MOSTRAR_CORRIENTE
    ) {

      oled.setCursor(3, 1);
      oled.print(F("CORRIENTE"));

    } else if (
      seleccion
      == MOSTRAR_TEMPERATURA
    ) {

      oled.setCursor(2, 1);
      oled.print(F("TEMPERATURA"));

    } else if (
      seleccion
      == MOSTRAR_HUMEDAD
    ) {

      oled.setCursor(4, 1);
      oled.print(F("HUMEDAD"));

    } else if (
      seleccion
      == MOSTRAR_LUZ
    ) {

      oled.setCursor(2, 1);
      oled.print(F("LUMINOSIDAD"));

    } else {

      oled.setCursor(3, 2);
      oled.print(F("SELECCIONE"));

      oled.setCursor(5, 4);
      oled.print(F("MEDIDA"));
    }

    textoAnterior[0] = '\0';

    seleccionAnterior =
      seleccion;
  }

  if (
    seleccion
    == MOSTRAR_CORRIENTE
  ) {

    dtostrf(
      corriente,
      4,
      2,
      numero
    );

    strcpy(textoNuevo, numero);
    strcat(textoNuevo, "A ");

  } else if (
    seleccion
    == MOSTRAR_TEMPERATURA
  ) {

    dtostrf(
      temperatura,
      4,
      1,
      numero
    );

    strcpy(textoNuevo, numero);
    strcat(textoNuevo, "C ");

  } else if (
    seleccion
    == MOSTRAR_HUMEDAD
  ) {

    dtostrf(
      humedad,
      3,
      0,
      numero
    );

    strcpy(textoNuevo, numero);
    strcat(textoNuevo, "%  ");

  } else if (
    seleccion
    == MOSTRAR_LUZ
  ) {

    dtostrf(
      luminosidad,
      3,
      0,
      numero
    );

    strcpy(textoNuevo, numero);
    strcat(textoNuevo, "%  ");

  } else {

    return;
  }

  if (
    strcmp(
      textoNuevo,
      textoAnterior
    ) != 0
  ) {

    oled.setFont(
      u8x8_font_px437wyse700b_2x2_r
    );

    oled.setCursor(
      2,
      4
    );

    oled.print(
      textoNuevo
    );

    strcpy(
      textoAnterior,
      textoNuevo
    );
  }
}

// MONITOR SERIAL

void actualizarSerial(unsigned long ahora) {

  if (
    ahora - anteriorSerial
    < INTERVALO_SERIAL
  ) {
    return;
  }

  anteriorSerial = ahora;

  Serial.print(F("I: "));
  Serial.print(corriente, 3);
  Serial.print(F(" A"));

  Serial.print(F(" | T: "));
  Serial.print(temperatura, 1);
  Serial.print(F(" C"));

  Serial.print(F(" | H: "));
  Serial.print(humedad, 1);
  Serial.print(F(" %"));

  Serial.print(F(" | Luz: "));
  Serial.print(luminosidad, 1);
  Serial.print(F(" %"));

  Serial.print(F(" | Config ID: "));

  noInterrupts();

  uint32_t id = idConfigAplicada;

  interrupts();

  Serial.println(id);
}

// I2C - ESP32-C3 SOLICITA DATOS

void enviarDatosI2C() {

  escribirU16Wire(
    (uint16_t)i2cCorriente_mA
  );

  escribirU16Wire(
    (uint16_t)i2cTemperatura_x10
  );

  escribirU16Wire(
    i2cHumedad_x10
  );

  escribirU16Wire(
    i2cLuminosidad_x10
  );

  escribirU16Wire(
    seqCorriente
  );

  escribirU16Wire(
    seqTemperatura
  );

  escribirU16Wire(
    seqHumedad
  );

  escribirU16Wire(
    seqLuminosidad
  );

  escribirU32Wire(
    idConfigAplicada
  );
}

// I2C - RECIBIR CONFIGURACION

//
// IMPORTANTE:
// No se hace Serial ni procesamiento pesado aqui.
// Solo se copia el comando recibido.

void recibirDatosI2C(int cantidad) {

  if (
    cantidad != 21
  ) {

    while (Wire.available()) {
      Wire.read();
    }

    return;
  }

  byte comando =
    Wire.read();

  if (
    comando
    != CMD_CONFIGURACION
  ) {

    while (Wire.available()) {
      Wire.read();
    }

    return;
  }

  uint32_t id =
    leerU32Wire();

  uint32_t corrienteMs =
    leerU32Wire();

  uint32_t temperaturaMs =
    leerU32Wire();

  uint32_t humedadMs =
    leerU32Wire();

  uint32_t luminosidadMs =
    leerU32Wire();

  cfgId = id;
  cfgCorrienteMs = corrienteMs;
  cfgTemperaturaMs = temperaturaMs;
  cfgHumedadMs = humedadMs;
  cfgLuminosidadMs = luminosidadMs;

  configuracionPendiente = true;
}

// SETUP

void setup() {

  Serial.begin(115200);

  pinMode(
    SEL_CORRIENTE,
    INPUT_PULLUP
  );

  pinMode(
    SEL_TEMPERATURA,
    INPUT_PULLUP
  );

  pinMode(
    SEL_HUMEDAD,
    INPUT_PULLUP
  );

  pinMode(
    SEL_LUZ,
    INPUT_PULLUP
  );

  dht.begin();

  oled.begin();

  oled.setPowerSave(0);

  oled.clearDisplay();

  oled.setFont(
    u8x8_font_chroma48medium8_r
  );

  oled.setCursor(4, 2);
  oled.print(F("MEDIDOR"));

  oled.setCursor(1, 4);
  oled.print(F("MULTIVARIABLE"));

  // Nano como esclavo I2C
  // A4 = SDA
  // A5 = SCL

  Wire.begin(
    DIRECCION_I2C
  );

  Wire.onRequest(
    enviarDatosI2C
  );

  Wire.onReceive(
    recibirDatosI2C
  );

  unsigned long ahora =
    millis();

  anteriorMuestraCorriente = ahora;
  anteriorLDR = ahora;
  anteriorTemperatura = ahora;
  anteriorHumedad = ahora;

  // Permitimos que el primer DHT pueda leerse
  // cuando se cumpla su intervalo logico.
  anteriorLecturaFisicaDHT = 0;

  Serial.println();
  Serial.println(
    F("MEDIDOR MULTIVARIABLE - LAB 3")
  );

  Serial.println(
    F("Arduino Nano - I2C Slave 0x08")
  );

  Serial.println(
    F("--------------------------------")
  );

  Serial.println(
    F("Corriente -> A0")
  );

  Serial.println(
    F("LDR       -> A1")
  );

  Serial.println(
    F("DHT11     -> D4")
  );

  Serial.println(
    F("OLED SDA  -> D2")
  );

  Serial.println(
    F("OLED SCL  -> D3")
  );

  Serial.println(
    F("I2C SDA   -> A4")
  );

  Serial.println(
    F("I2C SCL   -> A5")
  );
}

// LOOP

void loop() {

  unsigned long ahora =
    millis();

  aplicarConfiguracionPendiente();

  actualizarCorriente(
    ahora
  );

  actualizarLDR(
    ahora
  );

  actualizarDHT(
    ahora
  );

  actualizarPantalla(
    ahora
  );

  actualizarSerial(
    ahora
  );
}
