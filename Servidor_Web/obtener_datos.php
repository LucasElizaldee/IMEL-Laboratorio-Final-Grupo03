<?php

header("Content-Type: application/json; charset=utf-8");

require_once __DIR__ . "/conexion.php";

$variables = [
    "corriente",
    "temperatura",
    "humedad",
    "luminosidad"
];

$conexion = conectar_bd();

$actuales = [];

foreach ($variables as $variable) {

    $stmt = $conexion->prepare("
        SELECT
            id_local,
            fecha,
            valor
        FROM muestras
        WHERE variable = ?
        ORDER BY id_local DESC
        LIMIT 1
    ");

    $stmt->bind_param(
        "s",
        $variable
    );

    $stmt->execute();

    $resultado = $stmt->get_result();

    if ($resultado->num_rows > 0) {

        $fila = $resultado->fetch_assoc();

        $actuales[$variable] = [
            "id_local" => intval($fila["id_local"]),
            "fecha" => $fila["fecha"],
            "valor" => floatval($fila["valor"])
        ];

    } else {

        $actuales[$variable] = null;
    }

    $stmt->close();
}


$modo = isset($_GET["modo"])
    ? trim($_GET["modo"])
    : "";


if ($modo === "actuales") {

    $conexion->close();

    echo json_encode([
        "ok" => true,
        "actuales" => $actuales
    ]);

    exit;
}


$desde = isset($_GET["desde"])
    ? trim($_GET["desde"])
    : date("Y-m-d");

$hasta = isset($_GET["hasta"])
    ? trim($_GET["hasta"])
    : date("Y-m-d");


function fecha_valida($fecha) {

    $d = DateTime::createFromFormat(
        "Y-m-d",
        $fecha
    );

    return (
        $d
        && $d->format("Y-m-d") === $fecha
    );
}


if (
    !fecha_valida($desde)
    || !fecha_valida($hasta)
) {

    http_response_code(400);

    echo json_encode([
        "ok" => false,
        "error" => "Formato de fecha invalido"
    ]);

    $conexion->close();
    exit;
}


$fecha_desde = $desde . " 00:00:00.000";

$fecha_hasta_obj = new DateTime(
    $hasta
);

$fecha_hasta_obj->modify(
    "+1 day"
);

$fecha_hasta = (
    $fecha_hasta_obj->format("Y-m-d")
    . " 00:00:00.000"
);


$historico = [
    "corriente" => [],
    "temperatura" => [],
    "humedad" => [],
    "luminosidad" => []
];


$stmt = $conexion->prepare("
    SELECT
        id_local,
        fecha,
        variable,
        valor
    FROM muestras
    WHERE
        fecha >= ?
        AND fecha < ?
    ORDER BY fecha ASC
    LIMIT 10000
");

$stmt->bind_param(
    "ss",
    $fecha_desde,
    $fecha_hasta
);

$stmt->execute();

$resultado = $stmt->get_result();

while (
    $fila = $resultado->fetch_assoc()
) {

    $variable = $fila["variable"];

    if (
        !array_key_exists(
            $variable,
            $historico
        )
    ) {
        continue;
    }

    $historico[$variable][] = [
        "id_local" => intval(
            $fila["id_local"]
        ),
        "fecha" => $fila["fecha"],
        "valor" => floatval(
            $fila["valor"]
        )
    ];
}

$stmt->close();
$conexion->close();


echo json_encode([
    "ok" => true,
    "desde" => $desde,
    "hasta" => $hasta,
    "actuales" => $actuales,
    "historico" => $historico
]);

?>
