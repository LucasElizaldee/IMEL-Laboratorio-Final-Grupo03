#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>

// LAB 3 - GATEWAY
// ESP32-C3 = I2C MASTER + WIFI + MQTT

//
// Funciones:
// - Lee el Arduino Nano por I2C
// - Publica cada variable por MQTT solo cuando hay
//   una muestra nueva
// - Recibe configuraciones desde lab3/config
// - Reenvia configuraciones al Nano por I2C
// - Espera confirmacion fisica del Nano
// - Publica confirmacion en lab3/config/confirmacion
// - Reconecta WiFi/MQTT sin usar delay()
//
// IMPORTANTE:
// Antes de cargar, cambiar SOLO las 3 lineas de
// WIFI_SSID, WIFI_PASSWORD y MQTT_SERVER.

// WIFI Y MQTT - CAMBIAR ESTAS 3 LINEAS

const char* WIFI_SSID =
  "TU_WIFI";

const char* WIFI_PASSWORD =
  "TU_CLAVE_WIFI";

// IP LOCAL de la PC donde corre Mosquitto.
// Ejemplo: 192.168.1.50
const char* MQTT_SERVER =
  "192.168.1.50";

const uint16_t MQTT_PORT = 1883;

// TOPICS MQTT

const char* TOPIC_CORRIENTE =
  "lab3/corriente";

const char* TOPIC_TEMPERATURA =
  "lab3/temperatura";

const char* TOPIC_HUMEDAD =
  "lab3/humedad";

const char* TOPIC_LUMINOSIDAD =
  "lab3/luminosidad";

const char* TOPIC_CONFIG =
  "lab3/config";

const char* TOPIC_CONFIRMACION =
  "lab3/config/confirmacion";

// I2C

//
// ESP32-C3 = MASTER
// GPIO4 = SDA
// GPIO5 = SCL
//
// Nano = SLAVE 0x08
//

const int SDA_PIN = 4;
const int SCL_PIN = 5;

const byte NANO_ADDR = 0x08;

const byte CMD_CONFIGURACION = 0xA1;

const byte TAMANO_PAQUETE_NANO = 20;

// INTERVALOS DEL GATEWAY

const unsigned long INTERVALO_LECTURA_NANO = 20;
const unsigned long INTERVALO_REINTENTO_WIFI = 10000;
const unsigned long INTERVALO_REINTENTO_MQTT = 5000;
const unsigned long INTERVALO_REENVIO_CONFIG = 2000;

// LIMITES DE CONFIGURACION

const uint32_t MIN_CORRIENTE_MS   = 50;
const uint32_t MIN_TEMPERATURA_MS = 2000;
const uint32_t MIN_HUMEDAD_MS     = 2000;
const uint32_t MIN_LUMINOSIDAD_MS = 50;
const uint32_t MAX_INTERVALO_MS   = 600000;

// WIFI + MQTT

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long ultimoIntentoWiFi = 0;
unsigned long ultimoIntentoMQTT = 0;

// DATOS RECIBIDOS DEL NANO

float corriente = 0.0;
float temperatura = 0.0;
float humedad = 0.0;
float luminosidad = 0.0;

uint16_t seqCorriente = 0;
uint16_t seqTemperatura = 0;
uint16_t seqHumedad = 0;
uint16_t seqLuminosidad = 0;

uint16_t ultimoSeqCorrientePublicado = 0;
uint16_t ultimoSeqTemperaturaPublicado = 0;
uint16_t ultimoSeqHumedadPublicado = 0;
uint16_t ultimoSeqLuminosidadPublicado = 0;

uint32_t idConfigAplicadaNano = 0;

unsigned long anteriorLecturaNano = 0;

// CONFIGURACION PENDIENTE HACIA EL NANO

bool hayConfigPendiente = false;

uint32_t idConfigPendiente = 0;
uint32_t cfgCorrienteMs = 0;
uint32_t cfgTemperaturaMs = 0;
uint32_t cfgHumedadMs = 0;
uint32_t cfgLuminosidadMs = 0;

