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
│   ├── LevelMeter.h         medidores de pico y RMS (hilo de audio → interfaz, atómicos)
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
│   ├── Color/               Módulo 2
│       ├── ColorProcessor.*     Saturación → Tone → Magic Chorus (siempre 100 % wet)
│       ├── ColorParameters.h    parámetros, rangos y valores por defecto
│       ├── SaturationProcessor.* Tube / Tape / Console con ADAA
│       ├── ToneProcessor.*      inclinación tonal bipolar
│   │   └── ChorusProcessor.*    Classic / Dimension / Ensemble
│   ├── Movement/            Módulo 3
│       ├── MovementProcessor.*  GranularEngine → Tremolo (siempre 100 % wet)
│       ├── MovementParameters.h parámetros, rangos y configuración de cada Style
│       ├── GrainBuffer.h        buffer circular estéreo (2 s, potencia de dos)
│       ├── GrainWindow.h        ventana Hann en tabla
│       ├── GrainVoice.h         un grano (32 voces fijas)
│       ├── GrainGenerator.*     cuándo nace cada grano y cómo es, según el Style
│       ├── GranularEngine.*     captura + voces + feedback
│   │   └── TremoloProcessor.*   trémolo senoidal con fase estéreo
│   ├── Space/               Módulo 4
│       ├── SpaceProcessor.*     Delay → Reverb, mezcla y Width
│       ├── SpaceParameters.h    parámetros, divisiones de tempo y ajuste de cada tipo de reverb
│       ├── DelayProcessor.*     delay estéreo con feedback cruzado y filtrado
│   │   └── ReverbProcessor.*    red de retardos con realimentación (8 líneas), Room / Plate / Hall
│   └── Engine/              Módulo 5
│       ├── VintageEngine.*      calcula los desvíos y los suma a los parámetros
│       ├── VintageEngineParameters.h  Flux, Rate, Depth, Character y alcance de cada destino
│       ├── Magnitude.h          Magnitude: factor global sobre las cantidades de efecto
│       └── ModulationSource.h   señal aleatoria suave (interpolación coseno)
├── Chain/                 SIN JUCE — el plugin entero
│   ├── VintageChain.*       cadena + Vintage Engine + Magnitude + medidores
│   └── ChainParameters.h    todos los parámetros de un bloque, como valores simples
├── Plugin/
│   └── ParameterManager.*   APVTS ↔ structs del Core, estado versionado
└── PluginProcessor.*        adaptador JUCE fino (buses, parámetros, estado) sobre VintageChain
Tests/
├── CoreTests.cpp            pruebas del Core (sin JUCE)
├── TextureTests.cpp         pruebas del Módulo 1 (sin JUCE)
├── ColorTests.cpp           pruebas del Módulo 2 (sin JUCE)
├── MovementTests.cpp        pruebas del Módulo 3 (sin JUCE)
├── SpaceTests.cpp           pruebas del Módulo 4 (sin JUCE)
├── EngineTests.cpp          pruebas del Módulo 5 (sin JUCE)
└── IntegrationTests.cpp     los cinco módulos juntos, Magnitude y medidores (sin JUCE)
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
- La mezcla del coro es de potencia constante (coseno/seno), para que Magic Chorus no baje el nivel.
- `ChorusProcessor` conserva los modos Classic/Dimension/Ensemble y sus parámetros completos para uso futuro; Color solo expone la perilla.
- **Valores por defecto:** Drive 25 % Tube, Tone 0, Magic Chorus 50 %. Con Drive, Tone y Magic Chorus a 0, el módulo es transparente bit a bit.
- Los parámetros eliminados (Focus y los controles del chorus) se ignoran al cargar estados antiguos.

## Módulo 3 — Movement

Flujo: `entrada → motor granular (Grain, Feedback, Style) → Tremolo (Depth, Rate)`.

