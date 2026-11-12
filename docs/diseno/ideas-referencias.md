# Ideas y referencias (abierto; se encaja en cada fase)

## Artistas y lo que piden al instrumento

| Referencia | Lo que implica en el diseño |
|---|---|
| Basic Channel, Deepchord, Luigi Tozzi, Neel, Mike Parker | Acordes dub en delay, hats finos, bucles largos que mutan poco a poco |
| **Jeff Mills** | Bucles de 909 hipnóticos y largos; hats y rides 909; cambios por mute/filtro, no por arreglo |
| **Oscar Mulero, Reeko, Architectural** | Techno hipnótico/industrial español: bombo con drive y cola, percusión metálica seca, loops-herramienta, tempos 130–145 |
| Rambadu, Orphx, Abdulla Rashim, Northern Electronics | Rumble grave, percusión resonante, texturas oscuras |
| Plastikman, Sleeparchive | Minimalismo, 303/101, microvariaciones |
| Hypnus Records, Toki Fuko, ambient | Drones, filtros que se mueven como agua, espacios grandes |

Consecuencias concretas:
- Rango de tempo 40–200 BPM; los proyectos de inicio, entre 120 y 140.
- **METAL**: modos 808 y **909** (hat/ride con ruido + metálico).
- **GRAVE**: modo "industrial" (drive y cola de rumble más larga).

## Efectos nuevos

| Idea | Qué haríamos | Código | Coste estimado |
|---|---|---|---|
| **MAREA** (inspirado en Slink de Hypnus) — **hecho (12 nov): inserto de pista, 16 bandas, 4 senos** | Banco de 8–16 filtros paso-banda repartidos entre L y R. Sus niveles los mueve una "ola" (suma de senos desfasados) | Propio. Slink es cerrado: solo la idea | 16 bandas ≈ 250–350 ciclos/muestra. Opción de 8 bandas |
| **PLACA** (Plateau de Valley) | Reverb de placa Dattorro con modulación del tanque, tamaño y "congelar"; sustituye a la reverb tipo Clouds | **Portable**: Plateau es GPL-3.0 (Dale Johnson) | Tanque en 16 bit a 22 kHz ≈ 35–50 KB de RAM; ~400 ciclos |
| **RESONADOR** (SMR de 4ms) | 6 resonadores afinados a escala, rotables; como efecto de pista o del master | **Portable**: el código del SMR es MIT (sin usar la marca 4ms) | ~150–250 ciclos |
| Intellijel (Rainmaker, Plonk, Metropolix) | Delay multi-tap con comb; percusión física; secuenciador por etapas (ya en ETAPAS) | Cerrado: solo ideas | Depende |
| Erica Synths | Carácter de saturación/filtros Black series, drum synths | Cerrado: solo ideas | — |

## Cadena de efectos propuesta (a validar con CPU real)

Envío A: delay dub → (opcional) envío a B. Envío B: **PLACA**. Inserto del master: **MAREA** u otro filtro. Por pista: inserto opcional **RESONADOR**.

Sin medir en el chip no se puede tener todo a la vez: MAREA (16 bandas) + PLACA + delay + 6 motores probablemente supera el presupuesto de un núcleo. Por eso habrá un **medidor de CPU** y los efectos pesados se elegirán uno a uno.

## Delay: tipos y lo que lo saca de lo normal

Un buffer, tres tipos (KNOB TIPO en la página DELAY):

| Tipo | Carácter | Cómo |
|---|---|---|
| **TAPE** | Eco de cinta dub: oscuro, con wow/flutter, saturación que crece con el feedback, desgaste | Interpolación con la velocidad de cinta modulada; cambiar el tiempo **desliza el tono** como al mover la velocidad de un Space Echo, en vez de saltar |
| **BBD** | Analógico de cubos (bucket brigade): más oscuro, ruido de reloj, compansión, granulado al alargar el tiempo | Remuestreo a la frecuencia del "reloj" del BBD, filtro anti-alias, compansor; tiempos largos = más sucio |
| **PINGPONG** | Estéreo alternando L/R, limpio o con color | El buffer partido en dos (2 × 0,75 s) con cruce de feedback |

