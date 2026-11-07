# Vintage Maker

Multiefecto VST3 de carácter cinematográfico, analógico y lo-fi (C++20, JUCE 9.0.5, CMake).

Cadena: `INPUT → TEXTURE → COLOR → MOVEMENT → SPACE → OUTPUT`

> Uso personal. No se distribuye.

## Estado

| Módulo | Estado |
|---|---|
| 0 — Core y fundamentos | **Validado** (Windows + FL Studio) |
| 1 — Texture | **Implementado y probado en Linux.** Falta validar en Windows y FL Studio |
| 2 — Color | Pendiente |
| 3 — Movement | Pendiente |
| 4 — Space | Pendiente |
| 5–9 | Pendiente |

## Compilar en Windows 11 (Visual Studio 2022, x64)

Clona o descomprime el proyecto en una **ruta corta** (por ejemplo `C:\dev\VintageMaker`), porque las rutas largas de JUCE pueden superar el límite de 260 caracteres de Windows.

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

O ejecuta `scripts\build-windows.bat`, que hace lo mismo en Debug y en Release.

- El primer `cmake` descarga JUCE 9.0.5 (necesita Git e Internet).
- Resultado: `build\VintageMaker_artefacts\Release\VST3\Vintage Maker.vst3` (es una carpeta).
- Para usarlo en FL Studio, copia esa carpeta a `C:\Program Files\Common Files\VST3\` y, en FL Studio, ve a *Options → Manage plugins → Find more plugins*.
- Para abrir el proyecto en Visual Studio: `build\VintageMaker.sln`.

Opciones de CMake:

| Opción | Por defecto | Uso |
|---|---|---|
| `VM_BUILD_PLUGIN` | ON | Compila el VST3 (descarga JUCE) |
| `VM_BUILD_TESTS` | ON | Compila `CoreTests` |
| `VM_ENABLE_SANITIZERS` | OFF | ASan + UBSan (solo GCC/Clang) |

Solo pruebas del Core, sin JUCE: `cmake -S . -B build-core -DVM_BUILD_PLUGIN=OFF`.

## GitHub Actions (CI)

`.github/workflows/build.yml` se ejecuta en cada push, en cada pull request y a mano desde la pestaña *Actions* (botón *Run workflow*). Tiene dos trabajos:

| Trabajo | Runner | Qué hace |
|---|---|---|
| Core tests (Linux, ASan/UBSan) | `ubuntu-latest` | Compila el Core con detectores de errores de memoria y comportamiento indefinido, y ejecuta `CoreTests` |
| Windows x64 (Debug / Release) | `windows-latest` | Descarga JUCE, compila el VST3, ejecuta `CoreTests` y, en Release, **pluginval** con strictness 10; después sube el `.vst3` |

El plugin compilado (solo Release; Debug se compila y prueba pero no se sube) se descarga en *Actions → (ejecución) → Artifacts → `VintageMaker-VST3-windows-Release`*.

Subir el proyecto (el zip ya incluye el historial de Git):

```bat
cd C:\dev\VintageMaker
git remote add origin https://github.com/<usuario>/VintageMaker.git
git push -u origin main --tags
```

## Arquitectura

```text
Source/
├── Core/                  SIN JUCE — flujo de señal y utilidades de tiempo real
│   ├── AudioBufferView.h    vista no propietaria de audio planar (mono/estéreo)
│   ├── SmoothedValue.h      rampas lineales de parámetros (anti-clicks)
│   ├── DenormalGuard.h      FTZ/DAZ por ámbito
│   ├── ParameterIds.h       IDs estables y rangos (compartidos con el plugin)
│   ├── ModuleProcessor.h    contrato común de los 4 módulos DSP
│   ├── ModuleSlot.*         bypass + dry/wet por módulo, sin clicks
│   ├── AudioEngine.*        Input Gain → 4 slots en serie
│   └── MasterProcessor.*    Master Dry/Wet, Output Gain, Bypass, troceo de bloques
├── DSP/
│   ├── Test/TestProcessor.* procesador transparente (slots aún sin módulo real)
│   └── Texture/             Módulo 1
│       ├── TextureProcessor.*   orquesta los componentes (siempre 100 % wet)
│       ├── TextureParameters.h  parámetros, rangos y valores por defecto
│       ├── WowFlutter.*         modulación de tono con línea de retardo
│       ├── DelayLine.h          retardo con interpolación Hermite
│       ├── NoiseGenerator.h     ruido blanco/rosa determinista
│       ├── VinylProcessor.*     hiss, crackle y "drop"
│       └── DegradeProcessor.*   transitorios, saturación y ancho de banda
├── Plugin/
│   └── ParameterManager.*   APVTS ↔ structs del Core, estado versionado
└── PluginProcessor.*        adaptador JUCE fino (buses, parámetros, estado)
Tests/
├── CoreTests.cpp            pruebas del Core (sin JUCE)
└── TextureTests.cpp         pruebas del Módulo 1 (sin JUCE)
```

**El Core no depende de JUCE.** Así, todo el DSP se compila y se prueba en segundos en cualquier plataforma, y el plugin solo traduce entre el host y el Core.

### Fórmula de mezcla global

```
wet       = Cadena(InputGain · x)
procesado = OutputGain · (x + Mix · (wet − x))
y         = x + Activo · (procesado − x)        Activo: 1 = on, 0 = bypass (con fundido)
```

- La señal *dry* es la entrada original, antes del Input Gain.
- **Output Gain se aplica después del Master Dry/Wet**, de modo que fija el nivel final del plugin en cualquier posición de Mix. Es un cambio deliberado respecto al orden de la especificación (§8), y se puede revertir en `MasterProcessor` si se prefiere.
- El bypass es un paso directo exacto: con el bypass activado, la salida es la entrada bit a bit.

### Contrato de los módulos (`ModuleProcessor`)

| Función | Hilo | Regla |
|---|---|---|
| `prepare (spec)` | mensajes, audio parado | Único sitio donde se puede reservar memoria |
| `process (block)` | audio | In place, sin asignaciones, sin bloqueos |
| `reset()` | audio o mensajes | Debe ser seguro en tiempo real |
| `setParameters / getParameters` | audio (inicio de bloque) | Struct propio de cada módulo; solo fija objetivos de suavizado |
| `getLatencySamples()` | cualquiera | Constante tras `prepare` |

Los módulos siempre devuelven señal 100 % wet. El bypass y el dry/wet de cada módulo los aplica `ModuleSlot`. Un módulo apagado no consume CPU y se reinicia (`reset`) al volver a activarse, para que no suenen colas viejas.

### Parámetros (versión 1)

| ID | Nombre | Rango | Defecto |
|---|---|---|---|
| `inputGain` | Input Gain | −24 … +24 dB | 0 dB |
| `outputGain` | Output Gain | −24 … +24 dB | 0 dB |
| `masterMix` | Master Dry/Wet | 0 … 100 % | 100 % |
| `masterBypass` | Bypass | on/off | off |

- **Los IDs no se renombran ni se eliminan nunca**, porque FL Studio guarda la automatización por ID.
- `masterBypass` es también el parámetro de bypass del host, así que el botón de bypass de FL Studio usa el mismo fundido sin clicks.
- El estado se guarda en XML con la raíz `<VintageMaker stateVersion="1">`. Un estado de una versión futura se rechaza; si a un estado antiguo le faltan parámetros, esos conservan sus valores por defecto.

## Módulo 1 — Texture

Flujo: `entrada → Wow + Flutter → Degrade → caída de nivel del Drop → + ruido de vinilo`.

| Sección | Controles | Cómo funciona |
|---|---|---|
| **Wow** | Depth, Rate (0,1–2 Hz), Shape | Retardo variable de hasta ±3 ms. Shape: seno → triángulo → deriva aleatoria suave |
| **Flutter** | Depth, Rate (4–15 Hz), Variation | Retardo variable de hasta ±0,2 ms. Variation aleatoriza velocidad y amplitud |
| **Vinyl** | Amount, Crackle Type, Drop | Hiss rosa + ráfagas de crackle (Normal / Dusty / Heavy). Drop: bajada ocasional de tono y nivel (cada ~30 s con Drop=0 % hasta ~5 s con 100 %) |
| **Degrade** | Amount, Bandwidth, Smoothing | Suaviza transitorios → saturación suave → pasa-altos y pasa-bajos. Bandwidth: de 500 Hz–1,2 kHz (estrecho) a 40 Hz–12 kHz (ancho) |

Más el **encendido** del slot (`texEnable`) y su **Dry/Wet** (`texMix`).

- **Sin latencia.** El retardo es `profundidad × (1 + modulación)`, así que con Wow, Flutter y Drop a 0 la señal pasa bit a bit. El retardo medio crece con la profundidad (hasta unos 3 ms con Wow al máximo), lo que forma parte del efecto.
- La desviación de tono depende de la velocidad: con Wow al 100 % y 1 Hz es de ±1,9 %.
- Los cambios de profundidad son lentos a propósito (0,4 s en Wow): una rampa rápida del retardo doblaría el tono.
- El ruido es aditivo y no depende del nivel de entrada. Tiene semillas fijas, así que dos renders iguales dan el mismo resultado.
- **Valores por defecto:** Wow 25 %, Flutter 15 %, Vinyl 20 %, Degrade 25 %. A diferencia del Módulo 0, el plugin **ya no es transparente** con los valores por defecto.
- Un estado guardado antes del Módulo 1 se carga bien. Los parámetros de Texture que no estén en él conservan su valor actual.

### Parámetros de Texture (versión 1)

| ID | Rango | Defecto |
|---|---|---|
| `texEnable` | on/off | on |
| `texMix` | 0–100 % | 100 % |
| `texWowDepth` / `texWowShape` | 0–100 % | 25 % / 0 % |
| `texWowRate` | 0,1–2 Hz | 0,6 Hz |
| `texFlutterDepth` / `texFlutterVariation` | 0–100 % | 15 % / 30 % |
| `texFlutterRate` | 4–15 Hz | 8 Hz |
| `texVinylAmount` / `texVinylDrop` | 0–100 % | 20 % / 0 % |
| `texCrackleType` | Normal, Dusty, Heavy | Normal |
| `texDegradeAmount` / `texDegradeBandwidth` / `texDegradeSmoothing` | 0–100 % | 25 % / 60 % / 30 % |

## Pruebas

`TextureTests` (93 comprobaciones): retardo Hermite, salida idéntica a la entrada con todo apagado, **desviación de tono medida por cruces por cero** (Wow ±1,9 %, Flutter ±1,26 %, mitad de profundidad = mitad de desviación), niveles de hiss y crackle por tipo, drop (caída de nivel y de tono), respuesta en frecuencia de Degrade, suavizado de transitorios, resultados idénticos con cualquier tamaño de bloque, frecuencias de 22,05 a 192 kHz, valores extremos y NaN/Inf, mono, reinicio repetible, cero asignaciones y CPU.

`CoreTests` (248 comprobaciones) cubre:

- Silencio, transparencia bit a bit con los valores por defecto (mono y estéreo) y ganancias.
- Orden de la cadena.
- Master Dry/Wet, bypass maestro (exactitud y ausencia de clicks), y bypass y dry/wet por slot.
- Independencia del tamaño de bloque (de 1 a 4096 muestras, incluidos bloques mayores que el preparado).
- Frecuencias de muestreo de 22,05 kHz a 192 kHz.
- Valores extremos, automatización aleatoria y parámetros NaN/Inf.
- Latencia, **cero asignaciones de memoria en `process()`** (contador de `operator new`) y rendimiento.

## Pendiente de verificar en Windows

- [ ] Compilación MSVC en Debug y Release, sin avisos con `/W4`.
- [ ] `ctest` en verde en Windows.
- [ ] Carga en FL Studio 2026, audio entrante y saliente, automatización, bypass y guardado/carga de proyecto.
- [ ] Texture en FL Studio: oír cada sección por separado (Wow, Flutter, Vinyl, Degrade) y comprobar que el Dry/Wet y el encendido no hacen clicks.
- [ ] pluginval (strictness 10): automatizado en CI, falta ver la primera ejecución en verde.