| Control | ID | Qué hace |
|---|---|---|
| **Grain** | `movGrain` | Cantidad de señal granular: `salida = entrada·cos(g·π/2) + granos·sin(g·π/2)` (potencia constante: un fundido lineal restaba unos 3 dB en el centro). A 0 el módulo es transparente bit a bit |
| **Feedback** | `movFeedback` | Realimenta la salida granular al buffer de captura (ganancia máxima 0,85, con `tanh`, siempre decae) |
| **Tremolo** | `movTremolo` | Profundidad del trémolo (0 = apagado). La ganancia oscila entre 1 y `1 − profundidad` |
| **Rate** | `movRate` | Velocidad del trémolo, 0,1–15 Hz |
| **Style** | `movStyle` | **Frozen / Drift / Cloud** (ver abajo) |

Más el **encendido** del slot (`movEnable`) y su **Dry/Wet** (`movMix`).

| Style | Comportamiento |
|---|---|
| **Frozen** | La captura se detiene y los granos (220 ms, 14/s) repiten los últimos 0,4 s congelados, como un colchón sostenido. Con Frozen elegido **Feedback no actúa** (no se escribe nada en el buffer). Si se elige Frozen sin haber pasado antes audio por otro estilo, el buffer está vacío y no suena nada granular |
| **Drift** | Granos de 160 ms, 18/s; el punto de lectura vaga lentamente por el pasado reciente (0,12–0,62 s), con micro-desafinación (±8 cents) |
| **Cloud** | Granos de 100 ms, 32/s en posiciones aleatorias de hasta 1 s atrás, muy abiertos en estéreo; algunos granos suenan una octava arriba (12 %) o abajo (8 %) |

- Size, Density, Pitch, Spread y Freeze **no son controles**: son valores fijos de cada Style (`MovementParameters.h`).
- **32 voces fijas, sin reservas dinámicas.** Si todas están ocupadas, el grano nuevo se omite.
- Los granos usan ventana Hann (sin clicks), interpolación lineal e índices con máscara, así que nunca leen fuera del buffer. El punto de lectura se mantiene siempre detrás del de escritura durante todo el grano.
- Nivel normalizado por potencia: el nivel granular se mantiene cerca del de la entrada en los tres estilos.
- Trémolo senoidal; en estéreo el canal derecho va un cuarto de ciclo detrás.
- Sin latencia. Los cambios de parámetros se suavizan 20 ms.
- **Valores por defecto:** Grain 40 %, Feedback 30 %, Tremolo 25 %, Rate 4 Hz, Style Cloud.

## Módulo 4 — Space

Flujo: `z = entrada + Delay · eco(entrada)` → `salida = z + Mix · reverb(z)`. La reverb va después del delay, así que los ecos también reciben reverb.

| Control | ID | Qué hace |
|---|---|---|
| **Delay** | `spcDelay` | Cantidad de delay que se suma a la señal (0 = sin delay) |
| **Time** | `spcTime` | Sync apagado: 20 ms a 1,5 s (exponencial). Sync encendido: elige la división musical |
| **Feedback** | `spcFeedback` | Repeticiones (ganancia máxima 0,92; siempre decae) |
| **Sync** | `spcSync` | Botón: el delay sigue el tempo del host |
| **Mix** | `spcReverbMix` | Cantidad de reverb que se suma a la señal |
| **Decay** | `spcDecay` | Duración de la cola (RT60); el rango depende del Type |
| **Type** | `spcType` | **Room** (0,2–2,5 s), **Plate** (0,4–6 s), **Hall** (0,8–12 s) |
| **Width** | `spcWidth` | Apertura estéreo: en el delay es el ping-pong (0 = mono, 100 % = los ecos rebotan entre izquierda y derecha); en la reverb escala el componente lateral (0 = mono, 50 % = imagen natural, 100 % = el doble de ancho) |

Más el **encendido** del slot (`spcEnable`) y su **Dry/Wet** (`spcMix`). El ID `spcMix` es el Dry/Wet del slot; la cantidad de reverb es `spcReverbMix`.

