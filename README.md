# Calibracion

Banco de pruebas en ESP32-S3 para leer, y más adelante calibrar, los sensores de una unidad de
monitoreo estructural: acelerómetro y giroscopio, inclinómetro y magnetómetro. Muestra los valores
por la consola serie y en una pantalla OLED.

## Estado

- Hecho: lectura y visualización de los cuatro sensores (consola y OLED).
- Pendiente: calibración con una tecla al arrancar y guardado de los valores en NVS.
- Con problema: el inclinómetro SCL3400 reporta banderas de error (ver "Problemas conocidos").

## Hardware

| Sensor | Bus | Conexión | Configuración |
|---|---|---|---|
| ICM-42670-P (acelerómetro y giroscopio) | SPI3, modo 0, 10 MHz | CS en GPIO37 | Acelerómetro ±2 g, giroscopio ±250 °/s, 100 Hz, filtro de 16 Hz |
| SCL3400-D01 (inclinómetro) | SPI3, modo 0, 1 MHz | CS en GPIO38 | Modo A: ±30°, 32768 LSB/g, filtro de 10 Hz |
| QMC5883L (magnetómetro) | I2C0, 400 kHz | dirección 0x0D | ±2 G, 100 Hz, OSR 512 |
| OLED SSD1306, 128x64 | I2C0, 400 kHz | dirección 0x3C | Texto de 8 filas |

- SPI3: SCLK en GPIO40, MOSI en GPIO41, MISO en GPIO42.
- I2C0: SCL en GPIO0, SDA en GPIO1.
- Placa: ESP32-S3 con 16 MB de flash. Este proyecto no usa PSRAM.

## Compilar y cargar

- ESP-IDF v5.4.4, objetivo `esp32s3`. `sdkconfig.defaults` fija el objetivo, la flash de 16 MB y la
  consola por UART0 a 115200 baudios.
- Con la extensión de ESP-IDF para VSCode: **ESP-IDF: Build, Flash and Monitor**.
- Clonar en una ruta corta, por ejemplo `C:\dev\Calibracion`. En Windows, las rutas largas
  (más de unos 250 caracteres) hacen fallar la compilación.
- `sdkconfig` no se versiona. Si ya existe uno generado antes de tener `sdkconfig.defaults`,
  bórralo junto con `build/` y vuelve a compilar para que los valores se apliquen.

## Qué muestra

**Consola serie**, unas 10 líneas por segundo de cada sensor:

```
ACC >> X=-0.0306 Y=+0.0023 Z=+0.9757 g | |a|=0.9762 g
GYR >> X=+0.120 Y=-0.050 Z=+0.030 dps
INC >> X=+1.207 Y=-0.262 grados | ax=+0.02107 ay=-0.00457 g | T=26.6 C
MAG >> X=+0.1230 Y=-0.4560 Z=+0.7890 G | |B|=0.9230 G
```

**OLED**, cada sensor con un título corto y sus valores (aceleración en g, giroscopio en °/s,
inclinación en grados, campo magnético en gauss). Si un sensor no responde muestra `sin datos`.

## Problemas conocidos

- **SCL3400 con banderas de error.** Reporta `STATUS=0x0001` (PIN_CONTINUITY) y `ERR_FLAG2=0x4000`
  (D_EXT_C), que el datasheet describe como un error de conexión del condensador externo del pin 10
  (D_EXTC). Mientras persista, el driver entrega los datos marcados con `[CON BANDERAS]` y la OLED
  agrega un `!` al título del inclinómetro. **Esos datos no son confiables para calibrar.** La causa
  probable está en el hardware (condensador de 100 nF del pin 10 o su soldadura), pendiente de revisar.
- **Pines delicados.** GPIO0 es un pin de arranque y aquí se usa como SCL de I2C. GPIO37 es el CS del
  ICM-42670-P; en módulos ESP32-S3 N16R8 (PSRAM octal) ese pin comparte el bus de la PSRAM.

## Estructura

```
main/          código del proyecto (un módulo por sensor, más la pantalla y main.c)
components/    helper_i2c y ssd1306, copiados del proyecto anterior
sdkconfig.defaults
```

## Origen y licencias

`components/helper_i2c` y `components/ssd1306` provienen del proyecto anterior de la unidad. Su autor
original es Carlos A. Vargas C. El controlador del SSD1306 se basa en el de Adafruit Industries
(licencia BSD): al redistribuir, hay que conservar los avisos de sus cabeceras.
