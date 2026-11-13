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

## Fase 2b (entregada en el emulador, 2026-11-11)

- **Motores:** GRAVE (bombo con rumble) en la pista 1 y METAL (platos 808/909) en la pista 2. Pistas 3-6 siguen con la voz de prueba.
- **Bancos y patrones:** teclas negras = bancos A-K, teclas blancas = 16 patrones del banco. Banco A = patrones de muestra; B-K = generados a partir de una semilla (176 patrones no caben en RAM). Lo editado en un banco se pierde al cambiar de banco hasta que exista el guardado en flash (fase 6).
- **Vista PASOS (SEQ):** teclas blancas = pasos; paso mantenido + K1 probabilidad, + K2 condición.
- **Aleatorio (ARP):** de la página a la vista; en la vista PATRONES, patrón entero. HOME+ARP deshace (un nivel). ARP+ALGORITHM cantidad, ARP+knob bloqueo.
- **Tempo:** HOME+SELECT. Al mantener HOME la pantalla muestra una chuleta de combinaciones. Tap tempo: pendiente.
- **Manual de usuario:** `docs/manual/es.md` y `docs/manual/en.md`, se amplían en cada fase solo con lo que ya funciona.
- **Pendiente conocido:** un patrón aleatorizado conserva el nombre anterior; no hay marca de "modificado".
- Validación: pruebas DSP, 58 comprobaciones en el simulador, emulador sin interfaz y CI en verde (commit 0508291). Nada instalado en el FM-1.

## Fase 2c (entregada en el emulador, 2026-11-11)

- **GOLPE** (palma / caja / aro) en la **pista 6**, que ya hacía de percusión en los 16 patrones. Se propuso la pista 3, pero habría quitado el acorde de los patrones de muestra.
- **Adelantar y atrasar:** página GROOVE con SWING y DESPLAZA (pista entera, ±45 % de paso); micro por paso en la vista PASOS (paso mantenido + K3, ±3/8 de paso). Probado a la muestra.
- **ATAQUE** en la voz de prueba (MOTOR, K4): 1 ms a ~1,4 s; la caída empieza al terminar el ataque. Los motores de percusión no lo usan.
- Páginas: SONIDO · MOTOR · RITMO · GROOVE · MEZCLA.
- Validación: pruebas DSP (13 bloques), 62 comprobaciones en el simulador, emulador sin interfaz. Nada instalado en el FM-1.

## Fase 2d (2026-11-11, noche)

- **GOLPE corregido** tras la prueba de Javier ("los knobs cambian poco"): rangos más anchos (BRILLO 350 Hz–8,5 kHz, CAÍDA hasta ~2 s), AFINA mueve también la banda de ruido, CUERPO actúa en los tres tipos. Hay una prueba que exige que cada knob cambie la energía o el brillo al menos un 25 % en palma, caja y aro.
- **Knobs más rápidos:** 40 clics de punta a punta en los parámetros continuos (antes 64); HOME sigue dando el ajuste fino.
- **TAMBOR** (toms, congas, percusión metálica; 2 operadores FM). Se elige en MOTOR; por defecto en la pista 6 de PULSAR, PERIGEO, FOSFORO y CUASAR, y en parte de los patrones generados.

## Fase 3a: envíos, eco y reverb (entregada en el emulador, 2026-11-11, noche)

- **Envíos por pista** A (eco) y B (reverb) en la página MEZCLA, después del fader; HOME+PRESETS = envío A.
- **Eco** (bus A) con tres tipos sobre un buffer de 16 bit a 22 kHz (66 KB, 1,5 s): TAPE (wow/flutter, saturación, el tiempo desliza el tono), BBD (más oscuro cuanto más largo, soplido de reloj) y PINGPONG (dos líneas de 0,75 s). Tiempo en semicorcheas (1–16). El feedback puede auto-oscilar; lo sujeta la saturación.
- **Reverb PLACA** (bus B): topología de placa de Dattorro (1997) escrita para este proyecto, a 22 kHz en 16 bit (33 KB). **No es un port de Plateau**, como se había apuntado al principio; se inspira en el mismo algoritmo publicado. Controles: caída, tono, movimiento, nivel.
- **Vista EFECTOS** en el ciclo de HOME (ÓRBITA → MEZCLA → EFECTOS) con páginas ECO, ECO 2 y REVERB.
- Los efectos son del proyecto. Los buffers grandes y el estado del motor van al pool de RAM del aparato (no a .bss).
- **Aún no hecho de la lista del eco:** cabezales múltiples, resonador en el feedback, parada de cinta, dropouts, throws grabados, eco a la octava. Tampoco el flanger.
- Validación: el eco vuelve a su tiempo en los tres tipos, PINGPONG alterna lados, la placa deja cola estéreo, y con feedback y caída al máximo durante 20 s no hay valores inválidos ni saturación dura. 70 comprobaciones en el simulador.

## Gestos de dub (2026-11-12)

- **THROW:** mantener REC lanza la pista elegida al eco a tope (con fundido de ~8 ms, sin clic); REC mantenido + botón de pista lanza también esa. Al soltar REC vuelven todos.
- **FREEZE:** HOME+REC activa/quita. El eco deja de escuchar y reescribe lo que lee sin filtro ni pérdida; el bucle se oscurece despacio por la interpolación.
- **DUCK:** knob en ECO 2 (K4). Seguidor del bombo (pista 1) con ~150 ms de vuelta; baja los retornos de eco y reverb, por defecto 35 %.
- Validación: el throw manda una pista con envío 0 al eco; con freeze el bucle sigue igual 4 s después y un golpe nuevo no entra; al quitarlo se apaga; el duck al 100 % deja los retornos en un tercio. 79 comprobaciones en el simulador.
- **6 u 8 pistas (decidido 12 nov):** seguimos con 6. Pasar a 8 (como Groove OS, Hortator o el firmware Open de M-VAVE) queda en espera para una actualización futura; el obstáculo no es el núcleo sino el acceso a las pistas 7 y 8 desde el panel y la CPU con motores pesados.

