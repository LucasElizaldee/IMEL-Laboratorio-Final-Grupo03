import json
import time
import threading

import requests
import paho.mqtt.client as mqtt

# CONFIGURACION

BROKER_MQTT = "127.0.0.1"
PUERTO_MQTT = 1883

TOPICO_CONFIG = "lab3/config"
TOPICO_CONFIRMACION = "lab3/config/confirmacion"

URL_PENDIENTE = (
    "https://utecnologica.org/"
    "estudiantes/imel/g03/config_pendiente.php"
)

URL_CONFIRMAR = (
    "https://utecnologica.org/"
    "estudiantes/imel/g03/confirmar_config.php"
)

INTERVALO_POLLING = 2
INTERVALO_REENVIO = 5
TIMEOUT_HTTP = 15

# ESTADO

config_pendiente = None
ultimo_envio = 0.0

lock = threading.Lock()

# MQTT

def al_conectar(cliente, userdata, flags, rc):

    if rc == 0:

        print("MQTT conectado")

        cliente.subscribe(
            TOPICO_CONFIRMACION
        )

        print(
            "Suscripto a "
            + TOPICO_CONFIRMACION
        )

    else:

        print(
            "Error MQTT al conectar. Codigo:",
            rc
        )


def confirmar_en_servidor(id_config):

    try:

        respuesta = requests.post(
            URL_CONFIRMAR,
            json={
                "id": id_config
            },
            timeout=TIMEOUT_HTTP
        )

        respuesta.raise_for_status()

        datos = respuesta.json()

        if datos.get("ok"):

            print()
            print(
                "========================================"
            )

            print(
                "CONFIGURACION CONFIRMADA EN SERVIDOR"
            )

            print(
                f"ID: {id_config}"
            )

            print(
                f"Estado: {datos.get('estado')}"
            )

            print(
                "========================================"
            )

            return True

        print(
            "El servidor no acepto la confirmacion:",
            datos
        )

    except Exception as error:

        print(
            "ERROR confirmando en servidor:"
        )

        print(
            error
        )

    return False


def al_mensaje(cliente, userdata, mensaje):

    global config_pendiente

    if mensaje.topic != TOPICO_CONFIRMACION:
        return

    try:

        datos = json.loads(
            mensaje.payload.decode("utf-8")
        )

        id_confirmado = int(
            datos["id"]
        )

    except Exception:

        print(
            "Confirmacion MQTT invalida:",
            mensaje.payload
        )

        return

    with lock:

        if config_pendiente is None:
            return

        id_esperado = int(
            config_pendiente["id"]
        )

    if id_confirmado != id_esperado:

        print(
            "Confirmacion recibida para otro ID:",
            id_confirmado
        )

        return

    print()
    print(
        f"Confirmacion MQTT recibida. ID: {id_confirmado}"
    )

    if confirmar_en_servidor(
        id_confirmado
    ):

        with lock:

            if (
                config_pendiente is not None
                and int(
                    config_pendiente["id"]
                ) == id_confirmado
            ):

                config_pendiente = None

# CONSULTAR SERVIDOR

def consultar_pendiente():

    try:

        respuesta = requests.get(
            URL_PENDIENTE,
            timeout=TIMEOUT_HTTP
        )

        respuesta.raise_for_status()

        datos = respuesta.json()

        if not datos.get("ok"):

            print(
                "Respuesta invalida del servidor:",
                datos
            )

            return None

        if not datos.get(
            "pendiente",
            False
        ):

            return None

        return {
            "id":
                int(
                    datos["id"]
                ),

            "corriente_ms":
                int(
                    datos["corriente_ms"]
                ),

            "temperatura_ms":
                int(
                    datos["temperatura_ms"]
                ),

            "humedad_ms":
                int(
                    datos["humedad_ms"]
                ),

            "luminosidad_ms":
                int(
                    datos["luminosidad_ms"]
                )
        }

    except Exception as error:

        print(
            "ERROR consultando servidor:"
        )

        print(
            error
        )

        return None

# PUBLICAR CONFIGURACION

def publicar_configuracion(
    cliente,
    config
):

    global ultimo_envio

    payload = json.dumps(
        {
            "id":
                config["id"],

            "corriente_ms":
                config["corriente_ms"],

            "temperatura_ms":
                config["temperatura_ms"],

            "humedad_ms":
                config["humedad_ms"],

            "luminosidad_ms":
                config["luminosidad_ms"]
        },
        separators=(",", ":")
    )

    resultado = cliente.publish(
        TOPICO_CONFIG,
        payload
    )

    if resultado.rc == mqtt.MQTT_ERR_SUCCESS:

        ultimo_envio = time.monotonic()

        print()
        print(
            "========================================"
        )

        print(
            "CONFIGURACION SERVIDOR -> MQTT"
        )

        print(
            f"ID: {config['id']}"
        )

        print(
            f"Corriente: {config['corriente_ms']} ms"
        )

        print(
            f"Temperatura: {config['temperatura_ms']} ms"
        )

        print(
            f"Humedad: {config['humedad_ms']} ms"
        )

        print(
            f"Luminosidad: {config['luminosidad_ms']} ms"
        )

        print(
            "========================================"
        )

    else:

        print(
            "No se pudo publicar por MQTT."
        )

# MAIN

def main():

    global config_pendiente
    global ultimo_envio

    print()
    print(
        "CLIENTE DE CONFIGURACION - UTECNologica"
    )

    print(
        "Grupo 03 - Instrumentacion IoT"
    )

    print(
        "Polling:"
    )

    print(
        URL_PENDIENTE
    )

    print()

    cliente = mqtt.Client()

    cliente.on_connect = al_conectar
    cliente.on_message = al_mensaje

    try:

        cliente.connect(
            BROKER_MQTT,
            PUERTO_MQTT,
            60
        )

    except Exception as error:

        print(
            "No se pudo conectar a Mosquitto:"
        )

        print(
            error
        )

        return

    cliente.loop_start()

    try:

        while True:

            nueva = consultar_pendiente()

            if nueva is not None:

                with lock:

                    if (
                        config_pendiente is None
                        or int(
                            config_pendiente["id"]
                        ) != int(
                            nueva["id"]
                        )
                    ):

                        config_pendiente = nueva
                        ultimo_envio = 0.0

                        print()
                        print(
                            "Nueva configuracion pendiente detectada."
                        )

            with lock:

                actual = (
                    None
                    if config_pendiente is None
                    else dict(
                        config_pendiente
                    )
                )

            if (
                actual is not None
                and cliente.is_connected()
                and (
                    time.monotonic()
                    - ultimo_envio
                    >= INTERVALO_REENVIO
                )
            ):

                publicar_configuracion(
                    cliente,
                    actual
                )

            time.sleep(
                INTERVALO_POLLING
            )

    except KeyboardInterrupt:

        print()
        print(
            "Cliente detenido."
        )

    finally:

        cliente.loop_stop()
        cliente.disconnect()


if __name__ == "__main__":
    main()
