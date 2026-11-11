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

## Mapa del panel (cambia por las 6 pistas)

| Control | Función |
|---|---|
| FX…GLO | Seleccionar pista 1–6. HOME + pista = mute. En la vista MEZCLA: pista = mute, HOME + pista = solo |
| HOME | Toque: ÓRBITA → MEZCLA → MASTER. Mantener: SHIFT |
| HOME + SAVE | GLOBAL (tempo, clock, MIDI, sistema) |
| SAVE | Proyectos |
| ARP | GENERAR patrón nuevo. HOME + ARP = deshacer |
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
