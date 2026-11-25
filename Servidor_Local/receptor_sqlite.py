import sqlite3
from datetime import datetime
import paho.mqtt.client as mqtt


# =====================================================
# CONFIGURACION
# =====================================================

BROKER = "localhost"
PUERTO = 1883
DB_LOCAL = "mediciones.db"


# =====================================================
# PREPARAR BASE DE DATOS
# =====================================================

conexion = sqlite3.connect(
    DB_LOCAL,
    check_same_thread=False
)

cursor = conexion.cursor()

cursor.execute("""
CREATE TABLE IF NOT EXISTS muestras (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    fecha TEXT NOT NULL,
    variable TEXT NOT NULL,
    valor REAL NOT NULL
)
""")

cursor.execute("""
CREATE INDEX IF NOT EXISTS idx_muestras_fecha
ON muestras(fecha)
""")

cursor.execute("""
CREATE INDEX IF NOT EXISTS idx_muestras_variable
ON muestras(variable)
""")

conexion.commit()


# =====================================================
# GUARDAR UNA MUESTRA
# =====================================================

def guardar_muestra(variable, valor):

    fecha = datetime.now().strftime(
        "%Y-%m-%d %H:%M:%S.%f"
    )[:-3]

    cursor.execute("""
    INSERT INTO muestras (
        fecha,
        variable,
        valor
    )
    VALUES (?, ?, ?)
    """, (
        fecha,
        variable,
        valor
    ))

    conexion.commit()

    print(
        f"{fecha} | "
        f"{variable}: {valor}"
    )


# =====================================================
# MQTT
# =====================================================

def on_connect(
    client,
    userdata,
    flags,
    reason_code,
    properties
):

    if reason_code == 0:

        print()
        print("RECEPTOR MQTT -> SQLITE")
        print("-----------------------------")
        print("Conectado a Mosquitto")
        print("Esperando muestras...")
        print()

        client.subscribe(
            "lab3/corriente"
        )

        client.subscribe(
            "lab3/temperatura"
        )

        client.subscribe(
            "lab3/humedad"
        )

        client.subscribe(
            "lab3/luminosidad"
        )

    else:

        print(
            "Error MQTT:",
            reason_code
        )


def on_message(
    client,
    userdata,
    msg
):

    mapa = {
        "lab3/corriente": "corriente",
        "lab3/temperatura": "temperatura",
        "lab3/humedad": "humedad",
        "lab3/luminosidad": "luminosidad"
    }

    if msg.topic not in mapa:
        return

    try:

        valor = float(
            msg.payload.decode()
        )

    except ValueError:

        print(
            "Dato invalido:",
            msg.payload
        )

        return

    variable = mapa[msg.topic]

    guardar_muestra(
        variable,
        valor
    )


# =====================================================
# CLIENTE MQTT
# =====================================================

client = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION2
)

client.on_connect = on_connect
client.on_message = on_message

client.connect(
    BROKER,
    PUERTO,
    60
)

try:

    client.loop_forever()

except KeyboardInterrupt:

    print()
    print("Receptor detenido.")

finally:

    conexion.close()