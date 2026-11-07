# Vintage Maker

Multiefecto VST3 de carácter cinematográfico, analógico y lo-fi (C++20, JUCE 9.0.5, CMake).

Cadena: `INPUT → TEXTURE → COLOR → MOVEMENT → SPACE → OUTPUT`

> Uso personal. No se distribuye.

## Estado

| Módulo | Estado |
|---|---|
| 0 — Core y fundamentos | **Implementado.** Core probado en Linux; falta validarlo en Windows y FL Studio |
| 1 — Texture | Pendiente |
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

El plugin compilado se descarga desde la ejecución, en *Actions → (ejecución) → Artifacts → `VintageMaker-VST3-windows-Release`*.

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
│   └── Test/TestProcessor.* procesador de prueba (Módulo 0)
├── Plugin/
│   └── ParameterManager.*   APVTS ↔ structs del Core, estado versionado
└── PluginProcessor.*        adaptador JUCE fino (buses, parámetros, estado)
Tests/
└── CoreTests.cpp            pruebas de consola sin JUCE
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

## Pruebas

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
- [ ] pluginval (strictness 10): automatizado en CI, falta ver la primera ejecución en verde.
