@echo off
title Inicio completo LAB 3 - Grupo 03
color 0A

set "CARPETA=%~dp0."
set "DASHBOARD=https://utecnologica.org/estudiantes/imel/g03/"

echo ============================================================
echo       LAB 3 - INSTRUMENTACION IoT - GRUPO 03
echo ============================================================
echo.

REM Verificar carpeta principal
if not exist "%CARPETA%" (
    echo ERROR: No existe la carpeta:
    echo %CARPETA%
    echo.
    pause
    exit /b 1
)

cd /d "%CARPETA%"

REM Verificar archivos necesarios
if not exist "receptor_sqlite.py" (
    echo ERROR: Falta receptor_sqlite.py
    pause
    exit /b 1
)

if not exist "sincronizar_utecnologica.py" (
    echo ERROR: Falta sincronizar_utecnologica.py
    pause
    exit /b 1
)

if not exist "cliente_config_utecnologica.py" (
    echo ERROR: Falta cliente_config_utecnologica.py
    pause
    exit /b 1
)

REM Comprobar Mosquitto
echo Verificando Mosquitto...

sc query mosquitto | find /I "RUNNING" >nul 2>&1

if errorlevel 1 (
    echo Mosquitto no aparece como iniciado.
    echo Intentando iniciar el servicio...
    net start mosquitto >nul 2>&1

    sc query mosquitto | find /I "RUNNING" >nul 2>&1

    if errorlevel 1 (
        echo.
        echo AVISO: No se pudo iniciar Mosquitto automaticamente.
        echo Si MQTT no conecta, inicia Mosquitto manualmente como administrador.
        echo.
    ) else (
        echo Mosquitto iniciado correctamente.
    )
) else (
    echo Mosquitto ya esta funcionando.
)

echo.

REM Abrir los tres procesos del sistema
echo Iniciando receptor SQLite...
start "LAB3 - Receptor SQLite" cmd /k "cd /d ""%CARPETA%"" && python receptor_sqlite.py"

timeout /t 2 /nobreak >nul

echo Iniciando sincronizacion con servidor UTECnologica...
start "LAB3 - Sincronizacion UTECnologica" cmd /k "cd /d ""%CARPETA%"" && python sincronizar_utecnologica.py"

timeout /t 2 /nobreak >nul

echo Iniciando cliente de configuracion remota...
start "LAB3 - Configuracion Remota" cmd /k "cd /d ""%CARPETA%"" && python cliente_config_utecnologica.py"

timeout /t 3 /nobreak >nul

REM Abrir dashboard
echo Abriendo dashboard...
start "" "%DASHBOARD%"

echo.
echo ============================================================
echo SISTEMA INICIADO
echo ============================================================
echo.
echo Deben quedar abiertas 3 ventanas:
echo   1. receptor_sqlite.py
echo   2. sincronizar_utecnologica.py
echo   3. cliente_config_utecnologica.py
echo.
echo Mosquitto debe estar RUNNING.
echo Dashboard:
echo %DASHBOARD%
echo.
echo Podes cerrar esta ventana cuando quieras.
echo ============================================================
echo.

pause