- **Sync:** con Sync encendido, Time recorre 8 divisiones: 1/16, 1/8T, 1/8, 1/4T, 1/8., 1/4, 1/4. y 1/2, calculadas con el BPM del host (120 BPM si el host no lo da). El texto del parámetro muestra los milisegundos y la división (`215 ms | 1/8`); la división es la que vale con Sync encendido. El retardo máximo es de 3 s: con tempos muy lentos se limita.
- Los cambios de Time (y de tempo) **se deslizan** en 0,12 s, como una cinta: sin clicks, con un ligero cambio de tono.
- Las repeticiones se vuelven más oscuras y delgadas a cada pasada (paso alto 120 Hz y paso bajo 5,5 kHz en el lazo) y el lazo está limitado con `tanh`.
- **Reverb:** 8 líneas con mezcla Hadamard, difusión de entrada, pre-delay fijo por tipo (Room 4 ms, Plate 8 ms, Hall 30 ms), amortiguación de agudos y modulación lenta contra el sonido metálico. El nivel de la cola se mantiene casi constante al cambiar Decay.
- Cambiar de **Type** hace un fundido de 50 ms entre motores. Un motor apagado no consume CPU.
- Con Mix a 0 la reverb se vacía; al subirla de nuevo no vuelve una cola vieja.
- Low Cut, High Cut, Duck y Pre-Delay no son controles: son valores fijos.
- Sin latencia. Con Delay y Mix a 0 el módulo es transparente bit a bit.
- **Valores por defecto:** Delay 25 %, Time 55 %, Feedback 35 %, Sync apagado, Mix 25 %, Decay 50 %, Type Hall, Width 80 %.

## Módulo 5 — Vintage Engine

En el host los parámetros aparecen agrupados por sección: **Master**, **Texture** (incluye Vinyl Amount), **Color**, **Movement**, **Space** y **Vintage Engine**. Los grupos solo cambian cómo se listan: los IDs y los estados guardados no cambian.

No es un módulo de audio: genera variaciones lentas y suaves y las **suma como pequeños desvíos** a los parámetros que fijaron el usuario o el host, justo antes de entregarlos al DSP de cada módulo. **Nunca escribe en los parámetros del host**, así que la automatización y los presets no se tocan (`PluginProcessor` lee el valor del host, el motor le suma su desvío y el resultado se recorta al rango válido).

| Control | ID | Qué hace |
|---|---|---|
| **Vintage Engine On** | `engEnable` | Encendido del módulo. Apagado, todos los desvíos vuelven a 0 en unos 60 ms (sin saltos) |
| **Flux** | `engFlux` | Intensidad de la aleatoriedad. A 0 el motor está apagado y todos los parámetros pasan sin cambios |
| **Rate** | `engRate` | Velocidad de la variación (0,04–1,2 Hz de base, exponencial) |
| **Depth** | `engDepth` | Tamaño del desvío en cada destino |
| **Character** | `engCharacter` | **Slow:** deriva muy lenta. **Organic:** tres velocidades mezcladas (×1, ×2,3, ×5,1). **Random:** más rápido e irregular (×3 y ×5,3 con intervalos variables) |

El desvío máximo de cada destino es `Flux · Depth · alcance`:

| Destino | Alcance (a Flux = Depth = 100 %) |
|---|---|
| Wow Depth / Wow Rate | ±45 % / ±0,8 Hz |
| Flutter Depth / Flutter Rate | ±40 % / ±6 Hz |
| Vinyl Amount | ±30 % |
| Drive (Color) | ±40 % |
| Grain (Movement) | ±40 % |
| Tremolo Rate (Movement) | ±5 Hz |
| Width (Space) | ±45 % |

- Cada destino tiene su propia señal independiente (correlación entre destinos medida: 0,16 como máximo).
- **Un valor a 0 se queda en 0.** Los destinos de cantidad (Wow Depth, Flutter Depth, Vinyl Amount, Drive, Grain) solo se mueven si están encendidos: el desvío se desvanece al acercarse a 0 y es completo a partir del 10 %. Así el motor nunca activa por sí solo un efecto que dejaste apagado. Rates y Width se mueven siempre.
- Todo pasa por un suavizado de 60 ms: ni cambiar de Character ni mover Flux produce saltos.
- Es determinista: con la misma semilla, el mismo resultado tras un `reset()`.
- Las estadísticas (tamaño y velocidad de las variaciones) no dependen de la frecuencia de muestreo ni del tamaño de bloque.
- Sin asignaciones de memoria. Coste despreciable (~0,005 % de un núcleo).
- **Valores por defecto:** Flux 60 %, Rate 50 %, Depth 70 %, Character Organic. El plugin queda **vivo** de serie; para oírlo estático, Flux a 0.