unsigned long ultimoEnvioConfigNano = 0;

uint32_t ultimaConfirmacionPublicada = 0;

// UTILIDADES I2C

uint16_t leerU16Wire() {

  uint16_t valor = 0;

  valor =
    ((uint16_t)(uint8_t)Wire.read() << 8);

  valor |=
    (uint8_t)Wire.read();

  return valor;
}


uint32_t leerU32Wire() {

  uint32_t valor = 0;

  for (byte i = 0; i < 4; i++) {

    valor <<= 8;

    valor |=
      (uint8_t)Wire.read();
  }

  return valor;
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

// PARSER JSON SIMPLE

//
// Recibe JSON generado por cliente_config.py, por ejemplo:
//
// {
//   "id": 3,
//   "corriente_ms": 1000,
//   "temperatura_ms": 5000,
//   "humedad_ms": 5000,
//   "luminosidad_ms": 2000
// }
//
// No requiere ArduinoJson.

bool extraerEnteroJSON(
  const String& texto,
  const char* clave,
  uint32_t& valor
) {

  String patron =
    "\"" + String(clave) + "\"";

  int posicion =
    texto.indexOf(patron);

  if (posicion < 0) {
    return false;
  }

  posicion =
    texto.indexOf(
      ':',
      posicion + patron.length()
    );

  if (posicion < 0) {
    return false;
  }

  posicion++;

  while (
    posicion < texto.length()
    && (
      texto[posicion] == ' '
      || texto[posicion] == '\t'
      || texto[posicion] == '\r'
      || texto[posicion] == '\n'
    )
  ) {
    posicion++;
  }

  if (
    posicion >= texto.length()
    || texto[posicion] < '0'
    || texto[posicion] > '9'
  ) {
    return false;
  }

  uint32_t numero = 0;

  while (
    posicion < texto.length()
    && texto[posicion] >= '0'
    && texto[posicion] <= '9'
  ) {

    numero =
      (numero * 10UL)
      + (texto[posicion] - '0');

    posicion++;
  }

  valor = numero;

  return true;
}

// VALIDAR CONFIGURACION

bool configuracionValida(
  uint32_t corrienteMs,
  uint32_t temperaturaMs,
  uint32_t humedadMs,
  uint32_t luminosidadMs
) {

  if (
    corrienteMs < MIN_CORRIENTE_MS
    || corrienteMs > MAX_INTERVALO_MS
  ) {
    return false;
  }

  if (
    temperaturaMs < MIN_TEMPERATURA_MS
    || temperaturaMs > MAX_INTERVALO_MS
  ) {
    return false;
  }

  if (
    humedadMs < MIN_HUMEDAD_MS
    || humedadMs > MAX_INTERVALO_MS
  ) {
    return false;
  }

  if (
    luminosidadMs < MIN_LUMINOSIDAD_MS
    || luminosidadMs > MAX_INTERVALO_MS
  ) {
    return false;
  }

  return true;
}

// PUBLICAR CONFIRMACION MQTT

bool publicarConfirmacion(
  uint32_t id
) {

  if (!mqtt.connected()) {
    return false;
  }

  char mensaje[64];

  snprintf(
    mensaje,
    sizeof(mensaje),
    "{\"id\":%lu,\"estado\":\"aplicada\"}",
    (unsigned long)id
  );

  bool ok =
    mqtt.publish(
      TOPIC_CONFIRMACION,
      mensaje
    );

  if (ok) {

    ultimaConfirmacionPublicada = id;

    Serial.print(
      "Confirmacion MQTT publicada. ID: "
    );

    Serial.println(id);
  }

  return ok;
}

// CALLBACK MQTT

void callbackMQTT(
  char* topic,
  byte* payload,
  unsigned int length
) {

  if (
    strcmp(
      topic,
      TOPIC_CONFIG
    ) != 0
  ) {
    return;
  }

  String mensaje;

  mensaje.reserve(
    length + 1
  );

  for (
    unsigned int i = 0;
    i < length;
    i++
  ) {

    mensaje +=
      (char)payload[i];
  }

  uint32_t id;
  uint32_t corrienteMs;
  uint32_t temperaturaMs;
  uint32_t humedadMs;
  uint32_t luminosidadMs;

  bool ok =
    extraerEnteroJSON(
      mensaje,
      "id",
      id
    )
    &&
    extraerEnteroJSON(
      mensaje,
      "corriente_ms",
      corrienteMs
    )
    &&
    extraerEnteroJSON(
      mensaje,
      "temperatura_ms",
      temperaturaMs
    )
    &&
    extraerEnteroJSON(
      mensaje,
      "humedad_ms",
      humedadMs
    )
    &&
    extraerEnteroJSON(
      mensaje,
      "luminosidad_ms",
      luminosidadMs
    );

  if (!ok) {

    Serial.println(
      "ERROR: JSON de configuracion invalido"
    );

    return;
  }

  if (
    !configuracionValida(
      corrienteMs,
      temperaturaMs,
      humedadMs,
      luminosidadMs
    )
  ) {

    Serial.println(
      "ERROR: intervalos fuera de rango"
    );

    return;
  }

  // Si el Nano ya habia aplicado este ID,
  // simplemente reenviamos la confirmacion.
  // Esto ayuda si la confirmacion MQTT anterior
  // se perdio.

  if (
    idConfigAplicadaNano == id
  ) {

    publicarConfirmacion(id);

    return;
  }

  idConfigPendiente = id;
  cfgCorrienteMs = corrienteMs;
  cfgTemperaturaMs = temperaturaMs;
  cfgHumedadMs = humedadMs;
  cfgLuminosidadMs = luminosidadMs;

  hayConfigPendiente = true;

  // Fuerza envio inmediato en loop().
  ultimoEnvioConfigNano = 0;

  Serial.println();
  Serial.println(
    "================================"
  );

  Serial.println(
    "CONFIGURACION MQTT RECIBIDA"
  );

  Serial.println(
    "================================"
  );

  Serial.print("ID: ");
  Serial.println(id);

  Serial.print("Corriente: ");
  Serial.print(corrienteMs);
  Serial.println(" ms");

  Serial.print("Temperatura: ");
  Serial.print(temperaturaMs);
  Serial.println(" ms");

  Serial.print("Humedad: ");
  Serial.print(humedadMs);
  Serial.println(" ms");

  Serial.print("Luminosidad: ");
  Serial.print(luminosidadMs);
  Serial.println(" ms");

  Serial.println(
    "================================"
  );
}

// WIFI

void iniciarWiFi() {

  WiFi.mode(
    WIFI_STA
  );

  WiFi.setAutoReconnect(
    true
  );

  WiFi.persistent(
    false
  );

  Serial.print(
    "Conectando WiFi: "
  );

  Serial.println(
    WIFI_SSID
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  ultimoIntentoWiFi =
    millis();
}


void mantenerWiFi(
  unsigned long ahora
) {

  if (
    WiFi.status()
    == WL_CONNECTED
  ) {
    return;
  }

  if (
    ahora - ultimoIntentoWiFi
    < INTERVALO_REINTENTO_WIFI
  ) {
    return;
  }

  ultimoIntentoWiFi =
    ahora;

  Serial.println(
    "Reintentando WiFi..."
  );

  WiFi.disconnect();

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );
}

// MQTT

void mantenerMQTT(
  unsigned long ahora
) {

  if (
    WiFi.status()
    != WL_CONNECTED
  ) {
    return;
  }

  if (mqtt.connected()) {
    return;
  }

  if (
    ahora - ultimoIntentoMQTT
    < INTERVALO_REINTENTO_MQTT
  ) {
    return;
  }

  ultimoIntentoMQTT =
    ahora;

  uint64_t chipId =
    ESP.getEfuseMac();

  char clientId[40];

  snprintf(
    clientId,
    sizeof(clientId),
    "lab3-c3-%08lX",
    (unsigned long)(chipId & 0xFFFFFFFF)
  );

  Serial.print(
    "Conectando MQTT a "
  );

  Serial.print(
    MQTT_SERVER
  );

  Serial.print(":");
  Serial.println(MQTT_PORT);

  if (
    mqtt.connect(
      clientId
    )
  ) {

    Serial.println(
      "MQTT conectado"
    );

    mqtt.subscribe(
      TOPIC_CONFIG,
      1
    );

    Serial.println(
      "Suscripto a lab3/config"
    );

  } else {

    Serial.print(
      "MQTT fallo, estado: "
    );

    Serial.println(
      mqtt.state()
    );
  }
}

// LEER NANO POR I2C

bool leerNano() {

  byte cantidad =
    Wire.requestFrom(
      (uint8_t)NANO_ADDR,
      (uint8_t)TAMANO_PAQUETE_NANO
    );

  if (
    cantidad
    != TAMANO_PAQUETE_NANO
  ) {

    while (Wire.available()) {
      Wire.read();
    }

    return false;
  }

  int16_t corriente_mA =
    (int16_t)leerU16Wire();

  int16_t temperatura_x10 =
    (int16_t)leerU16Wire();

  uint16_t humedad_x10 =
    leerU16Wire();

  uint16_t luminosidad_x10 =
    leerU16Wire();

  uint16_t nuevaSeqCorriente =
    leerU16Wire();

  uint16_t nuevaSeqTemperatura =
    leerU16Wire();

  uint16_t nuevaSeqHumedad =
    leerU16Wire();

  uint16_t nuevaSeqLuminosidad =
    leerU16Wire();

  uint32_t nuevoIdConfigAplicada =
    leerU32Wire();

  corriente =
    corriente_mA / 1000.0;

  temperatura =
    temperatura_x10 / 10.0;

  humedad =
    humedad_x10 / 10.0;

  luminosidad =
    luminosidad_x10 / 10.0;

  seqCorriente =
    nuevaSeqCorriente;

  seqTemperatura =
    nuevaSeqTemperatura;

  seqHumedad =
    nuevaSeqHumedad;

  seqLuminosidad =
    nuevaSeqLuminosidad;

  idConfigAplicadaNano =
    nuevoIdConfigAplicada;

  return true;
}

// PUBLICAR MEDICIONES NUEVAS

void publicarMedicionesNuevas() {

  if (!mqtt.connected()) {
    return;
  }

  char valor[24];

  if (
    seqCorriente
    != ultimoSeqCorrientePublicado
  ) {

    snprintf(
      valor,
      sizeof(valor),
      "%.3f",
      corriente
    );

    if (
      mqtt.publish(
        TOPIC_CORRIENTE,
        valor
      )
    ) {

      ultimoSeqCorrientePublicado =
        seqCorriente;
    }
  }

  if (
    seqTemperatura
    != ultimoSeqTemperaturaPublicado
  ) {

    snprintf(
      valor,
      sizeof(valor),
      "%.1f",
      temperatura
    );

    if (
      mqtt.publish(
        TOPIC_TEMPERATURA,
        valor
      )
    ) {

      ultimoSeqTemperaturaPublicado =
        seqTemperatura;
    }
  }

  if (
    seqHumedad
    != ultimoSeqHumedadPublicado
  ) {

    snprintf(
      valor,
      sizeof(valor),
      "%.1f",
      humedad
    );

    if (
      mqtt.publish(
        TOPIC_HUMEDAD,
        valor
      )
    ) {

      ultimoSeqHumedadPublicado =
        seqHumedad;
    }
  }

  if (
    seqLuminosidad
    != ultimoSeqLuminosidadPublicado
  ) {

    snprintf(
      valor,
      sizeof(valor),
      "%.1f",
      luminosidad
    );

    if (
      mqtt.publish(
        TOPIC_LUMINOSIDAD,
        valor
      )
    ) {

      ultimoSeqLuminosidadPublicado =
        seqLuminosidad;
    }
  }
}

// ENVIAR CONFIGURACION AL NANO

bool enviarConfiguracionNano() {

  Wire.beginTransmission(
    NANO_ADDR
  );

  Wire.write(
    CMD_CONFIGURACION
  );

  escribirU32Wire(
    idConfigPendiente
  );

  escribirU32Wire(
    cfgCorrienteMs
  );

  escribirU32Wire(
    cfgTemperaturaMs
  );

  escribirU32Wire(
    cfgHumedadMs
  );

  escribirU32Wire(
    cfgLuminosidadMs
  );

  byte resultado =
    Wire.endTransmission();

  if (
    resultado == 0
  ) {

    Serial.print(
      "Config enviada al Nano. ID: "
    );

    Serial.println(
      idConfigPendiente
    );

    return true;
  }

  Serial.print(
    "ERROR enviando config I2C. Codigo: "
  );

  Serial.println(
    resultado
  );

  return false;
}

// GESTIONAR CONFIGURACION PENDIENTE

void gestionarConfiguracion(
  unsigned long ahora
) {

  if (!hayConfigPendiente) {
    return;
  }

  // Si el Nano ya confirma el mismo ID,
  // publicamos confirmacion hacia la PC/nube.

  if (
    idConfigAplicadaNano
    == idConfigPendiente
  ) {

    if (
      publicarConfirmacion(
        idConfigPendiente
      )
    ) {

      hayConfigPendiente = false;
    }

    return;
  }

  if (
    ahora - ultimoEnvioConfigNano
    < INTERVALO_REENVIO_CONFIG
  ) {
    return;
  }

  ultimoEnvioConfigNano =
    ahora;

  enviarConfiguracionNano();
}

// MOSTRAR ESTADO PERIODICO

void mostrarEstado() {

  static unsigned long anterior = 0;

  unsigned long ahora =
    millis();

  if (
    ahora - anterior
    < 2000
  ) {
    return;
  }

  anterior = ahora;

  Serial.print(
    "WiFi: "
  );

  if (
    WiFi.status()
    == WL_CONNECTED
  ) {

    Serial.print(
      WiFi.localIP()
    );

  } else {

    Serial.print(
      "DESCONECTADO"
    );
  }

  Serial.print(
    " | MQTT: "
  );

  Serial.print(
    mqtt.connected()
      ? "OK"
      : "OFF"
  );

  Serial.print(
    " | Nano config ID: "
  );

  Serial.print(
    idConfigAplicadaNano
  );

  Serial.print(
    " | I="
  );

  Serial.print(
    corriente,
    3
  );

  Serial.print(
    " T="
  );

  Serial.print(
    temperatura,
    1
  );

  Serial.print(
    " H="
  );

  Serial.print(
    humedad,
    1
  );

  Serial.print(
    " L="
  );

  Serial.println(
    luminosidad,
    1
  );
}

// SETUP

void setup() {

  Serial.begin(
    115200
  );

  // I2C master
  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );

  Wire.setClock(
    100000
  );

  Serial.println();
  Serial.println(
    "ESP32-C3 GATEWAY - LAB 3"
  );

  Serial.println(
    "I2C: SDA GPIO4 / SCL GPIO5"
  );

  Serial.println(
    "Nano: Slave 0x08"
  );

  mqtt.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );

  mqtt.setCallback(
    callbackMQTT
  );

  mqtt.setBufferSize(
    512
  );

  iniciarWiFi();
}

// LOOP

void loop() {

  unsigned long ahora =
    millis();

  mantenerWiFi(
    ahora
  );

  mantenerMQTT(
    ahora
  );

  if (mqtt.connected()) {
    mqtt.loop();
  }

  if (
    ahora - anteriorLecturaNano
    >= INTERVALO_LECTURA_NANO
  ) {

    anteriorLecturaNano =
      ahora;

    if (leerNano()) {

      publicarMedicionesNuevas();

      gestionarConfiguracion(
        ahora
      );
    }
  }

  // Aunque falle una lectura I2C puntual,
  // seguimos reintentando la configuracion.
  gestionarConfiguracion(
    ahora
  );

  mostrarEstado();
}
