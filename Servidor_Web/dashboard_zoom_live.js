const URL_DATOS = "obtener_datos.php";
const URL_GUARDAR_CONFIG = "guardar_config.php";
const URL_ULTIMA_CONFIG = "ultima_config.php";

const ERROR = {
    corriente: { tipo: "relativo", valor: 0.08 },
    temperatura: { tipo: "absoluto", valor: 2.0 },
    humedad: { tipo: "absoluto", valor: 5.0 },
    luminosidad: { tipo: "absoluto", valor: 0.0489 }
};

const charts = {};

let historicoCargando = false;
let ultimoTotalHistorico = 0;

function hoyISO() {
    const d = new Date();
    const y = d.getFullYear();
    const m = String(d.getMonth() + 1).padStart(2, "0");
    const dia = String(d.getDate()).padStart(2, "0");
    return `${y}-${m}-${dia}`;
}

function formatearFecha(fecha) {
    if (!fecha) return "--";

    const partes = fecha.split(" ");
    if (partes.length < 2) return fecha;

    const f = partes[0].split("-");
    if (f.length !== 3) return fecha;

    return `${f[2]}/${f[1]}/${f[0]} ${partes[1]}`;
}

function marcarServidor(ok) {
    const punto = document.getElementById("indicador-servidor");
    const texto = document.getElementById("texto-servidor");

    punto.classList.remove("ok", "error");

    if (ok) {
        punto.classList.add("ok");
        texto.textContent = "Servidor conectado";
    } else {
        punto.classList.add("error");
        texto.textContent = "Error de comunicación";
    }
}

function actualizarTarjeta(variable, dato) {
    const valor = document.getElementById(`valor-${variable}`);
    const fecha = document.getElementById(`fecha-${variable}`);

    if (!dato) {
        valor.textContent = "--";
        fecha.textContent = "Sin datos";
        return;
    }

    const decimales = variable === "corriente" ? 3 : 1;

    valor.textContent = Number(dato.valor).toFixed(decimales);
    fecha.textContent = `Última muestra: ${formatearFecha(dato.fecha)}`;
}

function calcularBandas(variable, valores) {
    const cfg = ERROR[variable];
    const superior = [];
    const inferior = [];

    for (const v of valores) {
        const numero = Number(v);

        if (cfg.tipo === "relativo") {
            superior.push(numero * (1 + cfg.valor));
            inferior.push(numero * (1 - cfg.valor));
        } else {
            superior.push(numero + cfg.valor);
            inferior.push(numero - cfg.valor);
        }
    }

    return { superior, inferior };
}

function crearOActualizarGrafica(variable, datos, reiniciarZoom = false) {
    const canvas = document.getElementById(`grafica-${variable}`);
    const etiquetas = datos.map(x => formatearFecha(x.fecha));
    const valores = datos.map(x => Number(x.valor));
    const bandas = calcularBandas(variable, valores);

    const datasets = [
        {
            label: variable,
            data: valores,
            borderWidth: 2,
            pointRadius: 0,
            pointHoverRadius: 4,
            tension: 0.12
        },
        {
            label: "Límite superior",
            data: bandas.superior,
            borderWidth: 1,
            pointRadius: 0,
            borderDash: [6, 4],
            tension: 0.12
        },
        {
            label: "Límite inferior",
            data: bandas.inferior,
            borderWidth: 1,
            pointRadius: 0,
            borderDash: [6, 4],
            tension: 0.12
        }
    ];

    if (charts[variable]) {
        charts[variable].data.labels = etiquetas;
        charts[variable].data.datasets = datasets;

        // IMPORTANTE:
        // En actualización automática NO hacemos resetZoom().
        // Así el usuario puede seguir mirando una zona ampliada mientras
        // llegan muestras nuevas.
        if (reiniciarZoom) {
            charts[variable].resetZoom();
        }

        charts[variable].update("none");
        return;
    }

    charts[variable] = new Chart(canvas, {
        type: "line",
        data: { labels: etiquetas, datasets },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false,
            normalized: true,

            interaction: {
                mode: "index",
                intersect: false
            },

            plugins: {
                legend: {
                    display: true,
                    position: "top"
                },

                tooltip: {
                    enabled: true
                },

                zoom: {
                    limits: {
                        x: { minRange: 4 }
                    },

                    pan: {
                        enabled: true,
                        mode: "x",
                        modifierKey: "shift"
                    },

                    zoom: {
                        wheel: {
                            enabled: true,
                            speed: 0.08
                        },

                        pinch: {
                            enabled: true
                        },

                        mode: "x"
                    }
                }
            },

            scales: {
                x: {
                    ticks: {
                        maxTicksLimit: 12,
                        autoSkip: true,
                        maxRotation: 25,
                        minRotation: 0
                    }
                },

                y: {
                    beginAtZero: false,
                    grace: "8%"
                }
            }
        }
    });

    canvas.addEventListener("dblclick", () => {
        charts[variable].resetZoom();
    });
}