Lo que lo hace distinto de un delay normal (todo opcional, a medir en CPU):
- **Cabezales múltiples** (TAPE): 3 cabezales a 1×, 2× y 3× con combinaciones, como los ecos de cinta de dub.
- **Feedback resonante:** el RESONADOR (SMR) dentro del lazo de feedback, afinado a la escala → los ecos se convierten en notas.
- **Filtro en el lazo con barrido:** cada repetición más oscura o más fina; un knob que barre en vivo.
- **Duck del delay por el bombo:** las repeticiones respiran con el kick.
- **Throw** (REC), **freeze** (HOME+REC, el buffer queda en bucle) y **auto-oscilación controlada** (sirena dub con el limitador interno).
- **Cinta que se degrada** (modo largo para drone): cada pasada pierde agudos y gana ruido, a lo Basinski.

### Más ideas para el delay (11 nov, tarde), todas controlables con un knob o un botón

| Idea | Qué hace | Control | Coste |
|---|---|---|---|
| **Tiempos en pasos** | El tiempo del delay se elige en pasos de 1/16: 3, 5, 7, 11... Los ecos caen fuera del 4/4 y crean polirritmos con las órbitas | KNOB TIEMPO en pasos, además de 1/8 con puntillo, etc. | Nulo |
| **Parada de cinta** | Frenar la cinta (el tono cae hasta parar) y arrancarla; también en reversa con el freeze | Mantener HOME+REC = frenar; soltar = arrancar | Bajo |
| **Desgaste (dropouts)** | Cada repetición puede perderse o salir apagada, con una probabilidad | KNOB DESGASTE | Bajo |
| **Espacio infinito** | Feedback cruzado delay ↔ reverb con el limitador vigilando: una nube que no muere | KNOB DLY↔REV | Bajo (lo que ya cuesta la reverb) |
| **Throws grabados** | Grabar en el patrón los envíos al delay de cada paso, como hace el dub en la mesa | REC + girar PRESETS mientras suena | Nulo en audio (memoria del patrón) |
| **Eco a la octava** | Cada repetición sube o baja una octava (shimmer oscuro) | KNOB OCTAVA en el lazo | **Alto**: necesita desplazador de tono; solo si sobra CPU |

## ESCENAS (propuesta 11 nov, 15:58; inspirado en las scenes/performance de Elektron)

Una escena es un conjunto de destinos (hasta 8: parámetro + valor) sobre el master, los efectos y las pistas, que
se activa de golpe y se suelta para volver. **Son del proyecto** (como la mezcla): sobreviven al cambio de patrón.

| Escena de ejemplo | Qué mueve |
|---|---|
| **SUMERGIR** | Filtro del master se cierra (LP) + envío y feedback del delay suben |
| **CATEDRAL** | Envío y tamaño de la reverb suben, el delay baja |
| **CINTA ROTA** | Drive + saturación de cinta + wow/flutter del delay |
| **DIGITAL** | Reducción de bits y de frecuencia de muestreo (bit/sample-rate reduction) |
| **TÚNEL** | Filtro paso-alto sube + reverb |
| **SECO** | Todos los envíos a 0: el golpe en seco antes de volver |
| **STUTTER** | Repetición de un paso (beat repeat) con el buffer del delay |
| **ECO INFINITO** | Freeze del delay + entrada cortada |

- **Cómo se tocan:** modo de teclado ESCENAS (HOME+SEQ). Las teclas negras son las escenas: mantener = activa,
  soltar = vuelve; HOME + tecla = se queda puesta (latch). Las blancas siguen tocando la pista. Con presión en las
  teclas (pendiente en el dispositivo), la presión sería la profundidad, como los pads del Rytm.
- **Transición:** tiempo de morph por escena (instantáneo .. 4 compases) para subidas y bajadas de dub.
- **Cómo se crean:** mantener la tecla de la escena y girar knobs: lo que gires queda guardado en la escena.
- **Coste:** las escenas no cuestan CPU; la reducción de bits y el stutter, poco.
- **Fase:** después de los efectos del master (Fase 3), porque mueven sus parámetros: Fase 3–4.
- **Decidido (11 nov, 16:58): las dos.** REDUCCIÓN (bits + frecuencia de muestreo, como BR/SRR del Rytm) en la
  cadena del master, y STUTTER (repetición de 1/4, 1/8, 1/16 o 1/32 con el buffer del delay).

## MODULACIÓN: LFOs y seguidores de envolvente (propuesta 11 nov, 16:58)

