# Firmware propio para M-VAVE FM-1 — Entrega 1: investigación y diseño

Estado: diseño, sin código. 11 de noviembre de 2026.
Nombre provisional: **[por decidir]** (propuestas en la sección D).

---

## 0. Lo que contradice el planteamiento inicial (léelo primero)

1. **Melodee no es la mejor base para un firmware nuevo.** Es una aplicación de ~50.000 líneas de C (groovebox de 4 partes con FM6, Prophet, CZ, SID, SLICE…) con la capa de hardware dentro del árbol y el emulador construido sobre su propio arnés de tests. Para hacer *tu* firmware habría que extirpar casi todo. Existe una base pensada justo para esto: **fm1-base** (Charles Vestal) sobre **felucca-bsp** (la capa de hardware que Leo Kuroshita separó de Felucca). Ver sección A.4 y la pregunta 1.
2. **Melodee está escrito en C (gnu99), no en C++.** Igual que Felucca, felucca-bsp y fm1-base. El compilador del dispositivo es el clang 4.0.1 de JieLi (2017) para la CPU pi32v2. Que compile C++ freestanding **no está verificado** (no he podido descargar el toolchain: el servidor de JieLi me devuelve 403). Ver A.5.
3. **"Una sola zona de firmware" es cierto, pero ya no es todo o nada.** Melodee ≥ 1.3.5 lleva un *boot stub* y un cargador de actualización que se queda en flash (0xE0000): si una escritura se corta, al reiniciar vuelve al modo de actualización. Y si todo falla, el chip tiene un modo UBOOT en ROM con herramientas de recuperación. Tu regla (no flashear hasta que lo pidas) se mantiene igual, pero el riesgo real se puede bajar mucho si nuestro firmware hereda ese mecanismo.
4. **El jack TRS del FM-1 solo es de entrada MIDI.** El FM-1 no puede mandar clock a tu Digitone II ni al OXI One por cable TRS; solo por USB o Bluetooth.
5. **El emulador no mide la CPU real.** Todo lo que diseñemos en el navegador puede no caber en el chip. Lo mitigamos con presupuestos conservadores y medidas relativas, pero la verdad solo llega con la primera prueba en hardware.

---

## A. Lo encontrado en los repos base

Repos clonados y leídos (commit de cada uno):

| Repo | Commit | Licencia |
|---|---|---|
| keremimo/melodee | `165d177` (2 nov 2026, línea 1.6.1) | GPL-3.0-only |
| hugelton/Felucca | `a4f007c` (4 nov 2026) | GPL-3.0-only |
| hugelton/felucca-bsp (submódulo de Felucca) | `5942e68` | MPL-2.0 |
| charlesvestal/fm1-base | `b7b25a4` (11 nov 2026) | MPL-2.0 |

### A.1 Hardware (verificado en código o documentación de los repos)

