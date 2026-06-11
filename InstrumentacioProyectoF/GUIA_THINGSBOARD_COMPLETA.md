# Guía Completa: Proyecto de Instrumentación Electrónica con ThingsBoard

## Índice
1. [Arquitectura del Sistema](#1-arquitectura-del-sistema)
2. [Configuración de ThingsBoard con Docker](#2-configuración-de-thingsboard-con-docker)
3. [Comunicación ESP32 ↔ ThingsBoard vía MQTT](#3-comunicación-esp32--thingsboard-vía-mqtt)
4. [Dashboard Web Full-Stack](#4-dashboard-web-full-stack)
5. [Problemas Encontrados y Soluciones](#5-problemas-encontrados-y-soluciones)
6. [Guía Paso a Paso para Replicar](#6-guía-paso-a-paso-para-replicar)
7. [Referencia de Comandos](#7-referencia-de-comandos)

---

## 1. Arquitectura del Sistema

```
┌─────────────────┐     MQTT (1884)      ┌──────────────────┐     REST API      ┌─────────────────┐
│                 │                      │                  │                   │                 │
│   ESP32-C6      │ ──── temperatura ──→ │   ThingsBoard    │ ←─── JWT Auth ── │   Dashboard     │
│   (Firmware)    │ ──── bombilla   ──→ │   (Docker)       │ ←─── Telemetry ─ │   (FastAPI)     │
│                 │ ──── ventilador ──→ │   PostgreSQL     │                   │   Puerto 5050   │
│   WiFi: 192...  │ ←─── setPWM     ──  │   Puerto 8080    │                   │                 │
│   Sensor: temp  │ ←─── setBombilla──  │   MQTT: 1884     │                   │                 │
└─────────────────┘                      └──────────────────┘                   └────────┬────────┘
                                                                                         │
                                                                                  ┌──────▼──────┐
                                                                                  │  Navegador  │
                                                                                  │  HTML/CSS/JS│
                                                                                  │  Chart.js   │
                                                                                  └─────────────┘
```

### Componentes:
| Componente | Tecnología | Puerto | Función |
|-----------|-----------|--------|---------|
| ESP32-C6 | C++ / ESP-IDF 5.5.4 | - | Lee sensor, controla actuadores, envía telemetría MQTT |
| ThingsBoard | Docker (tb-postgres) | 8080 (UI), 1884 (MQTT) | Recibe, almacena y sirve datos de dispositivos IoT |
| Dashboard Backend | Python FastAPI | 5050 | Puente entre ThingsBoard API y el frontend web |
| Dashboard Frontend | HTML5 + CSS3 + Chart.js | 5050 (servido por FastAPI) | Visualización profesional de telemetría |

---

## 2. Configuración de ThingsBoard con Docker

### 2.1 Archivo docker-compose.yml

**Ubicación:** `C:\Users\everh\thingsboard\docker-compose.yml`

```yaml
services:
  thingsboard:
    image: thingsboard/tb-postgres
    restart: always
    ports:
      - "8080:9090"      # UI web: host 8080 → contenedor 9090
      - "1884:1883"      # MQTT: host 1884 → contenedor 1883 (IMPORTANTE: no usa 1883 porque Mosquitto lo ocupa)
      - "8883:8883"      # MQTT SSL
      - "7070:7070"      # Edge
    logging:
      driver: "json-file"
      options:
        max-size: "100m"
        max-file: "10"
    environment:
      TB_QUEUE_TYPE: in-memory    # Cola en memoria (desarrollo)
    volumes:
      - tb-data:/data             # Persistencia de BD
      - tb-logs:/var/log/thingsboard

volumes:
  tb-data:
  tb-logs:
```

### 2.2 Por qué `thingsboard/tb-postgres` y no `tb-node` + PostgreSQL externo

- **tb-postgres**: Imagen monolítica que incluye ThingsBoard + PostgreSQL embebido. Ideal para desarrollo.
- **tb-node**: Requiere PostgreSQL externo y ejecutar migraciones de BD por separado.
- Se intentó usar `tb-node:4.3.1.2` + `postgres:18`, pero falló porque la tabla `queue` no existía (faltaba ejecutar la instalación/migración de esquema).

### 2.3 Comandos Docker utilizados

```bash
# Iniciar ThingsBoard
docker compose up -d

# Ver estado
docker compose ps

# Ver logs
docker compose logs -f thingsboard

# Detener
docker compose down
```

### 2.4 Credenciales de ThingsBoard

| Campo | Valor |
|-------|-------|
| URL | http://192.168.1.25:8080 |
| Usuario tenant | tenant@thingsboard.org |
| Password tenant | tenant |
| Usuario sysadmin | sysadmin@thingsboard.local |
| Password sysadmin | sysadmin |

### 2.5 Creación de Dispositivo en ThingsBoard

El dispositivo se creó mediante la API REST de ThingsBoard (no manualmente en la UI):

```python
# Login
POST http://localhost:8080/api/auth/login
Body: {"username": "tenant@thingsboard.org", "password": "tenant"}
→ Response: {"token": "<JWT>"}

# Crear dispositivo
POST http://localhost:8080/api/device
Headers: {"X-Authorization": "Bearer <JWT>"}
Body: {"name": "ESP32_INSTRUMENTACION", "type": "default"}
→ Response: {"id": {"id": "aa9442d0-6415-11f1-ade0-2187e63c29b9"}, ...}

# Establecer token de acceso
POST http://localhost:8080/api/device/credentials
Headers: {"X-Authorization": "Bearer <JWT>"}
Body: {
    "id": "<credential_id>",
    "deviceId": {"id": "aa9442d0-6415-11f1-ade0-2187e63c29b9", "entityType": "DEVICE"},
    "credentialsType": "ACCESS_TOKEN",
    "credentialsId": "MnCIBDPhkA3jXFXS8iVY"
}
```

**Datos del dispositivo:**
- **Nombre:** ESP32_INSTRUMENTACION
- **ID:** aa9442d0-6415-11f1-ade0-2187e63c29b9
- **Token de Acceso:** MnCIBDPhkA3jXFXS8iVY

---

## 3. Comunicación ESP32 ↔ ThingsBoard vía MQTT

### 3.1 Flujo de Conexión

```
ESP32 enciende
  │
  ├─ 1. Inicializa NVS, WiFi, hardware (I2C, ADC, GPIO, PWM)
  │
  ├─ 2. Conecta a WiFi: "PEDRO VASQUEZ  1" (DOBLE ESPACIO)
  │      IP obtenida por DHCP (ej: 192.168.1.38)
  │
  ├─ 3. Crea cliente MQTT y conecta a 192.168.1.25:1884
  │      Autentica con token: "MnCIBDPhkA3jXFXS8iVY"
  │
  ├─ 4. Suscribe a RPC:
  │      Topic: v1/devices/me/rpc/request/+
  │      Métodos: setPWM, setBombilla
  │
  └─ 5. Bucle principal (cada 50ms):
       ├─ tb.loop() → procesa mensajes MQTT entrantes
       ├─ Cada 500ms: lee temperatura del termistor
       │               actualiza bombilla y ventilador
       └─ Cada 1s: envía telemetría empaquetada:
                      temperatura (float), bombilla (0/1), ventilador (0-100%)
```

### 3.2 Configuración del Firmware (main.cpp)

**Archivo:** `H:\06_2026_S1\Intrumentacion electronica\InstrumentacioProyectoF\InstrumentacioProyectoF\main\main.cpp`

```cpp
// --- WiFi (NOTA: DOBLE ESPACIO entre "VASQUEZ" y "1") ---
constexpr char WIFI_SSID[] = "PEDRO VASQUEZ  1";
constexpr char WIFI_PASSWORD[] = "R3sUNAL2023";

// --- ThingsBoard ---
constexpr char TOKEN[] = "MnCIBDPhkA3jXFXS8iVY";
constexpr char THINGSBOARD_SERVER[] = "192.168.1.25";
constexpr uint16_t THINGSBOARD_PORT = 1884U;  // Puerto 1884 (no 1883)
```

### 3.3 Envío de Telemetría (MQTT)

**Formato:** El ESP32 usa la librería `ThingsBoard.h` que internamente publica al topic MQTT:

```
Topic: v1/devices/me/telemetry
Payload: {"temperatura": 25.5}
```

**Código en el ESP32:**
```cpp
// Enviar telemetría cada 2 ciclos de sensor (1s)
if (++telemetry_tick >= 2) {
    telemetry_tick = 0;
    int bomb_state = ctx->bombillo.is_on ? 1 : 0;
    int ven_speed = ctx->ventilador.current_speed_pct;

    tb.sendTelemetryData(TEMPERATURE_KEY, temperatura);
    tb.sendTelemetryData(BOMBILLA_KEY, bomb_state);
    tb.sendTelemetryData(VENTILADOR_KEY, ven_speed);
}
```

**IMPORTANTE:** La librería `sendTelemetryData` solo acepta 1 par key-value por llamada. Para empaquetar múltiples valores, se hacen 3 llamadas consecutivas.

### 3.4 RPC Bidireccional (ThingsBoard → ESP32)

```cpp
// Callback: setPWM(int 0..255)
static void rpc_setPWM(const JsonVariantConst &data, JsonDocument &response) {
    int pwm_value = data.as<int>();
    // Ajusta ciclo de trabajo del ventilador
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pwm_value);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// Callback: setBombilla(bool)
static void rpc_setBombilla(const JsonVariantConst &data, JsonDocument &response) {
    bool state = data.as<bool>();
    bomb_set_state(&(g_ctx->bombillo), state);
}
```

### 3.5 Protocolo MQTT de ThingsBoard

| Dirección | Topic | Payload |
|-----------|-------|---------|
| ESP32 → TB | `v1/devices/me/telemetry` | `{"key": value}` |
| TB → ESP32 | `v1/devices/me/rpc/request/{id}` | `{"method":"setPWM","params":128}` |
| ESP32 → TB | `v1/devices/me/rpc/response/{id}` | `{"result":"ok","pwm":128}` |
| ESP32 → TB | `v1/devices/me/attributes` | Atributos del dispositivo |

---

## 4. Dashboard Web Full-Stack

### 4.1 Estructura del Proyecto

```
C:\Users\everh\thingsboard\dashboard\
├── server.py                 # Backend FastAPI
├── requirements.txt          # Dependencias Python
├── iniciar.bat               # Script de arranque
├── detener.bat               # Script de parada
└── static/
    ├── index.html            # Frontend SPA
    ├── css/
    │   └── dashboard.css     # Estilos profesionales (tema oscuro)
    └── js/
        ├── chart.umd.min.js # Chart.js (local, no CDN)
        ├── api.js            # Cliente de API modular
        └── dashboard.js      # Lógica del dashboard
```

### 4.2 Backend (FastAPI) — `server.py`

**Propósito:** Servir como puente entre el frontend y la API REST de ThingsBoard.

**Endpoints:**

| Método | Ruta | Función |
|--------|------|---------|
| GET | `/api/health` | Verifica conexión con ThingsBoard |
| GET | `/api/device/info` | Info del dispositivo ESP32 |
| GET | `/api/telemetry/latest?keys=` | Último valor de telemetría |
| GET | `/api/telemetry/history?keys=&startTs=&endTs=` | Histórico de telemetría |
| POST | `/api/rpc/{method}` | Enviar comando RPC al ESP32 |

**Flujo de autenticación:**
1. El backend obtiene un JWT de ThingsBoard (`POST /api/auth/login`)
2. El JWT se cachea en memoria y se reusa en todas las llamadas
3. Se refresca automáticamente cuando expira

### 4.3 Frontend — `index.html` + `dashboard.js`

**Diseño:** Tema oscuro profesional con:
- **Ventilador:** SVG animado que gira proporcional a la velocidad real
- **Bombilla:** SVG con efecto de brillo pulsante cuando está encendida
- **Temperatura:** Valor grande (96px) con gradiente de color, fusionado con gráfico histórico
- **Gráfico:** Chart.js con línea de tiempo, controles de rango (1m, 5m, 15m, 1h)
- **Estadísticas:** Mín, Máx, Promedio, Cantidad de lecturas

**Actualización:** El frontend hace polling cada 500ms a la API del backend.

### 4.4 Instalación de Dependencias

```bash
pip install fastapi uvicorn httpx python-multipart
```

---

## 5. Problemas Encontrados y Soluciones

### 5.1 Conflicto de Puerto MQTT 1883 con Mosquitto

**Problema:** Había un servidor Mosquitto MQTT instalado en Windows que ocupaba el puerto 1883. El ESP32 se conectaba a Mosquitto en lugar de a ThingsBoard.

**Diagnóstico:**
```bash
netstat -ano | findstr ":1883"
# Mostró 2 procesos escuchando: mosquitto.exe (PID 5696) y com.docker.backend (PID 6448)
```

**Solución:** Cambiar el mapeo de puertos en Docker de `1883:1883` a `1884:1883` y actualizar el firmware ESP32 para usar puerto 1884.

### 5.2 SSID WiFi con Doble Espacio

**Problema:** El ESP32 no se conectaba al WiFi "PEDRO VASQUEZ  1".

**Diagnóstico:** Se verificó el SSID real con `netsh wlan show interfaces`. Tenía DOBLE espacio entre "VASQUEZ" y "1".

**Solución:** En main.cpp: `"PEDRO VASQUEZ  1"` (con doble espacio).

### 5.3 Tabla `queue` no existe en PostgreSQL

**Problema:** Con la imagen `tb-node:4.3.1.2` + PostgreSQL externo, ThingsBoard fallaba con `ERROR: relation "queue" does not exist`.

**Causa:** La imagen `tb-node` requiere ejecutar la instalación/migración de esquema de BD por separado. Sin eso, las tablas no se crean.

**Solución:** Cambiar a la imagen monolítica `thingsboard/tb-postgres` que maneja PostgreSQL embebido y ejecuta las migraciones automáticamente.

### 5.4 Token de Acceso ya Asignado

**Problema:** Al crear un nuevo dispositivo con el token `MnCIBDPhkA3jXFXS8iVY`, la API respondió: `"Device credentials are already assigned to another device!"`

**Causa:** Ya existía el dispositivo `ESP32_INSTRUMENTACION` con ese token.

**Solución:** Usar el dispositivo existente en lugar de crear uno nuevo.

### 5.5 Valores Booleanos en Telemetría

**Problema:** Al enviar `bombilla` como `bool` (true/false), ThingsBoard retornaba error 500 al consultar esa key.

**Solución:** Cambiar el tipo de dato en el ESP32 de `bool` a `int` (0/1):
```cpp
int bomb_state = ctx->bombillo.is_on ? 1 : 0;  // En lugar de bool
```

### 5.6 Dashboard mostraba "Desconectado"

**Problema:** El frontend alternaba entre "Conectado" y "Desconectado" aunque la API funcionaba.

**Causa:** El `Promise.all()` que agrupaba las llamadas a la API fallaba si una sola request tenía problemas, forzando el estado a desconectado.

**Solución:** Separar las llamadas API en try-catch independientes para que una falla individual no afecte el estado de conexión:
```javascript
// ANTES (frágil)
const [latest, history] = await Promise.all([...]);  // Si una falla, todo falla

// DESPUÉS (robusto)
try { health = await getHealth(); } catch (e) {}
try { latest = await getLatest(); } catch (e) {}
try { history = await getHistory(); } catch (e) {}
```

### 5.7 Chart.js desde CDN no Cargaba

**Problema:** El dashboard no cargaba Chart.js desde `cdn.jsdelivr.net`.

**Solución:** Descargar Chart.js localmente al directorio `static/js/` y referenciarlo sin CDN:
```html
<script src="/js/chart.umd.min.js"></script>
```

### 5.8 Puerto COM12 Ocupado al Flashear

**Problema:** `idf.py -p COM12 flash` fallaba con "Acceso denegado".

**Causa:** Un proceso Python (monitor serial o dashboard) mantenía el puerto abierto.

**Solución:** Cerrar todos los procesos Python antes de flashear:
```powershell
Stop-Process -Name "python" -Force -ErrorAction SilentlyContinue
```

### 5.9 La IP de la Máquina Cambiaba entre Redes

**Problema:** La IP de la PC cambiaba entre 172.20.25.9 y 192.168.1.25 según la red WiFi conectada.

**Solución:** Verificar siempre la IP actual con `Get-NetIPAddress -InterfaceAlias "Wi-Fi"` y actualizar `THINGSBOARD_SERVER` en el firmware si es necesario.

---

## 6. Guía Paso a Paso para Replicar

### 6.1 Requisitos Previos

- Docker Desktop instalado
- Python 3.10+ con pip
- ESP-IDF 5.5.4 instalado (C:\Espressif)
- ESP32-C6 conectado por USB (COM12)
- WiFi "PEDRO VASQUEZ  1" con password "R3sUNAL2023"

### 6.2 Paso 1: Levantar ThingsBoard

```bash
cd C:\Users\everh\thingsboard
docker compose up -d

# Verificar que está corriendo
docker compose ps
# Debe mostrar STATUS "Up" y puertos 8080, 1884

# Esperar 2-3 minutos a que ThingsBoard termine de instalar
docker compose logs -f thingsboard
# Esperar hasta ver: "Started ThingsboardServerApplication"
```

### 6.3 Paso 2: Compilar y Flashear ESP32

```powershell
# Cargar entorno ESP-IDF
. "C:\Espressif\tools\Microsoft.v5.5.4.PowerShell_profile.ps1"

# Ir al proyecto
cd "H:\06_2026_S1\Intrumentacion electronica\InstrumentacioProyectoF\InstrumentacioProyectoF"

# Compilar
idf.py build

# Flashear (asegurarse que ningún programa use COM12)
idf.py -p COM12 flash
```

### 6.4 Paso 3: Crear Dispositivo en ThingsBoard

Ir a `http://192.168.1.25:8080` → Login con `tenant@thingsboard.org` / `tenant`

1. **Devices** → **+** → **Add new device**
2. Nombre: `ESP32_INSTRUMENTACION`
3. **Manage credentials** → **Access token**: `MnCIBDPhkA3jXFXS8iVY`

### 6.5 Paso 4: Verificar Conexión

```powershell
# Verificar conexión MQTT
netstat -ano | Select-String ":1884.*ESTABLISHED"
# Debe mostrar: 192.168.1.25:1884 → IP_DEL_ESP32

# Verificar telemetría en ThingsBoard
curl http://localhost:8080/api/plugins/telemetry/DEVICE/{device_id}/values/timeseries?keys=temperatura
```

### 6.6 Paso 5: Levantar Dashboard

```bash
cd C:\Users\everh\thingsboard\dashboard
pip install fastapi uvicorn httpx python-multipart
python -m uvicorn server:app --host 0.0.0.0 --port 5050
```

Abrir navegador en: `http://192.168.1.25:5050`

---

## 7. Referencia de Comandos

### Docker
```bash
docker compose up -d              # Iniciar
docker compose down               # Detener
docker compose logs -f thingsboard # Ver logs en vivo
docker compose ps                  # Estado
```

### ESP32 (ESP-IDF)
```bash
idf.py build                      # Compilar
idf.py -p COM12 flash              # Flashear
idf.py -p COM12 monitor            # Monitor serial (Ctrl+] para salir)
```

### Dashboard
```bash
python -m uvicorn server:app --host 0.0.0.0 --port 5050   # Iniciar
Ctrl+C                                                      # Detener
```

### Verificación
```bash
# Ver puertos en escucha
netstat -ano | findstr "8080 1884 5050"

# Ver IP actual
Get-NetIPAddress -InterfaceAlias "Wi-Fi" | Select IPAddress

# Ver WiFi conectado
netsh wlan show interfaces | findstr SSID
```

---

## Apéndice A: Datos del Proyecto

| Dato | Valor |
|------|-------|
| Device ID ThingsBoard | aa9442d0-6415-11f1-ade0-2187e63c29b9 |
| Token Acceso ESP32 | MnCIBDPhkA3jXFXS8iVY |
| WiFi SSID | PEDRO VASQUEZ  1 (doble espacio) |
| WiFi Password | R3sUNAL2023 |
| ThingsBoard Server IP | 192.168.1.25 |
| MQTT Port | 1884 |
| Dashboard Port | 5050 |
| ESP32 Chip | ESP32-C6 (QFN40) |
| ESP32 GPIO Temp | ADC1_CH2 |
| ESP32 GPIO Bombilla | GPIO 5 |
| ESP32 GPIO Ventilador | GPIO 4 (LEDC PWM) |
| ESP32 I2C OLED | SDA=22, SCL=21 |

## Apéndice B: Topicos MQTT de ThingsBoard

| Topic | Dirección | Uso |
|-------|-----------|-----|
| `v1/devices/me/telemetry` | Device → Server | Enviar datos de telemetría |
| `v1/devices/me/attributes` | Device → Server | Actualizar atributos del dispositivo |
| `v1/gateway/connect` | Gateway → Server | Conectar gateway |
| `v1/gateway/telemetry` | Gateway → Server | Telemetría desde gateway |
| `v1/devices/me/rpc/request/+` | Server → Device | Recibir comandos RPC |
| `v1/devices/me/rpc/response/{id}` | Device → Server | Responder a comando RPC |

## Apéndice C: Formato de Datos en ThingsBoard

**Telemetría (Timeseries):**
```json
{"temperatura": 25.5, "bombilla": 1, "ventilador": 0}
```

**Atributos de Cliente:**
```json
{"firmware_version": "1.0", "ip": "192.168.1.38"}
```

**Atributos Compartidos (desde servidor):**
```json
{"targetTemperature": 24.0}
```

---

*Documento generado el 10 de junio de 2026*
*Proyecto: Instrumentación Electrónica — Control de Temperatura con ThingsBoard*
