<?php

header("Content-Type: application/json; charset=utf-8");
require_once __DIR__ . "/conexion.php";

$conexion = conectar_bd();

$resultado = $conexion->query("
    SELECT
        id,
        fecha_solicitud,
        corriente_ms,
        temperatura_ms,
        humedad_ms,
        luminosidad_ms,
        estado
    FROM configuracion
    WHERE estado = 'PENDIENTE'
    ORDER BY id DESC
    LIMIT 1
");

if (!$resultado || $resultado->num_rows === 0) {

    echo json_encode([
        "ok" => true,
        "pendiente" => false
    ]);

    $conexion->close();
    exit;
}

$fila = $resultado->fetch_assoc();

echo json_encode([
    "ok" => true,
    "pendiente" => true,
    "id" => intval($fila["id"]),
    "fecha_solicitud" => $fila["fecha_solicitud"],
    "corriente_ms" => intval($fila["corriente_ms"]),
    "temperatura_ms" => intval($fila["temperatura_ms"]),
    "humedad_ms" => intval($fila["humedad_ms"]),
    "luminosidad_ms" => intval($fila["luminosidad_ms"]),
    "estado" => $fila["estado"]
]);

$conexion->close();

?>