| Elemento | Dato | Dónde lo he visto |
|---|---|---|
| Chip | JieLi AC791N / WL82, CPU pi32v2, **dos núcleos** | docs de Melodee (BLE-HW-FACTS, BUILDING) |
| Reloj | 360 MHz (lo fija el bootloader; Melodee nunca toca el PLL) | `firmware/hal/fm1_clock.h` |
| FPU | precisión simple (float), sin double nativo | wiki de Melodee; regla "no double" de fm1-base |
| SRAM | ~500 KB en total repartidos por el linker de Melodee (96 KB RAM + ~350 KB pool + 32 KB código en RAM + 15 KB retenidos). **Sin RAM externa** | `firmware/app.ld`; README fm1-base |
| Flash | 1 MiB de serie. El código de la app cabe en **560 KiB** máx. (0x05000–0x90FFF) | `docs/BOOT-SAFETY.md`, `app.ld`, felucca-bsp `EXPERIMENTAL.md` |
| Audio | I2S a códec externo, 24 bit, **44.117,6 Hz** (≈44,1 kHz), doble buffer de 128 frames (2,9 ms) en Melodee; 256 frames por defecto en fm1-base | `firmware/src/audio.c`, `core.h`; `template/app.h` |
| Pantalla | 240×240, controlador clase ST7789, **RGB565** por SPI | `firmware/src/lcd.c`, `hal/fm1_lcd_hw.h` |
| Matriz de panel | 11 columnas × 6 filas con diodos, escaneo por interrupción a ~10 kHz | `hal/fm1_input.h` |
| Botones | **14**: FX, SCL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC, OCT−, OCT+ | `firmware/src/panel.c` |
| Encoders | **7, sin fin, con detent y sin pulsador**: SELECT, ALGORITHM, PRESETS, KNOB 1–4 | `hal/fm1_input.h`, `panel.c` |
| MASTER | **potenciómetro analógico** (ADC 10 bit), no encoder | `hal/fm1_adc.h` |
| Teclado | **27 teclas, F3 a G5** (16 blancas + 11 negras) | `hal/fm1_input.h` |
| LEDs | en botones y teclas, con niveles de brillo por PWM | `hal/fm1_input.h` |
| Velocidad/presión en teclas | existe lectura experimental, desactivada por defecto | felucca-bsp README, fm1-base `APP_TOUCH` |
| MIDI | USB (dispositivo), **TRS solo entrada**, Bluetooth MIDI (pila propia), USB host | `include/plat.h` de fm1-base ("the TRS jack, which has no out"), BUILDING de Melodee |
| USB audio | Melodee: estéreo o 4 pistas mono. fm1-base: 2, 4, 6, 8 o 10 canales | BUILDING de Melodee, `tools/build.py` de fm1-base |
| Batería | lectura por ADC | `hal/fm1_adc.h` |

Dos avisos de hardware que condicionan el diseño:

- **Fallo de CPU documentado:** multiplicaciones de 64 bit en línea entre lecturas de 16 bit corrompen registros de vez en cuando (felucca-bsp `docs/CPU.md`). Conclusión: DSP en `float`, sin `int64` en bucles calientes.
- **El segundo núcleo falla en algunas unidades** (lecturas erróneas de RAM compartida). Melodee y la BSP lo detectan y vuelven a un núcleo. Conclusión: **el diseño tiene que caber en un solo núcleo**; el segundo es un extra.
- **El mapa botón → serigrafía de Melodee es "la mejor suposición"**, con PLAY y REC medidos en hardware y un modo de calibración (OCT− y OCT+ al encender). fm1-base trae su propio mapa; lo comprobaré en la Fase 0.

### A.2 Toolchain y compilación del firmware

- **Compilador:** clang 4.0.1 de JieLi para pi32v2, solo Linux x86-64. En Mac, Melodee lo ejecuta en Docker (`linux/amd64`, Rosetta en Apple Silicon).
- **SDK:** tres ficheros del SDK AC79 de JieLi (Apache-2.0), clonado desde gitee.
- **Resultado:** un paquete `.fwsc` (app + cargador de actualización).
- **Releases de Melodee:** se compilan y firman en GitHub Actions con un flujo compartido (`zvenson/fm1-build`). fm1-base usa su propio `tools/build.py`, configurado con un `fm1.toml`.

### A.3 Emulador de navegador (verificado en `web/emu/` de Melodee)

- `web/emu/build.sh` compila con **Emscripten (emcc)** el *mismo C del firmware*, con el hardware simulado, a WebAssembly. Lo hace a través del arnés de tests de Melodee (`tests/ui_test.c` y `hostsim.c`).
- Corre en un AudioWorklet: cada bloque de audio avanza el reloj del "dispositivo"; la UI se dibuja cada 15 ms, como en el bucle principal.
- **Lo que no emula:** flash (no guarda), USB (ni MIDI out ni audio USB), MIDI TRS, batería.
- Se sirve con `python3 -m http.server`.
- Crédito: David Konsumer, PR #53 de Melodee.
- fm1-base hace lo mismo para cualquier app (`tools/build.py APP --web`) e incluye remuestreo 44,1 → 48 kHz. Hay una demo pública de su plantilla.

