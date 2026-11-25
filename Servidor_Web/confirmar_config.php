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

if (!is_array($datos) || !isset($datos["id"])) {
    http_response_code(400);
    echo json_encode(["ok" => false, "error" => "Falta el ID de configuracion"]);
    exit;
}

$id = intval($datos["id"]);
$conexion = conectar_bd();

$stmt = $conexion->prepare("
    UPDATE configuracion
    SET
        estado = 'APLICADA',
        fecha_aplicacion = CURRENT_TIMESTAMP
    WHERE
        id = ?
        AND estado = 'PENDIENTE'
");

$stmt->bind_param("i", $id);
$stmt->execute();

$actualizadas = $stmt->affected_rows;
$stmt->close();

if ($actualizadas > 0) {

    echo json_encode([
        "ok" => true,
        "id" => $id,
        "estado" => "APLICADA"
    ]);

} else {

    $consulta = $conexion->prepare("
        SELECT estado
        FROM configuracion
        WHERE id = ?
        LIMIT 1
    ");

    $consulta->bind_param("i", $id);
    $consulta->execute();
    $resultado = $consulta->get_result();

    if ($resultado->num_rows === 0) {

        http_response_code(404);

        echo json_encode([
            "ok" => false,
            "error" => "Configuracion no encontrada"
        ]);

    } else {

        $fila = $resultado->fetch_assoc();

        echo json_encode([
            "ok" => true,
            "id" => $id,
            "estado" => $fila["estado"]
        ]);
    }

    $consulta->close();
}

$conexion->close();

?>