function resetTodasLasGraficas() {
    Object.values(charts).forEach(chart => chart.resetZoom());
}

async function cargarActuales() {
    try {
        const respuesta = await fetch(`${URL_DATOS}?modo=actuales&t=${Date.now()}`, {
            cache: "no-store"
        });

        if (!respuesta.ok) {
            throw new Error(`HTTP ${respuesta.status}`);
        }

        const datos = await respuesta.json();

        if (!datos.ok) {
            throw new Error(datos.error || "Respuesta inválida");
        }

        marcarServidor(true);

        actualizarTarjeta("corriente", datos.actuales.corriente);
        actualizarTarjeta("temperatura", datos.actuales.temperatura);
        actualizarTarjeta("humedad", datos.actuales.humedad);
        actualizarTarjeta("luminosidad", datos.actuales.luminosidad);

        document.getElementById("ultima-actualizacion").textContent =
            `Actualizado: ${new Date().toLocaleTimeString()}`;

    } catch (error) {
        marcarServidor(false);
    }
}

function periodoIncluyeHoy() {
    const desde = document.getElementById("fecha-desde").value;
    const hasta = document.getElementById("fecha-hasta").value;
    const hoy = hoyISO();

    if (!desde || !hasta) return false;

    return desde <= hoy && hasta >= hoy;
}

async function cargarHistorico(reiniciarZoom = false, silencioso = false) {
    if (historicoCargando) return;

    historicoCargando = true;

    const desde = document.getElementById("fecha-desde").value;
    const hasta = document.getElementById("fecha-hasta").value;

    const params = new URLSearchParams();

    if (desde) params.append("desde", desde);
    if (hasta) params.append("hasta", hasta);

    params.append("t", Date.now());

    const mensaje = document.getElementById("mensaje-historico");

    if (!silencioso) {
        mensaje.textContent = "Consultando histórico...";
    }

    try {
        const respuesta = await fetch(`${URL_DATOS}?${params.toString()}`, {
            cache: "no-store"
        });

        if (!respuesta.ok) {
            throw new Error(`HTTP ${respuesta.status}`);
        }

        const datos = await respuesta.json();

        if (!datos.ok) {
            throw new Error(datos.error || "Respuesta inválida");
        }

        marcarServidor(true);

        actualizarTarjeta("corriente", datos.actuales.corriente);
        actualizarTarjeta("temperatura", datos.actuales.temperatura);
        actualizarTarjeta("humedad", datos.actuales.humedad);
        actualizarTarjeta("luminosidad", datos.actuales.luminosidad);

        crearOActualizarGrafica(
            "corriente",
            datos.historico.corriente || [],
            reiniciarZoom
        );

        crearOActualizarGrafica(
            "temperatura",
            datos.historico.temperatura || [],
            reiniciarZoom
        );

        crearOActualizarGrafica(
            "humedad",
            datos.historico.humedad || [],
            reiniciarZoom
        );

        crearOActualizarGrafica(
            "luminosidad",
            datos.historico.luminosidad || [],
            reiniciarZoom
        );

        const total =
            (datos.historico.corriente || []).length +
            (datos.historico.temperatura || []).length +
            (datos.historico.humedad || []).length +
            (datos.historico.luminosidad || []).length;

        ultimoTotalHistorico = total;

        if (total === 0) {
            mensaje.textContent = "No hay mediciones para el período seleccionado.";
        } else {
            mensaje.textContent =
                `${total} registros encontrados · actualización automática activa`;
        }

    } catch (error) {
        marcarServidor(false);

        if (!silencioso) {
            mensaje.textContent = `Error consultando datos: ${error.message}`;
        }

    } finally {
        historicoCargando = false;
    }
}