### A.4 Cómo se flashea

- **Instalador web** (Chrome/Edge, MIDI por USB) o `tools/fm1_install.py`. Guarda una copia completa antes de escribir.
- **Recuperación:**
  - Con *boot stub*: OCT− al encender → modo actualización.
  - Sin él: el modo ROM "WL80UBOOT", con la herramienta de recuperación de Melodee o con FM-1 Transporter (requiere una XIAO RP2040 cableada).
  - Para cambiar entre firmwares de terceros se recomienda pasar antes por **TransitFM1 0.1.3**.
- Cada firmware necesita una **identidad de paquete propia** (`FM-1_` + dígitos).

### A.5 Licencias: lo que importa

- Melodee y Felucca: **GPL-3.0-only**. Cualquier código que tomemos de ellos hace nuestro firmware GPL-3.0-only (no "or later").
- felucca-bsp y fm1-base: **MPL-2.0**, compatible con GPL: nuestra app puede ser GPL-3.0.
- **Bluetooth:** las librerías binarias de JieLi no pueden distribuirse dentro de un firmware GPL sin permisos extra. Hay que usar una **pila propia**: la de Melodee (sala limpia, propia) o la de fm1-base (`ble_stack = "own"`, de Optimist). Ambas están probadas con Mac e iPhone.
- **Código reutilizable con licencia compatible:** Mutable Instruments (Plaits, Rings, Clouds; MIT), DaisySP (MIT). Make Noise, Verbos, Elektron, Iridium y SM01 son cerrados: solo pueden servir de inspiración.

### A.6 Qué no he podido verificar

- **No he compilado nada.** En este entorno no hay emcc y el toolchain de JieLi no se puede descargar (403).
- El soporte de C++ del toolchain de JieLi. Hay que comprobar tres cosas: que compila, que enlaza sin runtime y que el arranque ejecuta los constructores estáticos.
- El margen real de CPU y de RAM que queda para nuestra app.
- FM1 Open, el firmware abierto oficial de M-VAVE: existe (anunciado el 20 de octubre), pero no he revisado su código y su licencia no está clara.
- Que el mapa de botones coincida con tu unidad.

### A.7 Límites prácticos (tu pregunta "¿cuáles son las limitaciones?")

| Recurso | Límite | Consecuencia de diseño |
|---|---|---|
| CPU | 360 MHz / 44.117 Hz ≈ **8.160 ciclos por muestra** en total para un núcleo, compartidos con UI, escaneo del panel y USB | Objetivo: audio ≤ 60 % de un núcleo (~4.900 ciclos/muestra). 4 pistas con motores medianos + FX: probable. 6 pistas con motores pesados: dudoso sin el segundo núcleo |
| RAM | ~500 KB en total, sin RAM externa; la plataforma (USB, BLE, buffers de pantalla) se lleva una parte grande. **Estimación**: 150–250 KB para la app | Sin asignación dinámica: todo estático, dimensionado al compilar |
| Pantalla | Un framebuffer completo RGB565 ocupa **115.200 B**: no cabe cómodo | Dibujo por franjas y solo lo que cambia |
| Delay | 1 s estéreo en float = 352 KB: **imposible** | Delay dub en 16 bit a 22 kHz: 1,5 s mono = **66 KB**; el estéreo, por ping-pong o desfase corto |
| Reverb | Una reverb tipo Clouds usa un buffer de 16 bit de **~32 KB** (estimado) | Una sola reverb global |
| Flash | 560 KiB de código, 1 MiB en total | Nada de librerías de samples; tablas pequeñas |
| Granular | 0,75 s a 22 kHz ≈ 33 KB | Posible, pero corto; queda para el final |
| Controles | Encoders sin pulsador; MASTER es absoluto | Las funciones secundarias van con HOME como SHIFT |
| MIDI | TRS solo de entrada | El FM-1 puede seguir clock por TRS, pero solo puede mandarlo por USB o BT |
| Batería | Más CPU, menos autonomía | Medir en la Fase 7 |

