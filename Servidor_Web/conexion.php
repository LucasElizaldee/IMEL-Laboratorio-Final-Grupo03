<?php

$DB_HOST = "localhost";
$DB_USUARIO = getenv("IMEL_DB_USER") ?: "TU_USUARIO";
$DB_CONTRASENA = getenv("IMEL_DB_PASSWORD") ?: "TU_CONTRASENA";
$DB_NOMBRE = getenv("IMEL_DB_NAME") ?: "TU_BASE_DE_DATOS";

function conectar_bd() {
    global $DB_HOST, $DB_USUARIO, $DB_CONTRASENA, $DB_NOMBRE;

    $conexion = new mysqli(
        $DB_HOST,
        $DB_USUARIO,
        $DB_CONTRASENA,
        $DB_NOMBRE
    );

    if ($conexion->connect_error) {
        http_response_code(500);
        header("Content-Type: application/json; charset=utf-8");
        echo json_encode([
            "ok" => false,
            "error" => "No se pudo conectar a la base de datos"
        ]);
        exit;
    }

    $conexion->set_charset("utf8mb4");

    return $conexion;
}

?>