## Fase 3b: master y modulación (2026-11-12)

- **Cadena del master** (tras los retornos): filtro DJ (izquierda paso-bajo, derecha paso-alto, centro transparente; dos SVF en serie siempre activos para que no haya clics al cruzar el centro) con RESO y DOBLE (segundo pico paralelo, idea de filtro doble tipo Blades, sin su código, que es analógico) → compresor de bus (un knob: umbral, ratio y ganancia; ataque 10 ms, vuelta 150 ms) → saturación de cinta → limitador suave. HOME+ALGORITHM = filtro desde cualquier vista. Sin EQ de 3 bandas todavía.
- **Modulación** (vista MOD en el ciclo de HOME): 2 LFO enganchados al compás (6 formas, 1/16–32 compases) y 2 seguidores de pista, cada uno con 2 destinos (HOME+K3/K4 = el segundo) y cantidad ± . Destinos: filtro, reso, eco (tiempo, repite, tono, nivel), reverb (caída, nivel), tono y nivel de cada pista. Se calcula por bloque (≤ 256 muestras); el nivel modulado se suaviza por muestra.
- **Pendiente de la 3b:** escenas, REDUCCIÓN (bits/frecuencia) y STUTTER, EQ del master. LFO por pista y DERIVA van con los motores.
- Validación: el filtro oscurece y adelgaza y en el centro no cambia la energía (<5 %) aunque RESO y DOBLE estén arriba; el compresor estrecha la dinámica (3,4 → 2,2 entre partes fuertes y suaves); el LFO de 1 compás va de 0,1 a 0,9 y está en su máximo a 1/4 de compás; el seguidor sube con el bombo y baja entre golpes. 90 comprobaciones en el simulador.

## Tipos de filtro y MAREA (2026-11-12)

- A petición de Javier, el filtro del master tiene **TIPO**: DJ, LP, HP, BP, NOTCH, MORPH (LP → BP → HP) y **MAREA**.
- **MAREA** (inspirado en el Slink de Hypnus Records, solo la idea) entra como un tipo del filtro del master, no como inserto aparte: 8 bandas de 100 Hz a 8 kHz alternando lados, con una ola de volumen que las recorre. POSICIÓN, OLA (velocidad), RESO (ancho de banda). Es la versión de 8 bandas; la de 16 sigue sin hacer (coste).
- Al cambiar de tipo el filtro empieza vacío: puede oírse un pequeño corte.
- Validación: LP oscurece y HP adelgaza; BP deja una banda y NOTCH la quita; MORPH va de oscuro a fino; con OLA la "color" de MAREA cambia en el tiempo y quieta no; ningún tipo da valores inválidos con todo al máximo. 92 comprobaciones en el simulador.

## MAREA como efecto de pista (2026-11-12)

- Javier (tras describir Slink: "al hacer el morphing de las ondas parece como unas olas de mar"; "más bien es para las pistas de armonías y sound design"): MAREA sale del filtro del master y pasa a ser un **inserto asignable a una pista** (PISTA: ninguna o 1-6). **Una sola MAREA a la vez** por coste de CPU.
- Referencia pública de Slink: banco de 32 paso-banda cuyos niveles mueve un motor de 16 senos desfasados. Nuestra versión, escrita de cero: **16 bandas** (6,5 octavas, RANGO las desplaza), niveles por **4 senos sumados** con longitudes y velocidades distintas (dos en sentido contrario), FORMA mezcla de 1 a 4 (oleaje suave a picado), PROFUND, LONGITUD, ESTÉREO (desfase espacial a la derecha), RESO, MEZCLA. Sale en estéreo; el PAN de la pista hace de balance; los envíos reciben la señal ya procesada.
- 3 páginas en EFECTOS (MAREA, MAREA 2, MAREA 3) con dibujo de las 16 bandas; las pestañas pasan a mostrar anterior/actual/siguiente y n/8.
- 5 destinos de modulación nuevos: MAR VELOC, FORMA, ESTÉREO, RANGO, MEZCLA.
- El filtro del master queda con DJ, LP, HP, BP, NOTCH y MORPH.
- Validación: con MEZCLA 0 la pista suena igual que sin MAREA; en otra pista no toca la 3; moviéndose el color cambia x2,6 en el tiempo y quieta no (x1,06); ESTÉREO separa los lados; mojado queda en ~70 % de la energía seca; nada inválido en los extremos. 97 comprobaciones en el simulador.
- **Abierto:** 32 bandas o varias MAREA a la vez cuando se conozca la CPU real; freeze propio de la ola (hoy VELOC 0 la detiene).

## MAREA rehecha según el manual de Slink (2026-11-12)

- Javier pasó la captura de la interfaz de Slink Filter; los controles se han rehecho según su manual de usuario público: RATE (con SYNC, freeze y reverse) → **VELOC** bipolar (centro quieta, derecha avanza, izquierda vuelve; HOME+VELOC libre/tempo, 32 compases a 1/4); **MULTI** (ciclos), **RIZO** (onda senoidal añadida), **DESFASE** (fase a mano), **INVIERTE** (profundidad bipolar), **GRAVEDAD** (atracción hacia abajo), **MEZCLA**; área **DESDE/HASTA** (fuera, las bandas quedan a su nivel; pendiente fija de 1 octava, ganancia fija 0 dB); bandas en **puntos fijos** (16, 40 Hz–15 kHz) repartidas a izquierda/derecha con **ANCHO**. RESO se mantiene.
- Supuestos propios (el manual no da la fórmula): la ola es un seno con un segundo seno de rizo a 3,3× y −1,7× de velocidad; GRAVEDAD eleva la altura a una potencia (1–9); la pendiente y la ganancia del área no se pueden ajustar. Los LFO/SEQ/envolvente propios de Slink no se copian: se usa la vista MOD.
- Pantalla: la curva de la ola sobre las frecuencias, bordes del área punteados y velocidad arriba.
- Validación: MEZCLA 0 = seco; en otra pista no toca la 3; quieta no cambia el color (x1,04), en marcha sí (x2,1); ANCHO 0 es mono exacto; VELOC centro = 0, por debajo negativa, con TEMPO arriba = 1/4 de compás por ciclo; INVIERTE al centro deja todo plano; una banda fuera del área no se mueve y una dentro sí; mojado ~88 % de la energía seca. 99 comprobaciones en el simulador.

