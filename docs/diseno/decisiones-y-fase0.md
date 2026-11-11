# Decisiones cerradas y estado de la Fase 0 — 11 nov 2026

## Decisiones

| Tema | Decisión |
|---|---|
| Base | Proyecto nuevo sobre **fm1-base + felucca-bsp** (MPL-2.0); nuestra app en **GPL-3.0** |
| Lenguaje DSP | C++14 de subconjunto (sin STL, excepciones, RTTI, new/delete, constructores globales, double, int64 en bucles calientes). Si falla la prueba con el toolchain de JieLi, plan B en C99 |
| Compilación | Emulador: en el entorno de Claude (clang 18 + WASI, ver abajo). Firmware: GitHub Actions (toolchain de JieLi) |
| Carpeta local | `~/Documents/FM1` en el Mac de Javier (backup original y tablas de acordes ya copiados) |
| Pistas | **6** (botones FX, SCL, ENV, LFO, EDIT y GLO), con mezclador completo |
| Motores (6) | GRAVE (bombo + rumble), METAL (hats/ride 808), ACORDE DUB, MACRO (modelos tipo Plaits), DRONE (resonadores), **BÁLTICO** (voz de 2 VCO inspirada en el Cwejman SM-1). Para más adelante: GOLPE, FM4, LÍNEA, NIEBLA |
| Acordes | Tablas de Plaits Palette (formato `rubato-plaits-chord-table`, 4 voces en cents + arpLength): Original (Mutable Instruments), Colour Chords (Jon Butler), Disastrous Peace (Joe McMullen), Tonnetz Walk (Rubato Audio). Todas MIT, con crédito |
| MIDI | Entrada: TRS + Bluetooth. Salida: Bluetooth (y USB). *Pendiente en el dispositivo* |
| Velocidad/presión en teclas | Sí (experimental en la base). *Pendiente en el dispositivo* |
| USB audio multipista | Sí: 6 pistas + mezcla (8 canales). *Pendiente en el dispositivo* |
| Estética | Punto de partida "Nebulosa": fondo casi negro azulado, un color por pista, vista orbital, popup grande al girar un knob con explicación |
| Nombre | **EPICYKLOS** (provisional; epi + Cyklos = "epiciclo"). Repo: `JavierIng/fm1-epicyklos` |

## Mezclador y cadena master (revisado 11 nov, 12:30)

- **Por pista:** nivel, pan, mute, solo, envío A (delay), envío B (reverb), duck desde la pista del bombo,
  corte de graves y drive. **Sin compresor por pista.**
- **Un solo compresor**, en la salida (compresor de bus), y **una sola reverb** (PLACA, tipo Plateau).
- **Master:** retornos A/B → filtro DJ → EQ de 3 bandas → compresor de bus → saturación de cinta → limitador.
- **Delay** con tipos: **TAPE**, **BBD** y **PINGPONG** (ver `ideas-referencias.md`, "Delay").
  Un único buffer de 16 bit a 22 kHz (~1,5 s, 66 KB) que cada tipo usa a su manera.
- **Coste estimado** (sin verificar en el chip): al quitar los 6 compresores, ~3.000–3.500 ciclos por muestra
  de ~4.900. Los modelos pesados de MACRO y MAREA de 16 bandas siguen pudiendo pasarse.

## Patrones, aleatorio, mute y solo (decidido 11 nov, 15:54)

- **Patrón** = la secuencia y el sonido de cada pista. La **mezcla, los efectos y el tempo van con el proyecto**:
  al cambiar de patrón no saltan los niveles ni la cola del delay.
- **16 patrones por proyecto** (~2 KB cada uno; 4–6 proyectos en flash, estimado). SAVE abre los patrones:
  las 16 teclas blancas son los huecos (luz = ocupado); el cambio queda **en cola hasta el final del compás de
  16 pasos** (opción: inmediato). SAVE otra vez guarda. Nombre automático editable (SELECT posición, ALGORITHM letra).
- **ARP = aleatorio de la página actual**, con límites musicales (SONIDO en escala y rangos útiles; RITMO golpes y
  rotación, no la longitud; MEZCLA solo pan y envíos, nunca el nivel). HOME+ARP deshace. ARP + ALGORITHM = cuánto.
  ARP + girar un knob = bloquear ese parámetro.