async function cargarUltimaConfig() {
    try {
        const respuesta = await fetch(`${URL_ULTIMA_CONFIG}?t=${Date.now()}`, {
            cache: "no-store"
        });

        const datos = await respuesta.json();

        if (!datos.ok || !datos.existe) return;

        document.getElementById("ultima-id").textContent = datos.id;

        document.getElementById("ultima-solicitud").textContent =
            formatearFecha(datos.fecha_solicitud);

        document.getElementById("ultima-aplicacion").textContent =
            datos.fecha_aplicacion
                ? formatearFecha(datos.fecha_aplicacion)
                : "--";

        const estado = document.getElementById("ultimo-estado");

        estado.textContent = datos.estado;
        estado.className = "";

        if (datos.estado === "APLICADA") {
            estado.classList.add("estado-aplicada");
        } else if (datos.estado === "PENDIENTE") {
            estado.classList.add("estado-pendiente");
        } else if (datos.estado === "REEMPLAZADA") {
            estado.classList.add("estado-reemplazada");
        }

        document.getElementById("ultimos-intervalos").textContent =
            `Corriente: ${datos.corriente_ms} ms | ` +
            `Temperatura: ${datos.temperatura_ms} ms | ` +
            `Humedad: ${datos.humedad_ms} ms | ` +
            `Luminosidad: ${datos.luminosidad_ms} ms`;

    } catch (error) {
        console.error(error);
    }
}

async function enviarConfiguracion(evento) {
    evento.preventDefault();

    const mensaje = document.getElementById("mensaje-config");

    const cuerpo = {
        corriente_ms: Number(document.getElementById("cfg-corriente").value),
        temperatura_ms: Number(document.getElementById("cfg-temperatura").value),
        humedad_ms: Number(document.getElementById("cfg-humedad").value),
        luminosidad_ms: Number(document.getElementById("cfg-luminosidad").value)
    };

    mensaje.className = "mensaje";
    mensaje.textContent = "Enviando configuración...";

    try {
        const respuesta = await fetch(URL_GUARDAR_CONFIG, {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify(cuerpo)
        });

        const datos = await respuesta.json();

        if (!respuesta.ok || !datos.ok) {
            throw new Error(datos.error || "No se pudo guardar");
        }

        mensaje.className = "mensaje ok";
        mensaje.textContent =
            `Configuración ID ${datos.id} creada. Estado: ${datos.estado}.`;

        await cargarUltimaConfig();

    } catch (error) {
        mensaje.className = "mensaje error";
        mensaje.textContent = `Error: ${error.message}`;
    }
}

document.getElementById("btn-consultar").addEventListener(
    "click",
    () => cargarHistorico(true, false)
);

document.getElementById("btn-hoy").addEventListener("click", () => {
    const hoy = hoyISO();

    document.getElementById("fecha-desde").value = hoy;
    document.getElementById("fecha-hasta").value = hoy;

    cargarHistorico(true, false);
});

document.getElementById("btn-reset-todas").addEventListener(
    "click",
    resetTodasLasGraficas
);

document.querySelectorAll(".boton-reset").forEach(boton => {
    boton.addEventListener("click", () => {
        const nombre = boton.dataset.chart;

        if (charts[nombre]) {
            charts[nombre].resetZoom();
        }
    });
});

document.getElementById("form-config").addEventListener(
    "submit",
    enviarConfiguracion
);

const hoy = hoyISO();

document.getElementById("fecha-desde").value = hoy;
document.getElementById("fecha-hasta").value = hoy;

cargarActuales();
cargarHistorico(true, false);
cargarUltimaConfig();

/*
 * Tarjetas actuales: cada 5 s.
 * Histórico/gráficas: cada 5 s SOLO si el rango seleccionado incluye hoy.
 * Esto permite ver llegar los puntos sin perder el zoom/pan.
 */
setInterval(cargarActuales, 5000);

setInterval(() => {
    if (periodoIncluyeHoy()) {
        cargarHistorico(false, true);
    }
}, 5000);

setInterval(cargarUltimaConfig, 2000);