## MAREA más audible (2026-11-12)

- Javier: "las funciones parecen correctas pero no se nota el cambio". Medido: la causa principal era la **velocidad** (la de inicio daba ~1 ciclo cada 27 s y el knob dedicaba casi todo su recorrido a velocidades lentísimas). Nueva curva: 0,03–3,8 Hz, con ~0,3 Hz en el centro de cada mitad.
- Bandas finas mueven más el color pero bajaban el nivel (RESO 0,9 dejaba un 18 % de la energía): ahora una compensación según el ancho de banda mantiene el nivel (99–116 % del seco en todo el recorrido).
- Inicio pensado para oírse: MEZCLA 100, VELOC ~3 s por ciclo, RESO 0,60, ANCHO 100, GRAVEDAD 0,35.
- Resultado medido sobre una sierra sostenida: el cambio de color pasa de x1,22 a x3,08 con los valores de inicio.
- MAREA ya iba antes del eco y la reverb (inserto de pista; los envíos reciben la señal procesada).
- Pendiente de decidir: vista FLOW (cadena de efectos con activar/desactivar y, quizá, orden) y adelantar los motores de síntesis (ACORDE DUB, DRONE) para juzgar MAREA con pads reales.

## Fase 4a: ACORDE DUB y DRONE (2026-11-12)

- Orden acordado con Javier: motores primero (para juzgar MAREA, eco y reverb con sonidos reales), luego la vista FLOW, luego las escenas. Los patrones de muestra por estilo (A ambient/jungle, B deep/dub, C hipnótico raw) quedan anotados para después.
- **ACORDE DUB** (pista 3): 4 voces de las tablas de Plaits Palette (Original, Colour Chords, Disastrous Peace, Tonnetz Walk; MIT, ya en CREDITS), cada una 2 sierras polyBLEP a −6/+7 cents, paso bajo resonante con envolvente propia (más corta que la de amplitud). La voz más grave se baja a la octava de la nota. `tools/chords_to_h.py` genera `app/dsp/chords.h` desde `assets/chords` con nombres cortos en ASCII.
- **DRONE** (pista 5): sostiene (0,2–30 s tras el último golpe, luego ~2 s de caída), legato con portamento de 0,25 s, 3 osciladores (raíz, raíz separada, intervalo), timbre seno → triángulo → sierra → aire, DERIVA (paseo aleatorio lento de tono, separación y filtro). Al parar el secuenciador se apaga (`Voice::release`).
- Pista 4 (bajo) sigue con la voz de prueba hasta que haya motor de bajo (LÍNEA o BÁLTICO).
- Coste en el ordenador (no en el FM-1): ACORDE ~0,9× y DRONE ~1,4× la voz de prueba. Que ACORDE (8 sierras) salga más barato que la de prueba es sospechoso; la medida del ordenador no vale para decidir nada del aparato.
- Validación: los dos suenan; cambiar de acorde cambia el contenido (1192 → 1302 cruces por segundo de octava a MIN9); los 70 acordes de las 4 tablas, limpios; el dron sigue a 2 s y se apaga tras parar. 103 comprobaciones en el simulador.

## Fase 4b: guardar patrones y EVOLUCIÓN (2026-11-12)

- Javier: "los patrones son muy estáticos y repetitivos" y "tendríamos que poder guardar el patrón generado en un banco o ranura". Orden acordado: guardar → EVOLUCIÓN (mutar + ausencias) → LFO de pista y DERIVA → FLOW.
- **Guardar** usa el almacén de objetos de fm1-base (`plat_store_*`): en el aparato, la flash (A/B, CRC, a prueba de cortes); en el emulador, el navegador (localStorage). No es un apaño temporal: es el mismo camino que usará la fase 6.
  - **Mapa de flash (decisión que no debe cambiar entre versiones):** objetos `OBJ_BANK0`..+21, dos por banco (ranuras 1-8 y 9-16), 8 KiB cada uno desde 0x97000 hasta 0xC2FFF, por debajo del panel (0xDC000) y del Bluetooth (0xDE000). Cada objeto: cabecera "EPKB", versión, banco, mitad y máscara de ranuras, luego 8 registros de 438 bytes (3512 de 3840). Un objeto de otra versión se ignora: un cambio de formato futuro necesita convertir, no romper.
  - Se guarda **lo que suena** (con la evolución dentro). Gesto: vista PATRONES, HOME + tecla blanca, dos veces en 3 s. Marca `*` en el nombre.
  - Antes de cargar otro banco, la interfaz lee sus objetos y se los pasa al núcleo (stage); el núcleo los aplica al cargarlo. Carrera conocida y aceptada: si se elige un banco justo cuando otro en cola entra en el compás, ese otro puede sonar un compás sin lo guardado.
  - Falta: borrar una ranura guardada (volver a fábrica), guardar el proyecto (mezcla, efectos, tempo, MOD), nombres. Fase 6.
- **EVOLUCIÓN** (página nueva por pista: MUTAR, CADA, VUELTA, AUSENCIA): un desvío sobre el patrón, no una edición. Cada CADA compases: vuelve hacia lo escrito según VUELTA, cambia según MUTAR (golpes ±, rotación, tono, caída, carácter; nota en motores melódicos: 4ª, 5ª, 8ª), y descansa según AUSENCIA. MUTAR 0 + VUELTA 0 = congelado. Cambiar de patrón o PLAY vuelve a lo escrito. Coste: unas operaciones por compás, nada por muestra.
  - Banco A con evolución de fábrica (bombo quieto); los generados, al azar.
  - Supuesto propio: los golpes nunca bajan a 0 por mutación (eso lo hace AUSENCIA); en pistas hechas a mano no se mutan los golpes, solo la rotación.