- **Mute y solo por pista** (ya en la Fase 1): en todas las vistas HOME+pista = mute; en MEZCLA pista = mute y
  HOME+pista = solo. **Mute y solo son del proyecto, no del patrón:** al cambiar de patrón se mantienen (si
  silencias el bombo, sigue silenciado). El mute se guarda con el proyecto; el solo no (al cargar, nada en solo).
- Fases: patrones en memoria, cambio en cola y aleatorio en la **Fase 2**; guardado en flash y nombres en la **Fase 6**.

## Mapa del panel (cambia por las 6 pistas)

| Control | Función |
|---|---|
| FX…GLO | Seleccionar pista 1–6. HOME + pista = mute. En la vista MEZCLA: pista = mute, HOME + pista = solo |
| HOME | Toque: ÓRBITA → MEZCLA → MASTER. Mantener: SHIFT |
| HOME + SAVE | GLOBAL (tempo, clock, MIDI, sistema) |
| SAVE | Patrones (teclas blancas = 16 huecos); SAVE otra vez = guardar |
| ARP | ALEATORIO de la página actual. HOME + ARP = deshacer. ARP + ALGORITHM = cantidad; ARP + knob = bloquear |
| SEQ | Modo PASOS (16 teclas blancas = pasos, con sus LEDs) |
| PLAY | Arranca/para. HOME + PLAY = FILL |
| REC | Mantener: DUB THROW. Mantener REC + pista = throw de esa pista. HOME + REC = FREEZE del delay |
| SELECT | Página. HOME + SELECT = tempo |
| ALGORITHM | MUTAR |
| PRESETS | Nivel de la pista. HOME + PRESETS = envío A |
| KNOB 1–4 | Parámetros de la página. HOME + KNOB = ajuste fino |
| OCT− / OCT+ | Octava del teclado. En modo PASOS: grupo de 16 pasos |

## MIDI: lo que se puede y lo que no

- Recibir por TRS (Digitone II, OXI One) y por Bluetooth a la vez: **sí**.
- **Clock:** solo una fuente a la vez (INT, TRS, BT o USB). Si llegan dos, gana la elegida en GLOBAL.
- **Enviar por Bluetooth: sí, pero solo a un Mac, iPhone o iPad.** El Digitone II no tiene Bluetooth. La pila propia mantiene un único enlace a la vez.
- El clock por Bluetooth llega a ráfagas (decenas de ms). Seguirlo es viable con la marca de tiempo de BLE-MIDI y un reloj suavizado. Como maestro para otros equipos es peor que USB.

## Hardware de la unidad de Javier (del backup del 10 nov 2026)

- **Firmware:** oficial "FM-1 801".
- **Bootloader:** **m-bootloader 2.3**, de Julius Vitkauskas.
  - Menú de recuperación: PLAY al encender. Se abre solo si el firmware no arranca.
  - Modo descarga: REC al encender.
  - Escribe con M-Flasher, con CRC.
  - Una actualización oficial de M-VAVE lo sustituye.
- **Flash:** 0x856014 = Puya P25Q80, 1 MiB. Es la pieza que espera felucca-bsp.
- **Backup:** 1.048.576 B; el sha256 `b8ccc233…b5ea` coincide con el del JSON. Hay copia en `~/Documents/FM1/backup-original/`.

## Estado de la Fase 0

- **Hecho y verificado:** la plantilla de fm1-base (commit `b7b25a4`, BSP `e36cb7c`) compila a WebAssembly en el entorno de Claude **sin Emscripten**.
  - Cómo: clang 18 + wasi-sysroot 25 + un sustituto de `emcc`.
  - Prueba headless en Node: arranca, responde a teclas, dibuja la pantalla y suena (pico 0,09).
  - Publicada como página de prueba.
- **Repo:** `JavierIng/fm1-epicyklos`. CI en GitHub Actions en verde (11 nov): pruebas, emulador, prueba de C++ y firmware.
- **Prueba de C++ con el compilador del FM-1 (clang 4.0.1 de JieLi, pi32v2): OK.** Compila el subconjunto
  (clases con init(), plantillas, constexpr, float), sin runtime de C++ ni constructores globales. Falta enlazarlo
  dentro del firmware (Fase 1).
