# Vintage Maker

Multiefecto VST3 de carácter cinematográfico, analógico y lo-fi (C++20, JUCE 9.0.5, CMake).

Cadena: `INPUT → TEXTURE → COLOR → MOVEMENT → SPACE → OUTPUT`

> Uso personal. No se distribuye.

## Estado

| Módulo | Estado |
|---|---|
| 0 — Core y fundamentos | **Validado** (Windows + FL Studio) |
| 1 — Texture | **Implementado y probado en Linux.** Falta validar en Windows y FL Studio |
| 2 — Color | **Implementado y probado en Linux.** Falta validar en Windows y FL Studio |
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
│   ├── DelayLine.h          retardo con interpolación Hermite (compartido por Texture y Color)
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
│   ├── Texture/             Módulo 1
│   │   ├── TextureProcessor.*   orquesta los componentes (siempre 100 % wet)
│   │   ├── TextureParameters.h  parámetros, rangos y valores por defecto
│   │   ├── WowFlutter.*         modulación de tono con línea de retardo
│   │   ├── VinylDrop.h          el "drop" del vinilo (bajada de tono y nivel)
│   │   └── DegradeProcessor.*   transitorios, saturación y ancho de banda
│   └── Color/               Módulo 2
│       ├── ColorProcessor.*     Saturación → Tone → Magic Chorus (siempre 100 % wet)
│       ├── ColorParameters.h    parámetros, rangos y valores por defecto
│       ├── SaturationProcessor.* Tube / Tape / Console con ADAA
│       ├── ToneProcessor.*      inclinación tonal bipolar
│       └── ChorusProcessor.*    Classic / Dimension / Ensemble
├── Plugin/
│   └── ParameterManager.*   APVTS ↔ structs del Core, estado versionado
└── PluginProcessor.*        adaptador JUCE fino (buses, parámetros, estado)
Tests/
├── CoreTests.cpp            pruebas del Core (sin JUCE)
├── TextureTests.cpp         pruebas del Módulo 1 (sin JUCE)
└── ColorTests.cpp           pruebas del Módulo 2 (sin JUCE)
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

Flujo: `entrada → Wow + Flutter → Degrade → caída de nivel del Drop`.

El **ruido de vinilo** (hiss + crackle) **no está en la cadena**: lo genera el `MasterProcessor` y se suma al final, después de Output Gain, Master Dry/Wet y Bypass, así que ningún otro módulo lo altera. Solo se apaga con su propia perilla, o si el plugin está en bypass.

| Control | ID | Qué hace |
|---|---|---|
| **Wow Depth** | `texWowDepth` | Retardo variable de hasta ±8 ms, con deriva aleatoria suave |
| **Wow Rate** | `texWowRate` | 0,1–2 Hz |
| **Flutter Depth** | `texFlutterDepth` | Retardo variable de hasta ±0,6 ms |
| **Flutter Rate** | `texFlutterRate` | 4–15 Hz |
| **Vinyl Amount** | `texVinylAmount` | Nivel del hiss rosa y del crackle (ruido global, fuera de la cadena) |
| **Vinyl Drop** | `texVinylDrop` | Bajada ocasional de tono y nivel (cada ~30 s con 0 % hasta ~5 s con 100 %) |
| **Degrade** | `texDegradeAmount` | **Una sola perilla**: Amount, Bandwidth y Smoothing juntos. Sube el suavizado de transitorios y la saturación, y estrecha el ancho de banda (de 40 Hz–12 kHz a unos 130 Hz–2,4 kHz) |

Más el **encendido** del slot (`texEnable`) y su **Dry/Wet** (`texMix`).