## Módulo 6 — Integración general

`VintageChain` (sin JUCE) reúne todo: `medidor de entrada → Input Gain → Texture → Color → Movement → Space → Master Dry/Wet / Output Gain / Bypass → ruido de vinilo → medidor de salida`. `PluginProcessor` solo lee los parámetros del host, se los pasa y le añade el tempo. Las pruebas de integración usan esta misma clase, así que se prueba lo que corre en el plugin.

Para cada módulo, los valores del host pasan por: **valor del host → desvío del Vintage Engine → Magnitude → módulo**. Magnitude va al final, así que a 0 nada de lo que añada el motor llega al sonido.

| Control | ID | Qué hace |
|---|---|---|
| **Magnitude** | `magnitude` | Intensidad global, 0–100 % (por defecto 100 %). Multiplica las **cantidades** de efecto; no escribe en los parámetros del host, así que tus ajustes individuales quedan como estaban |

Magnitude escala: Wow Depth, Flutter Depth, Vinyl Drop, Degrade, Vinyl Amount, Drive, Tone, Magic Chorus, Grain, Feedback (Movement), Tremolo, Delay y Mix de la reverb. **No** escala: velocidades, tipos, Time, Decay, Width, Sync, el encendido y el Dry/Wet de cada módulo, ni las ganancias y el Dry/Wet maestros. A 0 el plugin es transparente (salida idéntica a la entrada, también con el motor al máximo).

**Medidores.** `getInputMeter()` y `getOutputMeter()` dan pico (caída de 0,5 s) y RMS (0,3 s) por canal, calculados muestra a muestra en el hilo de audio y publicados con atómicos; el resultado no depende del tamaño de bloque. Ignoran NaN/Inf. La interfaz del Módulo 7 los leerá.

**Notas de integración**
- Con los valores por defecto la salida queda a unos −0,3 dB de la entrada (señal tonal) y la suma mono conserva el 73 % del nivel estéreo: es un sonido muy ancho (correlación L/R casi 0), pero sin cancelaciones.
- El Vintage Engine se actualiza una vez por bloque del host (con 60 ms de suavizado). Con el motor apagado, la salida es idéntica bit a bit para cualquier tamaño de bloque; con el motor encendido, es muy parecida pero no idéntica.
- Tras `reset()` con el motor encendido, los módulos arrancan desde sus últimos valores, así que la repetición es muy parecida, no idéntica.

## Pruebas

`ColorTests` (91 comprobaciones): salida idéntica con todo apagado, **armónicos medidos por FFT**, **aliasing medido** con y sin ADAA, compensación de nivel, respuesta de Tone, **desviación de tono del chorus medida** a 5 frecuencias de muestreo, modos, Mix, Width y la perilla Magic Chorus, cambios de modo/tipo sin clicks, independencia del tamaño de bloque, valores extremos y NaN/Inf, mono, reinicio repetible, cero asignaciones y CPU.

`TextureTests` (67 comprobaciones): retardo Hermite, transparencia con todo apagado, **desviación de tono medida por cruces por cero**, drop, respuesta de la perilla Degrade, tamaño de bloque, frecuencias de 22,05 a 192 kHz, valores extremos y NaN/Inf, mono, reinicio, cero asignaciones y CPU.