- **Firmware de la plantilla compilado** (`epicyklos.fwsc`, 614.938 B, identidad FM-1_5700, Bluetooth con pila propia).
  Memoria medida por el build:

  | Recurso | Usado | Total | Libre |
  |---|---:|---:|---:|
  | Código en flash (imagen) | 147.092 B | ~573.000 B | ~426.000 B |
  | Código en RAM (.ram_text) | 5.576 B | 32.768 B | 27.192 B |
  | RAM .data + .bss | 40.216 B | 98.304 B | 58.088 B |
  | Pool de RAM | 69.384 B | 344.064 B | 274.680 B |

  → Quedan **~330 KB de RAM** para la app (antes estimábamos 150–250 KB). Comprobaciones del build: sin
  multiplicaciones de 64 bit junto a lecturas, sin double por software, sin nada de JieLi en Bluetooth.
- **Pendiente:**
  - comprobar que la página del emulador suena en el navegador de Javier;
  - enlazar código C++ dentro del firmware (Fase 1);
  - CPU real: solo en el dispositivo (Fase 7).
- **No disponible:** el shell del Mac (una VM Linux) no tiene salida a internet. Desde ahí no se instala nada, pero sirve para dejar y leer archivos en `~/Documents/FM1`.

## Estado de la Fase 1 (11 nov 2026)

- **Núcleo DSP en C++** (`app/dsp/`), compilado aparte a -O2 por el build de fm1-base (`o2_cflags`): reloj en
  coma fija Q32 (exacto: el primer intento en float derivaba ~0,4 muestras por tiempo; lo detectó la prueba),
  6 pistas con secuenciador euclidiano polimétrico (longitud, golpes, rotación, división), voz de prueba (seno con
  caída / sierra / ruido + filtro SVF), mute, solo, nivel, pan y soft clip.
- **Interfaz** (`app/app.c`, reescrito, GPL): vistas ÓRBITA y MEZCLA, páginas SONIDO / RITMO / MEZCLA de 4 knobs,
  popup con valor grande y explicación al girar, mapa del panel de las decisiones (lo de fases futuras avisa).
- **Verificado:** pruebas del núcleo en el ordenador (timing a la muestra, polimetría 16/12, mute/solo, sin NaN),
  23 comprobaciones del panel en el simulador, emulador sin navegador con sonido, y **firmware compilado en CI con el
  C++ enlazado**: sin multiplicaciones de 64 bit junto a lecturas, sin double por software; imagen 148.092 B.
- **No verificado:** nada en el FM-1 (no se ha instalado). CPU real desconocida.

## Estado de la Fase 2a: patrones (11 nov 2026, tarde)

- **16 patrones del primer proyecto** (`app/dsp/factory.cpp`, re menor): NEBULA, BRUMA, DERIVA, ORBITA, PULSAR,
  MAREA, PERIGEO, APOGEO, ECLIPSE, SILICE, FOSFORO, CUASAR, HORIZONTE, NADIR, ZENIT, CYKLOS. Del ambient sin bombo al
  pico y la salida; incluyen tresillos (PERIGEO), ciclos en cinco (CUASAR), un ciclo de acordes de 32 (HORIZONTE).
- **Cambio de patrón:** en cola hasta el final del compás de 16 semicorcheas, en la muestra exacta (prueba); parado,
  al momento. Mezcla, mute y solo se quedan (prueba). Las ediciones con los knobs se quedan en su patrón (en memoria;
  guardar en flash: Fase 6).
- **Vista PATRONES:** SAVE la abre; teclas blancas = 16 huecos (luz: el actual; parpadea el que está en cola);
  SAVE mantenido + tecla blanca cambia desde cualquier vista. Cabecera: `P04 >08`.
- **Verificado:** pruebas del núcleo, 34 comprobaciones en el simulador, emulador sin navegador, firmware en CI.
- **Limitación:** suenan con la voz de prueba hasta que lleguen los motores (Fases 2b–5).