---

## B. Propuesta de arquitectura

### B.1 Base de software (recomendación, pendiente de que decidas)

**Recomiendo un proyecto nuevo sobre fm1-base + felucca-bsp**, con la app en GPL-3.0:

- **API limpia:** `app_init`, `app_frame` (UI, ~60/s), `app_render` (audio) y `app_tick` (1 kHz, para el secuenciador).
- **El mismo código en tres sitios:** simulador en el ordenador, emulador en el navegador y dispositivo.
- **Bluetooth sin binarios de JieLi**, USB audio de hasta 10 canales, guardado en flash, backup e instalador web generado.

Contras:

- Es beta: su API puede cambiar antes de la 1.0.
- Lleva menos horas en hardware que Melodee.

Melodee se queda como referencia y de ella tomaríamos piezas concretas (por ejemplo, el seguimiento de clock) con su crédito. Si prefieres hacer fork de Melodee igualmente, se puede, pero el trabajo de quitar lo suyo es mayor que el de empezar limpio.

### B.2 Lenguaje y reglas del núcleo DSP

- **C++14 "de subconjunto"**, sin dependencias de plataforma:
  - sin STL, sin excepciones, sin RTTI y sin `new`/`delete`;
  - sin constructores globales (cada objeto tiene `init()` explícito);
  - sin `virtual` en el camino de audio;
  - solo `float`, sin `double`;
  - sin multiplicaciones de 64 bit en bucles calientes.
- **Plan B si la prueba de la Fase 0 falla:** el mismo diseño en C99. Con estas reglas la traducción es mecánica.
- **Memoria:** cada pista tiene un hueco de motor de tamaño fijo, una `union` del motor más grande. Cargar otro motor reinicia ese hueco: cero asignación dinámica, y el peor caso se conoce al compilar.
- **Estructura:** `dsp/` (portable, con tests en el ordenador que renderizan WAV) · `app/` (UI, secuenciador, presets) · plataforma (fm1-base).

### B.3 Pistas y motores

**4 pistas de sonido + Master + Global.** Cualquier motor en cualquier pista. Hay un medidor de coste: la UI avisa si una combinación se acerca al presupuesto de CPU.

¿Por qué 4 y no 6? Por CPU y RAM (A.7), y porque los 6 botones de pista se reparten mejor así: 4 pistas, más EDIT para el Master y GLO para Global. Si en hardware sobra CPU, se puede ampliar.

**Catálogo de motores sugerido** (elige 4–6 para empezar; ninguno copia los de Drelva Vave):

| Motor | Una frase | Referencias |
|---|---|---|
| **GRAVE** | Bombo sintetizado (seno con barrido de tono, click, saturación) más un *rumble* que sale de pasar el propio bombo por una difusión oscura, filtrada y comprimida por sidechain. | Northern Electronics, Abdulla Rashim, 909 |
| **METAL** | Banco de 6 cuadradas desafinadas (tipo 808) más ruido, por paso-bandas, con decay y acento: hats, rides y metales finos. | Mike Parker, Sleeparchive, 808/909 |
| **GOLPE** | Excitador (impulso o ruido) en un resonador de membrana o modal: toms, rimshots y percusión tonal. | Rambadu, Orphx, Basimilus (inspiración) |
| **ACORDE DUB** | 3–4 osciladores sobre un acorde (m, m7, m9, sus, 5ª, 11ª), con un filtro resonante que "pinga" en cada golpe; hecho para el delay. | Basic Channel, Deepchord, Luigi Tozzi |
| **MACRO** | Oscilador con modelos tipo Plaits (VA, waveshaper, FM 2-op, formante, aditivo, wavetable, cuerda, modal) y tres macros ARMÓNICOS / TIMBRE / MORPH con LPG. Código MIT de Mutable Instruments, con créditos. | Plaits, Mutable |
| **FM4** | FM de 4 operadores con algoritmos fijos: campanas, stabs metálicos y bajos FM (la herencia DX del FM-1, en pequeño). | Neel, Luigi Tozzi, Digitone |
| **DRONE** | Banco de resonadores excitado por ruido filtrado y sub, con deriva lenta de afinación y espectro. | Abdulla Rashim, Toki Fuko, Rings |
| **LÍNEA** | Mono saw/pulse/sub con filtro tipo diode o ladder, acento y slide. | Plastikman, 303/101 |
| **NIEBLA** | Granos sobre un buffer corto grabado del master o de una pista (limitado por RAM). Para el final. | Clouds, Hypnus |