- Validación: MUTAR 0 deja el patrón idéntico 16 compases; MUTAR 1 cambia el ritmo 28 veces en 32 compases sin vaciar la pista; VUELTA 1 vuelve al patrón; congelado se mantiene; AUSENCIA 1 no dispara; CYKLOS difiere del primer compás en 22 pista-compases de 32 con el bombo fijo; un patrón guardado en D13 se carga idéntico (tono, nota, carácter, pasos), con `*`, y se rechaza otra versión. El gesto de guardar (incluido que una sola pulsación no guarda) probado en el simulador: 109 comprobaciones.
- **Sin probar:** no se ha escuchado; la persistencia del emulador tras recargar no se ha comprobado en un navegador real (el mecanismo es el de fm1-base); en el aparato, nada.

## EVOLUCIÓN: qué, por pista y congelar (2026-11-12)

- Javier: "no quiero que varíe el groove pero sí los sintes" y pidió decidir por pista qué se deja afectar. La evolución ya era por pista; faltaba control rápido y elegir qué varía.
- **QUÉ** (por pista, se guarda con el patrón): TODO, RITMO (golpes, rotación), SONIDO (tono, caída, carácter/acorde), NOTA. Lo que queda fuera vuelve a lo escrito. AUSENCIA pasa a la página GROOVE para dejar sitio (decide si la pista suena).
- **ARP mantenido + pista** = evolución sí/no por pista (del proyecto, como el mute; conserva los knobs). Se descartó HOME + pista porque ya es mute en todas las vistas: en directo se habría silenciado una pista sin querer.
- **HOME + PLAY** = congelar la evolución de todo (estaba reservado para FILL, que queda sin sitio por ahora).
- De fábrica solo evolucionan los sintes (pistas 3-5); antes también platos y percusión, y eso movía el groove sin pedirlo.
- Formato de guardado versión 2 (un parámetro más por pista). Lo guardado en versión 1 se convierte al leerlo (QUÉ = TODO), no se pierde.
- Validación: RITMO mueve el ritmo y no el tono; SONIDO al revés; apagada vuelve al patrón y se queda; congelado no cambia en 8 compases y al soltar vuelve a moverse; los 16 patrones del banco A sin evolución en la batería; un objeto de versión 1 se convierte con sus valores. Gestos ARP + pista y HOME + PLAY probados en el simulador.

## Aleatorio general más variado (2026-11-12)

- Javier: "parece que se van repitiendo cada ciertos toques". Medido: no había bucle (200 pulsaciones = 200 patrones distintos), pero la receta era estrecha: el bombo solo tenía 4 variantes (60 % a negras), todo en re menor, mismos motores por pista, misma mezcla y efectos.
- Ahora: 8 familias de bombo (sin, negras 38 %, negras + fantasma, roto euclidiano, medio tiempo, 2-step, puntillo, ciclo propio 12/14/15) → 22 bombos distintos en 200 y negras en 74/200. Tonalidad menor nueva en cada aleatorio general (las 12 aparecen), con **ARP + K1 en PATRONES = tonalidad fija**. Cada ranura recuerda su tonalidad (también al deshacer y al guardar en memoria); cada banco tiene una (A en re).
- Sigue igual a propósito: los papeles y motores por pista y la mezcla/efectos (del proyecto). El "estilo" del aleatorio por banco (A ambient/jungle, B deep/dub, C hipnótico raw) va con los bancos de muestra por estilo.
- Supuesto: siempre menor (eólico). Modos (dórico, frigio) quedan para el estilo por banco.

## LFO por pista y DERIVA: página MOVIMIENTO (2026-11-12)

- Una página nueva por pista (la 6 de 7): FORMA, VELOC, DESTINO, CANT. Formas: las 6 del LFO global + **DERIVA** (paseo al azar que rebota en los bordes; VELOC dice cuánto vaga por ciclo). La DERIVA se hizo como forma del LFO y no como página aparte: un knob menos y vale para cualquier destino.
- Destinos: TONO, CAÍDA, CARÁCTER, EXTRA 1 y 2 del motor (con su nombre), NIVEL, PAN, ENVÍO A, ENVÍO B, GOLPES, ROTAR. A +100 recorre ±media escala; GOLPES ±1/4 de la longitud, ROTAR ±1/2. Se calcula por bloque (≤ 256 muestras), enganchado al compás.
- Sin destino de afinación continua: las voces fijan la frecuencia al disparar; un vibrato pediría cambiar los motores (el DRONE ya tiene su DERIVA de afinación).
- Del patrón: se guarda con él. El LFO no se "hornea" al guardar (seguiría moviéndose encima). PAN y envíos pasan a leerse con modulación.
- Guardado versión 3: los parámetros de pasos fijos (motor, longitud, golpes, forma, destino…) ocupan 1 byte y el resto 2; con 4 parámetros más, en 2 bytes cada uno, medio banco ya no cabía (3992 de 3840 bytes); ahora 3464. Las versiones 1 y 2 se convierten al leerlas (LFO a 0).
- De fábrica (banco A): el acorde respira hacia el eco (seno, 4 compases, +30) y el bajo hace DERIVA de tono (+25).
- Validación: CANT 0 no mueve nada en 2 compases; seno de 1 compás a TONO +100 da 1,00 a 1/4 y 0,00 a 3/4, y −100 lo invierte; GOLPES y ROTAR dan ≥ 4 ritmos distintos en 2 compases; DERIVA se queda en ±1, recorre más de 0,3 y no repite entre ciclos; un objeto de versión 2 se convierte con sus valores y el LFO a 0.
- **Ableton Link (pregunta de Javier):** no va por Bluetooth (Link es IP). El chip del FM-1 tiene Wi-Fi y fm1-base lo soporta, pero el modo Wi-Fi excluye Bluetooth y audio USB y deja menos RAM; portar Link (GPL-2.0+, compatible) sería trabajo propio. Para Ableton: audio USB multicanal (hasta 10 canales en fm1-base) + MIDI USB para el clock, en la fase 6. Bluetooth audio (A2DP) solo va a auriculares/altavoces, con 150-200 ms: no sirve para un DAW.