- **Sin latencia.** El retardo es `profundidad × (1 + modulación)`, así que con Wow, Flutter y Drop a 0 la señal pasa bit a bit. El retardo medio crece con la profundidad (hasta unos 8 ms con Wow al máximo).
- Desviación de tono: Wow al 100 % y 1 Hz, ±5 %; Flutter al 100 % y 10 Hz, ±3,8 %.
- Los cambios de profundidad son lentos a propósito (0,5 s en Wow).
- Valores fijos internos: Wow Shape 0 (seno + deriva), Flutter Variation 30 %.
- El ruido tiene semillas fijas: dos renders iguales dan el mismo resultado.
- **Valores por defecto:** Wow 25 %, Flutter 15 %, Vinyl Amount 20 %, Degrade 25 %. El plugin **no es transparente** con los valores por defecto.
- Los parámetros eliminados (Shape, Variation, Crackle Type, Degrade Bandwidth/Smoothing) se ignoran al cargar estados antiguos.

## Módulo 2 — Color

Flujo: `entrada → Saturación → Tone → Magic Chorus`. Tres perillas y un selector.

| Control | ID | Qué hace |
|---|---|---|
| **Drive** | `colDrive` | De 0 a +24 dB hacia la curva, con compensación de nivel |
| **Color Type** | `colType` | **Tube:** tanh con sesgo (pares e impares) y bloqueador de DC. **Tape:** curva simétrica (impares) con pérdida de agudos creciente. **Console:** recorte cúbico simétrico, más seco |
| **Tone** | `colTone` | −100…+100 %: inclinación de ±6 dB alrededor de 700 Hz |
| **Magic Chorus** | `colChorusAmount` | **Una sola perilla**: 0 = apagado (bit a bit); al subir aumenta la mezcla del coro. Modo Ensemble (3 voces) fijo, Rate 0,6 Hz, Depth y Width altos |

Más el **encendido** del slot (`colEnable`) y su **Dry/Wet** (`colMix`).

- **Anti-aliasing:** ADAA de primer orden, sin latencia ni sobremuestreo (unos 9 dB menos de aliasing con Drive al máximo). El oversampling 2x/4x llega en el Módulo 9.
- Cambiar el tipo de saturación hace un fundido de 10 ms, sin clicks.
- `ChorusProcessor` conserva los modos Classic/Dimension/Ensemble y sus parámetros completos para uso futuro; Color solo expone la perilla.
- **Valores por defecto:** Drive 25 % Tube, Tone 0, Magic Chorus 50 %. Con Drive, Tone y Magic Chorus a 0, el módulo es transparente bit a bit.
- Los parámetros eliminados (Focus y los controles del chorus) se ignoran al cargar estados antiguos.

## Pruebas

`ColorTests` (91 comprobaciones): salida idéntica con todo apagado, **armónicos medidos por FFT**, **aliasing medido** con y sin ADAA, compensación de nivel, respuesta de Tone, **desviación de tono del chorus medida** a 5 frecuencias de muestreo, modos, Mix, Width y la perilla Magic Chorus, cambios de modo/tipo sin clicks, independencia del tamaño de bloque, valores extremos y NaN/Inf, mono, reinicio repetible, cero asignaciones y CPU.

`TextureTests` (67 comprobaciones): retardo Hermite, transparencia con todo apagado, **desviación de tono medida por cruces por cero**, drop, respuesta de la perilla Degrade, tamaño de bloque, frecuencias de 22,05 a 192 kHz, valores extremos y NaN/Inf, mono, reinicio, cero asignaciones y CPU.

`CoreTests` (264 comprobaciones) cubre, además, el ruido de vinilo (nivel, estéreo, independencia de la cadena, bloques, asignaciones):

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
- [ ] Color en FL Studio: comparar Tube / Tape / Console, mover Tone y Magic Chorus en estéreo y comprobar que cambiar de tipo no hace clicks.
- [ ] Texture en FL Studio: oír cada sección por separado (Wow, Flutter, Vinyl Amount/Drop, Degrade) y comprobar que el vinilo suena igual con Color activado o apagado y comprobar que el Dry/Wet y el encendido no hacen clicks.
- [ ] pluginval (strictness 10): automatizado en CI, falta ver la primera ejecución en verde.