Arranque propuesto (MVP): T1 GRAVE, T2 METAL, T3 ACORDE DUB, T4 DRONE, con MACRO disponible para cargar.

### B.4 Secuenciador (uno por pista, con modos)

- **Dos capas separadas:** RITMO y TONO, para poder hacer polimetría y generativo.
- **RITMO**, con cuatro modos:
  - **PASOS:** 1–64 pasos.
  - **EUCLID:** golpes, longitud, rotación y euclídeo de acentos.
  - **ETAPAS:** 8 etapas con nº de pulsos y modo de gate, al estilo Metropolis/ML-185.
  - **TURING:** registro de desplazamiento aleatorio que se repite, con un control de "bloqueo" que fija la probabilidad de cambio.
- **TONO:** notas fijas por paso, o cuantizadas a una escala con un generador (paseo aleatorio, Turing o arpegio de acorde).
- **Por paso:**
  - probabilidad;
  - condiciones (1:2, 3:4, FILL, NO FILL);
  - *nudge* (micro-timing);
  - p-locks limitados: hasta 4 parámetros bloqueados por patrón, para acotar la RAM.
- **Longitud y división por pista** (1/32 a 1/1 y tresillos): la polimetría es la base del techno hipnótico.
- **MUTAR:** un control de cuánto cambia el generador en cada vuelta. **GENERAR:** patrón nuevo, con **1 nivel de deshacer**.
- **8 patrones por pista** (por confirmar con la RAM y la flash).
- **Acordes generativos:** me falta saber qué es exactamente "chords disastrous peace" (pregunta 5).

### B.5 Master (botón EDIT)

| Página | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| DELAY | TIEMPO (sincronizado: 1/16 a 1/2, con puntillo y tresillo) | FEEDBACK | COLOR (LP/HP en el lazo) | RETORNO |
| DELAY 2 | WOW/FLUTTER | SATURACIÓN | ESTÉREO (ping-pong) | DLY→REV (envío del delay a la reverb) |
| REVERB | TAMAÑO | AMORTIGUACIÓN | PREDELAY | RETORNO |
| SALIDA | FILTRO DJ (izquierda LP, derecha HP) | DRIVE/CINTA | DUCK (sidechain desde la pista del bombo) | LIMITADOR |

### B.6 Páginas de cada pista (se recorren con SELECT; 4 knobs por página)

| # | Página | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| 1–3 | MOTOR A/B/C | según el motor (hasta 3 páginas) | | | |
| 4 | RITMO | MODO (Pasos/Euclid/Etapas/Turing) | LONGITUD | DIVISIÓN | SWING |
| 5 | GENERADOR | DENSIDAD/GOLPES | ROTAR | MUTAR | ESCALA/RANGO |
| 6 | MOVIMIENTO | LFO DESTINO | LFO VELOCIDAD (sincronizada) | LFO PROFUNDIDAD | DERIVA (aleatorio lento) |
| 7 | MEZCLA | NIVEL | DELAY | REVERB | DUCK |
| 8 | CANAL | MOTOR (cargar) | PAN | CORTE GRAVES | DRIVE |

Ejemplo con MACRO: A = MODELO, NOTA, ARMÓNICOS, TIMBRE · B = MORPH, DECAY, COLOR LPG, GLIDE.

### B.7 Global (botón GLO)