## Vista FLOW (2026-11-12)

- Lo acordado tras "no se nota el cambio… un menú flow para cambiar el orden de los fx… activar o desactivar": la cadena dibujada, activar/quitar por bloque y algunas opciones de orden fijas, **sin orden libre**.
- Vista nueva en el ciclo de HOME: ÓRBITA → MEZCLA → EFECTOS → **FLOW** → MOD. Teclas blancas 1-6 = MAREA, ECO, REVERB, FILTRO, COMP, CINTA (luz = activo). K1 FILTRO EN (TODO / SECO / EFECTOS), K2 ESPACIO (ECO>REV / REV>ECO), K3 DRIVE (COMP>SAT / SAT>COMP), K4 CRUCE (el antiguo A REVERB, ahora en el sentido que diga ESPACIO).
- Quitar = fundido de ~10 ms y después no se calcula (en el FM-1, CPU libre: sin medir). Eco, reverb y filtro vuelven vacíos. El limitador no se quita.
- Supuesto: con el filtro en SECO, el seguidor y el duck siguen viendo las pistas sin filtrar (el bombo de la pista 1 se toma antes del filtro).
- Cambiar FILTRO EN mientras suena puede dar un pequeño salto (no hay fundido entre posiciones).
- Validación: con ECO y REVERB quitados el retorno es 0,000 (activos 0,656) y vuelve al activarlos; LP cerrado en EFECTOS deja pasar las pistas (x7,9 de energía frente a TODO); en SECO sin retornos ≈ TODO; REV>ECO alimenta el eco y ECO>REV no; COMP>SAT y SAT>COMP suenan distinto; MAREA quitada suena idéntica a sin MAREA (diferencia 0,000000). Todo lo anterior intacto con los valores de inicio.

## Envío PRE, DIGITAL y ESCENAS (2026-11-12)
- **Envío PRE por pista** (HOME + K3/K4 en MEZCLA): un solo ajuste para A y B; es de la mezcla, no del patrón. El MUTE sigue cortando el envío PRE (decisión: MUTE = silencio total; opción "PRE ignora MUTE" queda abierta).
- **DIGITAL** después de COMP/CINTA y antes del limitador: `Crush` (bits 16..4, sample hold /1..32) y `Stutter` (estéreo, media frecuencia, 16 bits, 16384 frames = 64 KB en `.pool`; máximo 0,74 s a 44,1 kHz; si no cabe se divide por dos). El firmware sigue cabiendo; CI en verde. CPU en el aparato: sin medir.
- **ESCENAS**: 8 escenas × 8 locks, del proyecto. Destino = índice de efecto o `EPK_SCENE_TRACK + pista*32 + parámetro` (solo parámetros continuos de pista desde la UI; los de pasos cambian a amount > 0,5). Se aplican al final de `apply_mod`, sobre `fx_live` y `pm`: no mueven los knobs y al salir todo vuelve. Fundido por escena en 8 pasos (0 a 4 compases, al tempo). Si dos escenas fijan lo mismo, gana la de número más alto.
- UI: HOME+SEQ modo escenas (las negras dejan de tocar notas); mantener = momentánea; HOME+negra = fija; knob con escena pulsada = grabar; SELECT = fundido; ARP = vaciar. En la lista de la escena los efectos llevan delante su página ("ECO 2 NIVEL", "REVERB NIVEL").
- 8 de fábrica: SUMERGIR, CATEDRAL, CINTA ROTA, DIGITAL, TUNEL, SECO, STUTTER, ECO INFINITO.
- **No se guardan al apagar** (fase 6, con el proyecto). FILL pierde su atajo HOME+PLAY (ahora congela la EVOLUCIÓN): pendiente buscarle otro.
- Pruebas: bloque 28 en `test_engine.cpp` (envío PRE 0,706 con nivel 0; correlación del stutter 1,000; fundido a la mitad 0,50) y escenas en `basic.txt`. Nada escuchado aún.

## NUBE: granular como tipo de reverb (2026-11-12)
- Javier: "prefiero el tema granular como Clouds, en el tipo de reverb; algo se ha perdido por el camino". Cierto: al principio la reverb iba a ser "tipo Clouds"; se cambió por PLACA (Dattorro) y el granular (NIEBLA) quedó "para el final". Se recupera como **segundo tipo del bus B**, no como pista ni como efecto aparte.
- **Seguimos con 6 pistas** (confirmado por Javier, 12 nov): las 8 pistas quedan descartadas por ahora.
- Memoria: NUBE y PLACA comparten un buffer de 16 bit (22.050 muestras, 1 s mono a 22 kHz, 44 KB; la placa usa 33 KB de él). Coste medido en CI: **+11 KB de pool** (de 306.644 a 317.752 B de 344.064; quedan **26 KB**). Imagen 240.452 B de ~581 KB.
- Motor propio con la idea de Clouds (MIT, crédito en CREDITS.md; no se ha portado código): 12 granos, captura mono repartida en estéreo, ventana de borde duro → triángulo → campana suave (TEXTURA), afinación por semitonos (±24), densidad con el centro en silencio (izq. regular, der. al azar), feedback (REPITE), freeze con HOME+REC (congela eco y nube a la vez). Sin modos espectrales.
- UI: página tras REVERB (TIPO, POSICION, TAMANO, DENSIDAD; ahora se llama REVERB 2, ver abajo); en NUBE la página REVERB pasa a REPITE, TEXTURA, AFINA, NIVEL. EFECTOS pasa a 10 páginas; DIGITAL muestra el panel del master (antes dibujaba MAREA por error).
- Coste de CPU: sin medir en el aparato; en el peor caso 12 granos con interpolación por muestra a 22 kHz. Si no llega, bajar a 8 granos.
- Pruebas: bloque 29 (suena; congelada sigue sonando sin entrada; densidad al centro = silencio; extremos sin valores inválidos ni picos > 1; vuelta a PLACA) y la página en `basic.txt`. No escuchado.
- Pendiente: destinos de modulación para NUBE (POSICIÓN, TAMAÑO), una escena de fábrica con NUBE.

