<?php

header("Content-Type: application/json; charset=utf-8");
require_once __DIR__ . "/conexion.php";

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    http_response_code(405);
    echo json_encode(["ok" => false, "error" => "Metodo no permitido"]);
    exit;
}

$entrada = file_get_contents("php://input");
$datos = json_decode($entrada, true);

if (!is_array($datos)) {
    http_response_code(400);
    echo json_encode(["ok" => false, "error" => "JSON invalido"]);
    exit;
}

$campos = ["corriente_ms","temperatura_ms","humedad_ms","luminosidad_ms"];

foreach ($campos as $campo) {
    if (!isset($datos[$campo])) {
        http_response_code(400);
        echo json_encode(["ok" => false, "error" => "Falta el campo " . $campo]);
        exit;
    }
}

$corriente_ms   = intval($datos["corriente_ms"]);
$temperatura_ms = intval($datos["temperatura_ms"]);
$humedad_ms     = intval($datos["humedad_ms"]);
$luminosidad_ms = intval($datos["luminosidad_ms"]);

$MAX_INTERVALO = 600000;

if ($corriente_ms < 50 || $corriente_ms > $MAX_INTERVALO) {
    http_response_code(400);
    echo json_encode(["ok" => false, "error" => "corriente_ms fuera de rango"]);
    exit;
}

if ($temperatura_ms < 2000 || $temperatura_ms > $MAX_INTERVALO) {
    http_response_code(400);
    echo json_encode(["ok" => false, "error" => "temperatura_ms fuera de rango"]);
    exit;
}

if ($humedad_ms < 2000 || $humedad_ms > $MAX_INTERVALO) {
    http_response_code(400);
    echo json_encode(["ok" => false, "error" => "humedad_ms fuera de rango"]);
    exit;
}

if ($luminosidad_ms < 50 || $luminosidad_ms > $MAX_INTERVALO) {
    http_response_code(400);
    echo json_encode(["ok" => false, "error" => "luminosidad_ms fuera de rango"]);
    exit;
}

$conexion = conectar_bd();
$conexion->begin_transaction();

try {

    $conexion->query("
        UPDATE configuracion
        SET estado = 'REEMPLAZADA'
        WHERE estado = 'PENDIENTE'
    ");

    $stmt = $conexion->prepare("
        INSERT INTO configuracion (
            corriente_ms,
            temperatura_ms,
            humedad_ms,
            luminosidad_ms,
            estado
        )
        VALUES (?, ?, ?, ?, 'PENDIENTE')
    ");

    $stmt->bind_param(
        "iiii",
        $corriente_ms,
        $temperatura_ms,
        $humedad_ms,
        $luminosidad_ms
    );

    $stmt->execute();
    $id = $conexion->insert_id;
    $stmt->close();
    $conexion->commit();

    echo json_encode([
        "ok" => true,
        "id" => $id,
        "estado" => "PENDIENTE",
        "corriente_ms" => $corriente_ms,
        "temperatura_ms" => $temperatura_ms,
        "humedad_ms" => $humedad_ms,
        "luminosidad_ms" => $luminosidad_ms
    ]);

} catch (Throwable $e) {

    $conexion->rollback();
    http_response_code(500);

    echo json_encode([
        "ok" => false,
        "error" => "No se pudo guardar la configuracion"
    ]);
}

$conexion->close();

?>