| Página | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| TIEMPO | TEMPO (40–200) | SWING global | RAMPA (tiempo para llegar a un tempo nuevo) | ESCALA/RAÍZ |
| CLOCK | FUENTE (INT / AUTO USB-TRS-BT) | OFFSET (±40 ms) | ENVIAR CLOCK *(pendiente en el dispositivo)* | MIDI canal base |
| SISTEMA | BRILLO | LED TECLAS *(pendiente)* | USB AUDIO (estéreo / multipista) *(pendiente)* | BT MIDI *(pendiente)* |

La rampa de tempo encaja con tu live act (80 → 128 BPM poco a poco), cuando el FM-1 va con su propio clock.

### B.8 Mapa del panel

| Serigrafía | En nuestro firmware | Con HOME pulsado (SHIFT) |
|---|---|---|
| MASTER (pot) | Volumen de salida | — |
| SELECT | Página siguiente/anterior de lo seleccionado | Tempo |
| ALGORITHM | **MUTAR** de la pista seleccionada (actuación) | Swing de la pista |
| PRESETS | Nivel de la pista seleccionada | Envío a delay de la pista |
| KNOB 1–4 | Los 4 parámetros de la página | Ajuste fino |
| FX / SCL / ENV / LFO | Seleccionar pista 1 / 2 / 3 / 4 | Mute / unmute de esa pista |
| EDIT | Master (efectos) | — |
| GLO | Global (tempo, clock, sistema) | — |
| HOME | Solo: pantalla de resumen (las 4 pistas). Mantenido: SHIFT | — |
| SAVE | Abre proyectos (slot con SELECT; SAVE guarda; OCT+ carga; OCT− cierra) | — |
| ARP | **GENERAR**: patrón nuevo para la pista | Deshacer el último GENERAR |
| SEQ | Modo PASOS: las 16 teclas blancas son los pasos y sus LEDs muestran los golpes | — |
| PLAY | Arranca/para | FILL mientras se mantiene |
| REC | Mantener: **DUB THROW** (la pista al 100 % al delay). En modo PASOS: grabación en vivo | **FREEZE** del delay (feedback infinito) |
| OCT− / OCT+ | Octava del teclado. En modo PASOS: grupo de 16 pasos anterior/siguiente | — |
| Teclas | Tocan la pista seleccionada, o transponen su patrón (ajuste por pista). En modo PASOS: blancas = pasos; mantener un paso y girar un KNOB = p-lock | — |

### B.9 Pantalla (240×240)

- **Línea superior:** pista y motor; a la derecha, el tempo (INT/EXT) y el estado de reproducción.
- **Zona central (~120 px):** la imagen propia del motor o del secuenciador (anillo euclídeo, rejilla de pasos, etapas, espectro del drone).
- **Línea de páginas:** pestañas de las páginas.
- **Abajo:** 4 tarjetas de parámetro, en el orden físico de los KNOB 1–4.
- **Pantalla HOME:** 4 carriles con la posición de cada patrón, mutes y niveles.

Estéticas posibles (pregunta 9):

- **A. "Fósforo":** negro y ámbar, como un osciloscopio.
- **B. "Monolito":** negro, blanco roto y un único acento rojo, brutalista, en la línea de Northern Electronics y Sandwell District.
- **C. "Módulo":** gris oscuro con un color por pista, como cables de patch.

Tipografía con licencia OFL: Barlow Condensed (ya está en fm1-base), IBM Plex Mono o Space Mono.

---

## C. Plan por fases

