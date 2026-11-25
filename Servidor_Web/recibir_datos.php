<?php

header("Content-Type: application/json; charset=utf-8");

require_once __DIR__ . "/conexion.php";

// Permite comprobar desde el navegador que el endpoint existe.
if ($_SERVER["REQUEST_METHOD"] === "GET") {
    echo json_encode([
        "ok" => true,
        "endpoint" => "recibir_datos",
        "metodo_esperado" => "POST"
    ]);
    exit;
}

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    http_response_code(405);
    echo json_encode([
        "ok" => false,
        "error" => "Metodo no permitido"
    ]);
    exit;
}

$entrada = file_get_contents("php://input");
$datos = json_decode($entrada, true);

if (!is_array($datos)) {
    http_response_code(400);
    echo json_encode([
        "ok" => false,
        "error" => "Se esperaba una lista JSON de muestras"
    ]);
    exit;
}

$variables_validas = [
    "corriente",
    "temperatura",
    "humedad",
    "luminosidad"
];

$conexion = conectar_bd();

$sql = "
    INSERT IGNORE INTO muestras (
        id_local,
        fecha,
        variable,
        valor
    )
    VALUES (?, ?, ?, ?)
";

$stmt = $conexion->prepare($sql);

if (!$stmt) {
    http_response_code(500);
    echo json_encode([
        "ok" => false,
        "error" => "No se pudo preparar la consulta"
    ]);
    $conexion->close();
    exit;
}

$recibidos = count($datos);
$guardados = 0;

foreach ($datos as $muestra) {

    if (
        !isset($muestra["id"]) ||
        !isset($muestra["fecha"]) ||
        !isset($muestra["variable"]) ||
        !isset($muestra["valor"])
    ) {
        continue;
    }

    $id_local = intval($muestra["id"]);
    $fecha = strval($muestra["fecha"]);
    $variable = strval($muestra["variable"]);
    $valor = floatval($muestra["valor"]);

    if (!in_array($variable, $variables_validas, true)) {
        continue;
    }

    // Normaliza fechas ISO del tipo 2026-11-02T16:49:45.003
    // al formato DATETIME de MySQL: 2026-11-02 16:49:45.003
    $fecha = str_replace("T", " ", $fecha);

    $stmt->bind_param(
        "issd",
        $id_local,
        $fecha,
        $variable,
        $valor
    );

    if ($stmt->execute()) {
        if ($stmt->affected_rows > 0) {
            $guardados++;
        }
    }
}

$stmt->close();
$conexion->close();

echo json_encode([
    "ok" => true,
    "recibidos" => $recibidos,
    "guardados" => $guardados
]);

?>