## PLACA como Plateau (2026-11-12)
- Javier: "reverb tipo Plateau Valley, recuerda". Era lo decidido el 11 nov (ideas-referencias: PLACA = Plateau) y se quedó corto: la placa tenía la topología de Dattorro pero solo 4 controles (caída, tono, mover, nivel).
- Añadido, con los controles de Plateau y código propio (Plateau es GPL-3.0, crédito en CREDITS.md): **PREDELAY** (0–240 ms), **TAMAÑO** (las líneas del tanque escaladas 0,25–1; los difusores y las dos líneas moduladas mantienen su sitio), **DIFUSIÓN** (ganancia de los 4 difusores de entrada), **corte de graves** fijo ~90 Hz a la entrada del tanque, **FREEZE** con HOME+REC (entrada a 0 y caída 0,9995). No hay control de corte de graves ni de modulación por separado (forma/profundidad): no caben en 4 knobs sin otra página.
- **Sin memoria nueva:** el predelay (5.300 muestras) cabe en el buffer de 44 KB que ya tenía NUBE (21.998 de 22.050); un `static_assert` impide que crezca sin darse cuenta. Pool 317.844 B (+92). El tamaño máximo es el de Dattorro: Plateau llega a espacios más grandes; aquí no cabe.
- UI: la página 4 de EFECTOS pasa a llamarse **REVERB 2**: TIPO + (PREDELAY, TAMAÑO, DIFUSIÓN) en PLACA, o + (POSICIÓN, TAMAÑO, DENSIDAD) en NUBE. FREEZE se ve en el panel de la reverb también con la placa.
- Por defecto: predelay 24 ms, tamaño y difusión al 100 % (suena como antes salvo el corte de graves y el predelay).
- Pruebas (bloque 30): predelay arriba retrasa la cola 240 ms; congelada se mantiene (1,57 frente a 1,39 antes de congelar: no crece más de 1,5×) y libre se apaga; tamaño, difusión y predelay movidos sin parar 10 s: sin valores inválidos ni picos > 1. No escuchado.

## Fuera el STUTTER: reverb más grande y shimmer (2026-11-12)
- Javier: "¿si quitamos stutter?" → "adelante". Motivo: la RAM (26 KB libres) no daba para reverb más grande y además MACRO (Plaits), que estaba en el plan desde el 11 nov y **no se ha empezado** (pendiente, junto a BÁLTICO).
- **STUTTER eliminado** (64 KB). Sus dos parámetros se reutilizan para el shimmer (`EPK_FX_REV_SHIMMER`, `EPK_FX_REV_SHIM_INT`) para no mover los índices de los demás. Sustituto aproximado: eco corto + throw + freeze. La escena de fábrica 7 pasa a SHIMMER.
- **Bus B a 88 KB** (44.100 muestras): PLACA con TAMAÑO de 0,25× a **2×** Dattorro (las líneas del tanque tienen sitio para el doble; las moduladas también) y PREDELAY hasta **500 ms**; NUBE con **2 s** y granos de hasta ~1 s.
- **Shimmer** (página REVERB 3, solo PLACA): desplazador de tono de dos tomas cruzadas sobre 2.048 muestras (4 KB) que coge la salida del tanque, la sube (+12, +7, +19, +24 o −12) y la mete a la entrada, limitada con tanh y con un paso bajo suave; se para con la reverb congelada para que no crezca sin fin.
- **GRAVES** (corte de graves del bus B, 20–400 Hz, antes fijo en 90) y **ANCHO** (M/S en la salida de la reverb), para los dos tipos.
- Memoria medida en CI: pool **300.488 B de 344.064 → 42,5 KB libres** (antes 26). Imagen 242.712 B.
- Pruebas: predelay arriba 500 ms; shimmer sube el brillo de la cola (161 → 221 Hz de cruces); shimmer al máximo con caída al máximo 12 s sin valores inválidos ni picos > 1; ANCHO 0 = retornos mono; congelada y libre como antes. No escuchado.
- Orden acordado: preparar la prueba en el aparato (medidor de CPU, procedimiento de instalación y vuelta al 801) antes de MACRO; MACRO irá en la pista 4 con unos pocos modelos de Plaits (MIT), medidos en RAM y CPU.

## Reverb propia de la NUBE (2026-11-12)
- Javier: "estaría bien que la nube tenga también reverb". Clouds tiene una reverb detrás de los granos; la NUBE sustituía a la placa y sonaba seca.
- Segunda placa pequeña (la misma clase `Plate`, ahora con su buffer como parámetro) al 0,4 del tamaño de Dattorro, sin predelay ni shimmer: 7.804 muestras (15,6 KB, buffer propio `g_nverb_buf`). Ajustes por bloque: caída propia, tono 0,55, movimiento 0,35, difusión 1. Se suma a la salida de la nube (cantidad al cuadrado × 1,6). En REVERB 3, con TIPO = NUBE, K1 REVERB (inicio 0,45) y K2 CAIDA (0,65).
- Memoria: pool 316.560 B de 344.064 → **27,5 KB libres** (antes 42,5). Es memoria que ya no queda para MACRO: si Plaits no cabe, esta reverb se puede reducir o quitar.
- Pruebas: con densidad al centro, la cola de la nube sin reverb es 0 y con reverb 0,25 a los 1,2 s. Se corrigió la prueba del predelay (esperaba 0,5 s tras parar, lo mismo que el predelay máximo, y medía la cola de un golpe anterior).
- Versión de prueba 0.1.1 compilada (Actions → Site, privada).