| Fase | Contenido | Qué se prueba en el emulador |
|---|---|---|
| **0. Entorno** | Repo propio con licencia y créditos. Compilar **sin cambios** el emulador y el firmware de la base (aquí, en tu Mac o con GitHub Actions). Prueba de C++ con el toolchain de JieLi. Mapa de RAM y flash reales de la plantilla. Elegir la identidad del paquete | La plantilla de la base suena y responde en el navegador |
| **1. Esqueleto** | Pantalla con páginas y tarjetas, mapa de panel, transporte, clock interno y un motor de prueba | Navegar con todos los controles, oír un tono, PLAY/STOP |
| **2. Ritmo** | Núcleo DSP con tests en el ordenador (WAV y coste relativo). GRAVE y METAL. Secuenciador PASOS + EUCLID. Mezcla y mutes | Un groove de bombo y hats con polimetría, mutes y modo PASOS en las teclas |
| **3. Master** | Delay dub, reverb, filtro DJ, drive, duck y limitador. Presupuesto de RAM documentado | Dub throws y freeze; sidechain |
| **4. Armonía** | ACORDE DUB y MACRO. Escalas. ETAPAS y TURING. MUTAR y GENERAR con deshacer | Acordes dub con delay; patrones generativos |
| **5. Atmósfera** | DRONE (y FM4 o LÍNEA si los eliges). LFOs, deriva y rampa de tempo | Una sesión ambient → techno completa |
| **6. Proyectos y clock** | Presets y proyectos (en el navegador, guardados en local). Seguir clock MIDI externo por WebMIDI. Pulido visual. Manual | Guardar/cargar; seguir a Ableton o al Digitone por WebMIDI |
| **7. Hardware** *(solo cuando lo pidas)* | Compilar para el dispositivo. Backup y plan de recuperación. Primer flasheo. CPU real. Activar lo "pendiente": BT, USB multipista, clock out, LEDs, guardado en flash | — (en el FM-1) |

Regla en todas las fases: compilo y valido antes de entregar. Si algo no se ha podido compilar o probar, lo digo explícitamente.

---

## D. Preguntas

**Bloqueantes (antes de la Fase 0):**

1. **Base:** ¿proyecto nuevo sobre fm1-base + felucca-bsp (mi recomendación) o fork de Melodee?
2. **Lenguaje:** ¿aceptas C++ de subconjunto, con plan B en C99 si el toolchain no lo soporta bien?
3. **Dónde compilar:**
   - ¿Tienes cuenta de GitHub? Así compilaría con GitHub Actions y publicaría el emulador en una web: tú solo abrirías un enlace.
   - ¿Tu Mac es Apple Silicon o Intel?
   - ¿Me dejas conectar una carpeta de tu Mac desde la app de escritorio para instalar las herramientas yo?
4. **¿Qué firmware lleva ahora tu FM-1** (oficial V15, Melodee, otro)? ¿Tienes algún backup?

**De diseño:**

5. ¿Qué es "chords disastrous peace"? ¿Y "SM01"? No quiero suponerlo.
6. ¿4 pistas o 6?
7. ¿Qué motores del catálogo quieres para empezar (4–6)?
8. **Clock:** ¿quién manda en tu set: el Digitone II, el OXI One o Ableton? Por TRS el FM-1 solo recibe, y el Digitone tiene MIDI DIN, así que haría falta un adaptador DIN→TRS (hay que comprobar si es tipo A o B).
9. Estética: ¿A, B o C, o tus propias referencias?
10. ¿Quieres velocidad y presión en las teclas? Es experimental en la base.
11. ¿Es prioritario el USB audio multipista (stems para el videopodcast)?
12. **Nombre:** ideas en la línea de tus conceptos (abismo, nebulosas, horizonte): **ABISMO**, **UMBRA**, **NEBULA**, **DERIVA**, **SEDIMENTO**. Hay que comprobar que no choquen con productos existentes.

---

## E. Créditos previstos (borrador)

- **Felucca:** Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0.
- **felucca-bsp:** Leo Kuroshita y otros, MPL-2.0.
- **fm1-base:** Charles Vestal, MPL-2.0.
- **Pila Bluetooth propia:** Optimist (Henri David).
- **Melodee:** Kerem Kilic (Ellic Studio). Se le acredita como referencia y por las piezas que tomemos.
- **Emulador web:** David Konsumer (PR #53 de Melodee).
- **Mutable Instruments / Emilie Gillet:** por cada módulo portado, MIT.
- **Fuente:** la tipografía elegida (OFL).
- M-VAVE y FM-1 son marcas de su dueño. Este firmware no está hecho, respaldado ni soportado por M-VAVE.
