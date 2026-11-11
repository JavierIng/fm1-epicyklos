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
- Tempo hasta **150 BPM** (antes 40–200; se mantiene, pero los presets por defecto cubren 120–140).
- **METAL**: modos 808 y **909** (hat/ride con ruido + metálico).
- **GRAVE**: modo "industrial" (drive y cola de rumble más larga).

## Efectos nuevos

| Idea | Qué haríamos | Código | Coste estimado |
|---|---|---|---|
| **MAREA** (inspirado en Slink de Hypnus) | Banco de 8–16 filtros paso-banda repartidos entre L y R. Sus niveles los mueve una "ola" (suma de senos desfasados) | Propio. Slink es cerrado: solo la idea | 16 bandas ≈ 250–350 ciclos/muestra. Opción de 8 bandas |
| **PLACA** (Plateau de Valley) | Reverb de placa Dattorro con modulación del tanque, tamaño y "congelar"; sustituye a la reverb tipo Clouds | **Portable**: Plateau es GPL-3.0 (Dale Johnson) | Tanque en 16 bit a 22 kHz ≈ 35–50 KB de RAM; ~400 ciclos |
| **RESONADOR** (SMR de 4ms) | 6 resonadores afinados a escala, rotables; como efecto de pista o del master | **Portable**: el código del SMR es MIT (sin usar la marca 4ms) | ~150–250 ciclos |
| Intellijel (Rainmaker, Plonk, Metropolix) | Delay multi-tap con comb; percusión física; secuenciador por etapas (ya en ETAPAS) | Cerrado: solo ideas | Depende |
| Erica Synths | Carácter de saturación/filtros Black series, drum synths | Cerrado: solo ideas | — |

## Cadena de efectos propuesta (a validar con CPU real)

Envío A: delay dub → (opcional) envío a B. Envío B: **PLACA**. Inserto del master: **MAREA** u otro filtro. Por pista: inserto opcional **RESONADOR**.

Sin medir en el chip no se puede tener todo a la vez: MAREA (16 bandas) + PLACA + delay + 6 motores + 6 compresores probablemente supera el presupuesto de un núcleo. Por eso habrá un **medidor de CPU** y los efectos pesados se elegirán uno a uno.