## Orden de la reverb y textos de valor (2026-11-12, noche)
- Javier: "REVERB 2 tendría que estar en la posición de REVERB: primero el tipo y luego los controles" y "quita que salga TIPO 1 / TIPO 100". Ahora la página 3 de EFECTOS (**REVERB**) es TIPO + (PREDELAY, TAMAÑO, DIFUSIÓN) o + (POSICIÓN, TAMAÑO, DENSIDAD); la 4 (**REVERB 2**) es CAÍDA, TONO, MOVER, NIVEL (en NUBE: REPITE, TEXTURA, AFINA, NIVEL).
- **Fallo mío:** al quitar el STUTTER se borraron sin querer los textos de TIPO (PLACA/NUBE), PREDELAY (ms), AFINA (semitonos) y DENSIDAD (NADA/REG/AZAR): salían como 0-100. Restaurados. Las pruebas no lo vieron porque comprueban valores, no el texto; se comprueba ahora con capturas de pantalla.
- Firmware oficial de M-VAVE recibido de Javier: `FM-1.fwsc` V16 (718.196 B), sha256 `d017fb65…61d5`, igual al que fm1-base registra como oficial (FM-1_016, 2026-10-23). Sirve para "volver al firmware de M-VAVE" desde el instalador.

## Pantalla de inicio: galaxia (2026-11-13)
- Javier pasó una imagen de una galaxia espiral: "algo así moviéndose como una órbita con el nombre en medio parpadeando".
- Hecha con código, sin imagen (una foto de 240×240 serían 115 KB y no se podría girar con soltura): 1.100 estrellas en dos brazos de espiral logarítmica, el 18 % sueltas entre brazos, núcleo blanco con halo azul, nudos rosas en los brazos; rotación diferencial (el centro gira más rápido); EPICYKLOS latiendo una vez por segundo con borde oscuro para leerse sobre el núcleo. Las estrellas salen de la misma semilla cada fotograma: no ocupan memoria.
- 3,2 s o hasta tocar cualquier botón, tecla o knob (el toque además hace lo suyo). ~30 fps, en dos franjas de 240×120. Coste en el aparato: sin medir; solo al encender, con el secuenciador parado.

## Mezcla, MACRO y bancos B y C (2026-11-13)
- Javier, escuchando el emulador por el altavoz del móvil: "el kick está muy presente, falta presencia en los chords, el drone bass también está muy arriba; estaría bien agregar los Plaits y reorganizar los patrones que faltan de los bancos A, B, C".
- **Medido antes de tocar** (ORBITA, cada pista en solo, RMS): bombo −18 dB, dron −20, bajo −36, acorde −38, platos −50. El dron estaba 18 dB por encima del acorde: no era cosa del altavoz.
- **Mezcla nueva** (misma medida, ORBITA): bombo −19, dron −26, bajo −27, acorde −27, platos −46. El acorde sube ~10 dB, el bajo ~9, el dron baja ~5; el bombo solo baja 1 dB (el compresor del master recupera parte de lo que se le quita al fader). Cambios: salida del dron ×0,57, salida del acorde ×2,6 y su filtro algo más abierto de fábrica; niveles de fábrica bombo 74, platos 62, acorde 72, bajo 70, dron 45. Hay una prueba que lo vigila.
- Aviso: un altavoz de móvil no reproduce por debajo de ~250 Hz. Lo que se oía "arriba" del bombo y del dron eran sus medios; los graves reales no se han juzgado con nada. Pendiente escucharlo con monitores o auriculares antes de dar la mezcla por buena.
- **MACRO** (motor nuevo, pista 4): seis modelos (ANALOG, PLIEGUE, FM, GRANO, CUERDA, ARMONICO) con TIMBRE y ARMON, y TONO como filtro que la nota abre y cierra. Idea de Plaits, código propio: **no es un port de Plaits**. Portar el Plaits real (tablas de ondas, voz, 16 modelos) sigue sin hacerse.
- MACRO usa la memoria que ya tenía cada voz (cabe donde GRAVE): no gasta RAM nueva. Coste medido en el ordenador: ×1,16 de la voz de prueba, menos que DRONE o TAMBOR. En el FM-1: sin medir.
- Los bancos guardados pasan a la versión 4 (ocho motores); los de la versión 3 se convierten al cargar y conservan sus motores (probado).
- **Banco B = techno hipnótico, banco C = ambient/drone**, con nombres y el mismo recorrido que el A. Generados con reglas por estilo, no compuestos a mano. Interpreté así "los patrones que faltan"; si Javier se refería a otra cosa, se rehace.
- Nadie ha escuchado todavía MACRO ni los bancos B y C: solo están comprobados con medidas (suenan, no saturan, cada mando cambia el sonido).

