import sqlite3
import time
from pathlib import Path

import requests

# CONFIGURACION

BASE_DIR = Path(__file__).resolve().parent

BASE_DATOS = BASE_DIR / "mediciones.db"

URL_SERVIDOR = (
    "https://utecnologica.org/"
    "estudiantes/imel/g03/recibir_datos.php"
)

# Durante las pruebas usamos 10 segundos.
# Para la configuracion final del laboratorio:
# INTERVALO = 600
INTERVALO = 10

TAMANO_LOTE = 500

TIMEOUT_HTTP = 20

# BASE DE DATOS LOCAL

def conectar_db():

    conexion = sqlite3.connect(
        BASE_DATOS,
        timeout=10
    )

    conexion.execute(
        "PRAGMA busy_timeout = 10000"
    )

    return conexion


def preparar_estado():

    conexion = conectar_db()

    cursor = conexion.cursor()

    # Se usa una tabla NUEVA para no mezclar este servidor
    # con el estado de sincronizacion que se utilizaba con
    # PythonAnywhere.

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS
        estado_sincronizacion_utecnologica (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            ultimo_id INTEGER NOT NULL DEFAULT 0
        )
    """)

    cursor.execute("""
        SELECT ultimo_id
        FROM estado_sincronizacion_utecnologica
        WHERE id = 1
    """)

    fila = cursor.fetchone()

    if fila is None:

        # En el primer arranque NO enviamos automaticamente
        # todo el historial viejo de pruebas/simulacion.
        # Empezamos desde el ultimo ID que ya existe en la
        # base local y, a partir de ahi, enviamos solo las
        # muestras nuevas del prototipo.

        cursor.execute("""
            SELECT COALESCE(MAX(id), 0)
            FROM muestras
        """)

        ultimo_existente = int(
            cursor.fetchone()[0]
        )

        cursor.execute("""
            INSERT INTO
            estado_sincronizacion_utecnologica (
                id,
                ultimo_id
            )
            VALUES (1, ?)
        """, (
            ultimo_existente,
        ))

        print(
            f"Primer inicio: sincronizacion arrancara desde ID {ultimo_existente}."
        )

        print(
            "El historial viejo no se enviara automaticamente."
        )

    conexion.commit()
    conexion.close()


def obtener_ultimo_id():

    conexion = conectar_db()

    cursor = conexion.cursor()

    cursor.execute("""
        SELECT ultimo_id
        FROM estado_sincronizacion_utecnologica
        WHERE id = 1
    """)

    fila = cursor.fetchone()

    conexion.close()

    if fila is None:
        return 0

    return int(fila[0])


def guardar_ultimo_id(ultimo_id):

    conexion = conectar_db()

    cursor = conexion.cursor()

    cursor.execute("""
        UPDATE estado_sincronizacion_utecnologica
        SET ultimo_id = ?
        WHERE id = 1
    """, (
        int(ultimo_id),
    ))

    conexion.commit()
    conexion.close()


def obtener_muestras_pendientes():

    ultimo_id = obtener_ultimo_id()

    conexion = conectar_db()

    conexion.row_factory = sqlite3.Row

    cursor = conexion.cursor()

    cursor.execute("""
        SELECT
            id,
            fecha,
            variable,
            valor
        FROM muestras
        WHERE id > ?
        ORDER BY id ASC
        LIMIT ?
    """, (
        ultimo_id,
        TAMANO_LOTE
    ))

    filas = cursor.fetchall()

    conexion.close()

    muestras = []

    for fila in filas:

        muestras.append({
            "id": int(fila["id"]),
            "fecha": str(fila["fecha"]),
            "variable": str(fila["variable"]),
            "valor": float(fila["valor"])
        })

    return muestras

# ENVIO HTTP

def enviar_lote(muestras):

    respuesta = requests.post(
        URL_SERVIDOR,
        json=muestras,
        timeout=TIMEOUT_HTTP
    )

    respuesta.raise_for_status()

    try:

        datos = respuesta.json()

    except ValueError:

        raise RuntimeError(
            "El servidor no devolvio JSON valido: "
            + respuesta.text[:300]
        )

    if not datos.get("ok", False):

        raise RuntimeError(
            "El servidor respondio con error: "
            + str(datos)
        )

    return datos

# SINCRONIZACION

def sincronizar():

    muestras = obtener_muestras_pendientes()

    if not muestras:

        print(
            "Sin muestras pendientes para enviar."
        )

        return

    primero = muestras[0]["id"]
    ultimo = muestras[-1]["id"]

    print()
    print(
        "========================================"
    )

    print(
        f"Enviando IDs {primero} a {ultimo}"
    )

    print(
        f"Cantidad local: {len(muestras)}"
    )

    datos = enviar_lote(
        muestras
    )

    # El endpoint usa INSERT IGNORE.
    # Por eso "guardados" puede ser menor que "recibidos"
    # si alguna muestra ya existia en el servidor.
    # Si el servidor responde ok, el lote se considera
    # correctamente procesado.

    guardar_ultimo_id(
        ultimo
    )

    print(
        f"Muestras enviadas: {datos.get('recibidos', len(muestras))}"
    )

    print(
        f"Nuevas guardadas: {datos.get('guardados', '?')}"
    )

    print(
        f"Ultimo ID confirmado: {ultimo}"
    )

    print(
        "Servidor: UTECnologica / Grupo 03"
    )

    print(
        "========================================"
    )

# MAIN

def main():

    print()
    print(
        "SINCRONIZACION LOCAL -> SERVIDOR UTECNologica"
    )

    print(
        "Grupo 03 - Instrumentacion IoT"
    )

    print(
        "URL:"
    )

    print(
        URL_SERVIDOR
    )

    print(
        f"Base local: {BASE_DATOS}"
    )

    print(
        f"Intervalo de prueba: {INTERVALO} s"
    )

    print()

    preparar_estado()

    while True:

        try:

            sincronizar()

        except requests.RequestException as error:

            print(
                "ERROR HTTP:"
            )

            print(
                error
            )

        except sqlite3.Error as error:

            print(
                "ERROR SQLITE:"
            )

            print(
                error
            )

        except Exception as error:

            print(
                "ERROR:"
            )

            print(
                error
            )

        time.sleep(
            INTERVALO
        )


if __name__ == "__main__":
    main()