`IntegrationTests` (178 comprobaciones): ajustes neutros, bypass y Dry/Wet = 0 son un cable; Magnitude 0 es un cable con todo al máximo y el motor encendido; Magnitude 50 % equivale exactamente a poner a mano la mitad de cada cantidad (y no toca nada más); Magnitude NaN o fuera de rango; efecto creciente con Magnitude en siete efectos por separado; automatizar Magnitude sin clicks; el motor no enciende un efecto a 0; las 16 combinaciones de encendido de los módulos; encender y apagar módulos en marcha; bloques de 1 a 4096 (idénticos con el motor apagado); 22,05 a 192 kHz en mono y estéreo; nivel y fase con valores por defecto; medidores (pico, RMS, canales independientes, bloques, silencio, NaN); automatización aleatoria de todos los parámetros durante 40 s; 120 s con los ajustes más duros sin crecer ni producir NaN; reinicio; cero asignaciones; CPU de los cinco módulos (unos 35x tiempo real, 3 % de un núcleo).

`EngineTests` (155 comprobaciones): exactamente transparente con Flux o Depth a 0 o con el módulo apagado (sin salto al apagarlo) (también al volver a 0), desvíos acotados y en movimiento en los tres caracteres, proporcionales a Flux y a Depth, sin saltos (paso máximo por bloque, también al cambiar Character/Rate/Flux de golpe), determinista y repetible tras `reset()`, velocidad Slow < Organic < Random y creciente con Rate, destinos independientes, estadísticas iguales de 22,05 a 192 kHz y con bloques de 1 a 8192 muestras, recorte a los rangos con parámetros en los extremos, no toca los parámetros que no son destino, valores NaN/Inf, módulos reales alimentados durante 30 s sin salirse de rango, cero asignaciones y CPU.

`SpaceTests` (124 comprobaciones): transparencia con todo apagado, tiempo del delay medido (20 ms a 1,5 s), sincronización a varios tempos y divisiones, tempo cambiante, repeticiones de feedback que decaen, ping-pong, repeticiones más oscuras, cambios de tiempo sin clicks, **RT60 medido** (integral de Schroeder) en los tres tipos, nivel y Mix de la reverb, Width, cambio de Type sin clicks, estabilidad con Decay y Feedback al máximo durante 60 s, tamaño de bloque, frecuencias de 22,05 a 192 kHz, valores extremos y NaN/Inf, mono, reinicio, sin cola vieja, cero asignaciones y CPU (unos 110x tiempo real).

`MovementTests` (70 comprobaciones): transparencia con Grain y Tremolo a 0, Grain creciente en los tres estilos, nivel normalizado, estilos distintos y Cloud ancho, Frozen sostiene el sonido tras cortar la entrada (Cloud no), Feedback alarga la cola y se mantiene acotado con entrada fuerte y 20 s, Tremolo (profundidad, velocidad y fase estéreo medidas), cambios de parámetros sin clicks, resultados idénticos con cualquier tamaño de bloque (1 a 512), frecuencias de 22,05 a 192 kHz, valores extremos y NaN/Inf, mono, silencio, reinicio repetible, límite de voces, cero asignaciones y CPU (unos 330x tiempo real).

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
- [ ] Movement en FL Studio: probar los tres Style con material rítmico y sostenido, subir Feedback al máximo (debe decaer, sin chillidos ni saltos de volumen), mover Tremolo/Rate y comprobar que no hay clicks al cambiar de Style.
- [ ] Space en FL Studio: activar Sync y comprobar que el delay sigue el tempo del proyecto (y los cambios de tempo), Feedback y Decay al máximo (deben decaer), cambiar Type en reproducción sin clicks y mover Width con material estéreo.
- [ ] Vintage Engine en FL Studio: con Flux a 0 el sonido debe ser idéntico al de antes; subir Flux/Depth y escuchar la deriva del wow, el flutter, el ruido y la saturación en los tres Character, sin saltos ni cambios bruscos de volumen; automatizar un parámetro destino (por ejemplo Drive) y comprobar que la automatización sigue mandando.
- [ ] Magnitude en FL Studio: a 0 el sonido debe ser el original; moverla en reproducción no debe hacer clicks, y subirla debe aumentar de forma pareja todos los efectos. Los ajustes de cada módulo no deben cambiar al moverla.
- [ ] pluginval (strictness 10): automatizado en CI, falta ver la primera ejecución en verde.