## Líneas ácidas y aleatorio por partes (2026-11-13, mañana)
- Javier: "sonidos psicodélicos ácidos que entren y salgan con cortes de filtros, podría salir de MACRO; alternar en unas con drones y en otras sin drones; que se intercambien los motores de la parte de sonido, no del groove; un random para las tres pistas de groove y otro para las de síntesis".
- **MACRO ACIDO** (séptimo modelo): sierra → cuadrada, filtro resonante de 18 dB/oct (SVF + un polo), envolvente de filtro más rápida que la de volumen, acento en 3 de cada 10 notas, **SALTOS** (el mando de ATAQUE en este modelo): saltos de octava y ligados en un bucle fijo de 16 golpes. Código propio; no emula el 303 (no hay filtro de diodos ni su envolvente exacta).
- Límite real: el secuenciador no tiene nota por paso. La línea ácida es **una nota con saltos de octava**, no una melodía escrita. Para melodías ácidas de verdad haría falta nota por paso en la vista PASOS (no hecho).
- **Entrar y salir:** LFO de pista en triángulo, 16 compases, a TONO (+75) y AUSENCIA 20. Con TONO a 0 el modelo se apaga del todo (probado: el compás más flojo queda por debajo del 10 % del más fuerte).
- **Dónde:** pista 5 en lugar del dron en PULSAR, ECLIPSE, FOSFORO y ZENIT (banco A); patrones pares del banco B; 35 % de los generados. En los generados la pista 3 sale un 25 % de las veces con MACRO (cuerda, armónicos, grano) en vez de ACORDE.
- **Aleatorio por partes:** ARP + K2 en PATRONES elige TODO / GROOVE (pistas 1, 2, 6) / SINTES (3, 4, 5). GROOVE no toca la tonalidad. Probado: 40 aleatorios de cada tipo sin un solo cambio fuera de su parte.
- Al añadir el modelo, el mando MODELO pasa de 6 a 7 posiciones: un patrón guardado esta mañana con MACRO (versión 0.1.4) puede cargar con el modelo vecino. No se convierte: esa versión solo existió unas horas en el emulador.
- No escuchado. El nivel de la línea ácida se ha puesto por medida (pico 0,30 en solo), no de oído.

## Camino de vuelta en la unidad de Javier (2026-11-13)
- Comprobado con su backup del 10 nov: los primeros 16 KB de su flash (el arranque) **no son los de M-VAVE V16**: difieren los cuatro sectores. Es m-bootloader 2.3. Además su firmware "FM-1 801" no es la V16 (ningún sector de la aplicación coincide).
- Consecuencia: **`fm1_rescue` (la herramienta de rescate de fm1-base) no sirve para su unidad**. Solo escribe si el arranque es el de V15 o V16; con otro, se niega y no escribe nada.
- Su camino de vuelta es otro: **m-bootloader + M-Flasher**. REC al encender = modo descarga; PLAY al encender = menú de recuperación (se abre solo si el firmware no arranca). Con M-Flasher se reescribe su backup completo de 1 MiB (sha256 `b8ccc233…b5ea`).
- Condición: que el instalador **no** sustituya m-bootloader. La casilla "install m-bootloader" pondría la 1.2 sobre su 2.3: no marcarla.
- **Sin verificar:** que el instalador web de fm1-base funcione con el firmware 801 en marcha (está documentado para el oficial); que M-Flasher acepte nuestro `.fwsc`; que una instalación de fm1-base deje intacto el arranque en su unidad (según su documentación, sí: no escribe por debajo de 0x4000).
- Antes de instalar, sin escribir nada: encender con PLAY mantenido y con REC mantenido y ver que m-bootloader responde.

## Guardar el proyecto y borrar patrones (2026-11-13)
- **Proyecto** = lo que no es del patrón: mezcla (nivel, pan, envíos, PRE, mute), efectos y FLOW, modulación, escenas (bloqueos y fundido), tempo, evolución por pista, tonalidad fija, parte del aleatorio, banco y patrón. Un solo proyecto, ~600 bytes.
- **Flash:** objeto nuevo `OBJ_PROJECT` (número 25) en 0xC3000..0xC4FFF, justo encima de los bancos. Los objetos anteriores no se mueven. Formato "EPKJ" versión 1 con los recuentos dentro (pistas, parámetros, efectos…): una versión futura con más efectos lee la actual.
- **Gestos:** HOME + SAVE dos veces = guardar (antes decía "GLOBAL, FASE 6"). ARP + SAVE dos veces = borrar; el de fábrica vuelve al encender (reiniciar el motor con el audio en marcha no es seguro). ARP + tecla blanca dos veces en PATRONES = borrar esa ranura guardada.
- No se guarda solo (desgaste de la flash y porque en directo no siempre se quiere). No se guardan solos, escenas puestas, freeze ni octava.
- Borrar una ranura del banco en memoria recarga el banco: se pierde lo editado sin guardar en sus otras ranuras.
- Probado en el simulador (`app/tests/project.txt`): cambiar tempo, mute y nivel, guardar, reiniciar y encontrarlos; una sola pulsación no guarda; borrar y reiniciar devuelve el de fábrica; guardar y borrar una ranura. En el navegador real y en el FM-1: sin probar.

## Reloj MIDI y notas de entrada (2026-11-13)
- Entrada por USB, TRS y Bluetooth sin menú (modo automático): con reloj, se sigue; sin reloj, tempo interno.
- **Reloj:** periodo entre pulsos suavizado (8 % por pulso) → tempo; con Start, PLAY desde el principio en el pulso siguiente; mientras suena, corrección de fase proporcional (5 % del error en semicorcheas, tope ±4 %) comparando los pulsos contados con la posición del motor (`epk_pos16`). Un segundo sin pulsos = reloj perdido.
- La hora de cada pulso es la marca de llegada de fm1-base en el aparato (`plat_midi_in_t24`); en el simulador, su propio tiempo en ms (los guiones corren más rápido que el reloj real).
- **Notas:** canal 1-6 → pista 1-6; otro canal → pista elegida.
- **No hecho:** salida de reloj (necesita emitirse desde el audio para ser exacta), Continue con posición, CC, selección de fuente de reloj (si llegan dos relojes a la vez se mezclan: usar uno solo).
- Supuesto sin medir: que el bucle principal lea los pulsos con suficiente frecuencia en el aparato. La marca de tiempo exacta en USB/TRS pide `midi_stamp = true` en fm1.toml, **que no está activado** (no sé su coste); hoy la hora es la de lectura. Bluetooth llega a ráfagas: seguirá peor.
- Probado en el simulador (`app/tests/midi.txt`): nota por canal 3 dispara la pista 3; Start + 200 pulsos a 25 ms → suena y el tempo queda en 100; al callarse el reloj sigue sonando a 100; Stop para.
