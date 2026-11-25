-- Esquema SQLite local; los scripts crean las tablas automáticamente.
CREATE TABLE IF NOT EXISTS muestras (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    fecha TEXT NOT NULL,
    variable TEXT NOT NULL,
    valor REAL NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_muestras_fecha ON muestras(fecha);
CREATE INDEX IF NOT EXISTS idx_muestras_variable ON muestras(variable);
CREATE TABLE IF NOT EXISTS estado_sincronizacion_utecnologica (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    ultimo_id INTEGER NOT NULL DEFAULT 0
);
