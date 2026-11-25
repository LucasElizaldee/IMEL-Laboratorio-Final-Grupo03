-- Estructura remota reconstruida a partir del SQL y las capturas de phpMyAdmin.
-- Seleccionar previamente una base vacía del grupo en phpMyAdmin.
CREATE TABLE IF NOT EXISTS muestras (
    id INT AUTO_INCREMENT PRIMARY KEY,
    id_local INT NOT NULL,
    fecha DATETIME(3) NOT NULL,
    variable VARCHAR(30) NOT NULL,
    valor DOUBLE NOT NULL,
    UNIQUE KEY uq_id_local (id_local),
    KEY idx_muestras_fecha (fecha),
    KEY idx_muestras_variable (variable)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS configuracion (
    id INT AUTO_INCREMENT PRIMARY KEY,
    fecha_solicitud DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    corriente_ms INT NOT NULL,
    temperatura_ms INT NOT NULL,
    humedad_ms INT NOT NULL,
    luminosidad_ms INT NOT NULL,
    estado VARCHAR(20) NOT NULL DEFAULT 'PENDIENTE',
    fecha_aplicacion DATETIME NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