| Fuente | Dónde | Qué es |
|---|---|---|
| **LFO de pista** (1 por pista) | Página MOVIMIENTO de cada pista | Formas: seno, triángulo, sierra, cuadrada, S&H, aleatorio suave. Sincronizado (1/16 .. 64 compases) o libre. Reinicio: libre, por compás o por cambio de patrón. 1 destino de la pista + profundidad |
| **DERIVA** (en la misma página) | Por pista | Paseo aleatorio muy lento sobre la afinación y el tono: lo que hace "vivir" a un drone |
| **LFO globales** (2) | Página MOD del master | Igual que los de pista, pero para el master y los efectos: un barrido de filtro de 32 compases, el tiempo del delay que respira |
| **SEGUIDORES** (2) | Página MOD del master | Siguen el nivel de una pista (o del master) con ataque y caída: el bombo abre el delay, la percusión cierra el filtro del acorde, el bajo "bombea" la reverb. El DUCK es un caso de estos (seguidor del bombo → bajar volumen) |

- Cada fuente global tiene **2 destinos** con su cantidad (+/−). Suficiente para el dub sin convertir la pantalla en
  una matriz ilegible de 4 knobs; si se queda corto, se amplía.
- **Coste:** los LFOs se calculan una vez por bloque de control (32 muestras): casi nada. Un seguidor ≈ 5 ciclos por
  muestra. Lo caro sería modular a ritmo de audio; no se hará salvo en los motores que lo lleven dentro.
- Las **ESCENAS** pueden mover profundidades y velocidades de LFO (p. ej. una escena que acelera el LFO del filtro).
- **Fase:** el LFO de pista y la deriva con los motores (Fases 2b–5); los LFO globales y seguidores con los efectos
  del master (Fase 3).

## Ideas del 11 nov (tarde): granular, Clouds/Warps, filtro doble — sin decidir

Apuntadas para ir viendo; ninguna está comprometida.

- **NIEBLA como granular que se graba solo:** buffer circular que captura una pista o la mezcla sin pulsar nada; congelar para quedarse con el trozo. Controles: posición, tamaño, densidad, afinación en escala, dispersión, granos al revés. No hay entrada de audio confirmada ni sitio en flash para muestras, así que el material sale del propio aparato y se pierde al apagar.
  - Memoria: 2 s a 22 kHz/16 bit = 88 KB (de ~270 KB libres, a repartir con delay y reverb). Alternativa: leer del buffer del delay (0 KB extra, pero quedan atados).
  - Abierto: ¿pista (ocupa una de las seis) o efecto global?
- **Código de Mutable Instruments** (repo `pichenettes/eurorack`; STM32 = MIT, compatible con GPL-3.0, con crédito):
  - Clouds: aprovechar solo el motor de granos; sin modos espectrales (FFT demasiado cara).
  - Warps: wavefolder y modulación en anillo sí; vocoder solo si sobra CPU.
  - Beads: no está publicado; solo como inspiración.
  - Blades: es analógico (pendiente de confirmar mirando la carpeta); se escribiría un filtro doble digital con su idea.
- **Filtro doble tipo Blades:** dos SVF con enrutado serie/paralelo y mezcla LP/BP/HP; candidato a ser el filtro DJ del master. Frecuencia y resonancia como destinos de los LFO globales y los seguidores de envolvente (con suavizado y límite de resonancia).
- **Riesgo:** la lista supera lo que cabe sonando a la vez; la CPU real no se conoce hasta probar en el aparato. Lo pesado, con interruptor de calidad.
- **Flanger** (11 nov, 19:27): solo si la CPU lo permite. Es barato (una línea de retardo corta modulada, ~1-2 KB); encajaría como efecto de pista o dentro del delay. Sin decidir.
- **Otros pendientes:** tap tempo; marca de "modificado" y nombre nuevo al aleatorizar un patrón; interfaz en inglés.

## Patrones de muestra por estilo (pedido el 12 nov, para más adelante)

- **Banco A:** 16 patrones más ambient y jungle.
- **Banco B:** 16 patrones más deep y dub.
- **Banco C:** 16 patrones de techno hipnótico, raw.
- Javier puede pasar ejemplos de referencia si hacen falta. Se hará después de los motores de síntesis, la vista FLOW y las escenas, cuando haya sonidos con los que escribirlos. Requisito técnico: los bancos B y C pasan de generados a escritos a mano (hoy B–K salen de una semilla); jungle pide también breaks rápidos (división 1/32, swing y micro ya existen) y quizá un motor o modo de break.
