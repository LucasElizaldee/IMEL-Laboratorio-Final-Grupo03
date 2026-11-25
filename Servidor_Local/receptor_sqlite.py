"""Receptor MQTT de las cuatro variables. Reconstrucción compatible para el repositorio."""
import math
import sqlite3
from datetime import datetime
from pathlib import Path
import paho.mqtt.client as mqtt

BASE_DATOS = Path(__file__).resolve().parent / "mediciones.db"
BROKER_MQTT = "127.0.0.1"
PUERTO_MQTT = 1883
TOPICOS = {f"lab3/{v}": v for v in ("corriente", "temperatura", "humedad", "luminosidad")}

# Abrimos la base local con tiempo de espera para compartirla con el sincronizador.
def conectar_db():
    return sqlite3.connect(BASE_DATOS, timeout=10)

# Creamos la tabla de muestras y los índices para las consultas históricas.
def preparar_base():
    with conectar_db() as conexion:
        conexion.execute("PRAGMA journal_mode=WAL")
        conexion.execute("""CREATE TABLE IF NOT EXISTS muestras (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            fecha TEXT NOT NULL,
            variable TEXT NOT NULL,
            valor REAL NOT NULL
        )""")
        conexion.execute("CREATE INDEX IF NOT EXISTS idx_muestras_fecha ON muestras(fecha)")
        conexion.execute("CREATE INDEX IF NOT EXISTS idx_muestras_variable ON muestras(variable)")

# Guardamos cada lectura por separado para respetar los distintos tiempos de muestreo.
def guardar_muestra(variable, valor):
    fecha = datetime.now().isoformat(sep=" ", timespec="milliseconds")
    with conectar_db() as conexion:
        conexion.execute("INSERT INTO muestras (fecha, variable, valor) VALUES (?, ?, ?)",
                         (fecha, variable, valor))
    return fecha

# Volvemos a suscribirnos a los cuatro tópicos al conectar o reconectar.
def al_conectar(cliente, userdata, flags, rc):
    if rc == 0:
        print("MQTT conectado", flush=True)
        for topico in TOPICOS:
            cliente.subscribe(topico)
    else:
        print(f"Error de conexión MQTT: {rc}", flush=True)

# Convertimos el mensaje numérico y guardamos la lectura que llegó del gateway.
def al_mensaje(cliente, userdata, mensaje):
    variable = TOPICOS.get(mensaje.topic)
    if variable is None:
        return
    try:
        valor = float(mensaje.payload.decode("utf-8"))
        if not math.isfinite(valor):
            raise ValueError("Valor no finito")
        fecha = guardar_muestra(variable, valor)
        print(f"{fecha} | {variable}: {valor}", flush=True)
    except (ValueError, UnicodeError, sqlite3.Error) as error:
        print(f"No se pudo guardar {variable}: {error}", flush=True)

# Iniciamos la base y dejamos el receptor escuchando al broker local.
def main():
    preparar_base()
    cliente = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
    cliente.on_connect = al_conectar
    cliente.on_message = al_mensaje
    cliente.connect(BROKER_MQTT, PUERTO_MQTT, 60)
    try:
        cliente.loop_forever()
    except KeyboardInterrupt:
        cliente.disconnect()

if __name__ == "__main__":
    main()
