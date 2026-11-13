# EPICYKLOS · Manual de usuario (borrador)

> Estado: **Fase 4b, solo emulador de navegador.** Nada de esto se ha probado todavía en un FM-1 real.
> Este manual crece con cada fase y solo describe lo que ya funciona. English: [en.md](en.md).

## 1. Qué es
Firmware abierto (GPL-3.0) para el M-VAVE FM-1: seis pistas con secuenciador euclidiano y polimétrico, pensado
para dub techno, techno hipnótico, ambient y drone.

## 2. Al encender
Una galaxia en espiral gira unos 3 segundos con EPICYKLOS latiendo en el centro. Tocar cualquier botón, tecla o
knob la salta.

## 2b. El emulador
- **Clic en la pantalla o en PLAY** para activar el sonido (el navegador lo exige).
- **Mantener un botón y girar un knob** (por ejemplo HOME + SELECT): **Mayús + clic** (o clic derecho) deja el
  botón pulsado, con borde azul; gira el knob; clic normal para soltarlo.
- Los knobs giran arrastrando o con la rueda del ratón.

## 3. Mapa del panel
| Control | Solo | Con HOME mantenido |
|---|---|---|
| FX SCL ENV LFO EDIT GLO | elegir pista 1-6 | mute de la pista |
| (en vista MEZCLA) | mute de la pista | solo de la pista |
| HOME | toque: ÓRBITA → MEZCLA → EFECTOS → FLOW → MOD | es la tecla de función |
| PLAY | arrancar / parar | **congelar la evolución** de todas las pistas |
| SELECT | cambiar de página | **tempo** (1 BPM por clic) |
| PRESETS | nivel de la pista | envío de la pista al eco |
| K1-K4 | los 4 parámetros de la página | ajuste fino |
| OCT- / OCT+ | octava del teclado | — |
| SAVE | vista PATRONES | en PATRONES, HOME + tecla blanca: **guardar** |
| SEQ | vista PASOS | modo **ESCENAS** (teclas negras) |
| ARP | aleatorio; mantenido + pista: evolución sí / no | **deshacer** |
| REC | mantener: **throw** de la pista al eco | **freeze** del eco (activa / quita) |
| ALGORITHM | fase siguiente | **filtro del master**, desde cualquier vista |

Al mantener HOME, la parte baja de la pantalla muestra esta chuleta.

## 4. Pistas y motores
Cada pista tiene un motor de sonido (página MOTOR, K1). Por ahora:
- **GRAVE** (pista 1): bombo. NOTA, DRIVE (de limpio a industrial), CLICK, CAÍDA, BARRIDO, RUMBLE (cola grave).
- **METAL** (pista 2): platos tipo 808/909. AFINA, METAL (de ruido a metálico), BRILLO, CAÍDA, ANILLO, VARÍA.
- **GOLPE** (pista 6): palma, caja o aro. AFINA, TIPO (PALMA → CAJA → ARO), BRILLO, CAÍDA, CUERPO, COLA.
- **TAMBOR** (se elige en MOTOR; por defecto en la pista 6 de algunos patrones): toms, congas y percusión
  metálica. AFINA, TIMBRE (de tom redondo a metálico), FM (brillo del ataque), CAÍDA, BARRIDO, CLICK.
- **ACORDE DUB** (pista 3): el acorde de dub techno. Cuatro voces, cada una con dos sierras ligeramente
  desafinadas, por un filtro paso bajo resonante con su propia envolvente.
  - NOTA (la nota base), ACORDE (qué acorde de la tabla; en pantalla sale su nombre: MIN7, MIN9, SUS4…), TONO (dónde
    se cierra el filtro) y CAÍDA.
  - En la página MOTOR: TABLA (Original, Colour Chords, Disastrous Peace o Tonnetz Walk, las tablas de Plaits
    Palette), ENVOLV (cuánto abre el filtro al golpear) y ATAQUE.
  - En Tonnetz Walk los nombres (C, AM, F…) son relativos: con NOTA en re, "C" suena como un re mayor.
- **DRONE** (pista 5): se **sostiene**. Cada golpe lo abre o lo mantiene; si cambia la nota, se desliza hasta ella.
  - NOTA, TIMBRE (seno → triángulo → sierra → sierra con aire), TONO (filtro) y SOSTÉN (cuánto se mantiene tras el
    último golpe: de 0,2 a 30 s; luego se apaga en unos 2 s).
  - En la página MOTOR: DERIVA (vagabundeo lento de afinación, separación y filtro: lo que le da vida), INTERVALO
    (la segunda voz: unísono, 3ª menor, cuarta, quinta, octava, quinta + octava) y ATAQUE (lo que tarda en entrar,
    hasta 8 s).
  - Al parar el secuenciador, el dron se apaga solo.
- **MACRO** (pista 4, el bajo; sirve también para melodías): siete maneras de hacer un sonido detrás de los mismos
  mandos. La idea es la de Plaits, de Mutable Instruments; el código es propio, no es un port.
  - En la página MOTOR: **MODELO**, **ARMON** (el segundo mando del modelo) y ATAQUE.
  - En la página SONIDO: NOTA, **TIMBRE** (el primer mando del modelo), TONO y CAÍDA.
  - **TONO** es un filtro que la propia nota abre y cierra: arriba del todo está abierto; al bajarlo, cada nota
    empieza abierta y se va cerrando mientras se apaga (el sonido "pluck" del bajo dub).
  - Los modelos:
    - **ANALOG**: dos osciladores. TIMBRE va de sierra a cuadrada y a pulso estrecho. ARMON desafina el segundo
      y, a partir de la mitad, lo pone una octava abajo, a la quinta, a la octava o a la quinta + octava.
    - **PLIEGUE**: un seno doblado sobre sí mismo. TIMBRE: cuántos pliegues. ARMON: asimetría.
    - **FM**: dos operadores. ARMON: la proporción (0,5 · 1 · 1,5 · 2 · 3 · 3,5 · 4 · 5 · 7). TIMBRE: cuánta FM
      (baja sola al apagarse la nota).
    - **GRANO**: dos formantes, parecido a una voz. TIMBRE: el formante. ARMON: dónde cae el segundo.
    - **CUERDA**: cuerda pulsada. TIMBRE: brillo. ARMON: dureza de la púa. CAÍDA es lo que resuena.
    - **ARMONICO**: ocho armónicos de seno. ARMON: cuáles suenan. TIMBRE: cuántos a la vez.
    - **ACIDO**: la línea ácida. Sierra a cuadrada (TIMBRE) por un filtro resonante de 18 dB por octava que
      cada nota abre de golpe. Aquí ARMON se llama **RESO** (resonancia) y ATAQUE se llama **SALTOS**: cuántas
      notas saltan una octava y se ligan con la siguiente, en un bucle de 16 golpes. Tres de cada diez notas van
      acentuadas. Con TONO abajo del todo la línea desaparece: es la manera de hacerla entrar y salir. No es una
      emulación de ninguna máquina concreta.
  - Solo hay dos mandos por modelo (Plaits tiene tres) y falta el resto de modelos de Plaits (acordes ya está en
    ACORDE; no hay voz, tabla de ondas, ruido ni percusión).
- **PRUEBA**: la voz provisional que antes hacía de bajo. Sigue en la lista de motores.

## 5. Páginas (SELECT)
SONIDO · MOTOR · RITMO (LONGITUD, GOLPES, ROTAR, DIVISIÓN) · GROOVE (SWING, DESPLAZA, AUSENCIA) · EVOLUCIÓN (MUTAR,
QUÉ, CADA, VUELTA) · MOVIMIENTO (FORMA, VELOC, DESTINO, CANT) · MEZCLA (NIVEL, PAN, A ECO, B REVERB). Como no caben todas, arriba se ven la anterior, la actual y
la siguiente, con el número de página.

### EVOLUCIÓN: que el patrón cambie solo
**Es por pista:** los knobs muestran y cambian la pista elegida; cada una de las seis tiene los suyos y se guardan
con el patrón. Cada pista puede ir cambiando poco a poco sin que toques nada. El patrón escrito no se modifica: lo
que cambia es un desvío sobre él, y al cambiar de patrón o pulsar PLAY se vuelve a empezar desde lo escrito.
- **MUTAR:** cuánto cambia cada vez. Lo que cambia: golpes (uno más o uno menos), rotación, tono, caída, carácter
  (en ACORDE, el acorde) y, en ACORDE, DRONE, MACRO y PRUEBA, la nota (cuarta, quinta, octava). Nunca deja la pista sin
  golpes: eso es cosa de AUSENCIA.
- **QUÉ:** qué puede cambiar. TODO, RITMO (golpes y rotación), SONIDO (tono, caída, carácter/acorde) o NOTA. Por
  ejemplo, platos en SONIDO cambian de color sin mover el groove; ACORDE en SONIDO cambia de acorde en su sitio.
- **CADA:** cada cuántos compases cambia: 1, 2, 4, 8 o 16. El primer tramo suena siempre tal como está escrito.
- **VUELTA:** cuánto vuelve hacia lo escrito cada vez. Con VUELTA alta se aleja poco y regresa; con VUELTA baja se
  va lejos.
- **AUSENCIA** (está en la página GROOVE, porque decide si la pista suena): probabilidad de que descanse cada
  tramo de CADA compases. En dub, lo que quitas cuenta tanto como lo que suena.
- **Congelar:** MUTAR 0 y VUELTA 0 dejan la pista como está en ese momento. Si te gusta, guárdala (sección 6).
- **ARP mantenido + botón de pista:** apaga o enciende la evolución de esa pista. Apagada, vuelve a lo escrito en el
  siguiente compás y se queda ahí; sus knobs se conservan para cuando la enciendas.
- **HOME + PLAY:** congela la evolución de todas las pistas tal como están ahora (pone CONGELADA); otra vez, la
  suelta.
- En la página EVOLUCIÓN, arriba a la derecha, seis cuadrados: color vivo = la pista evoluciona; apagado = tiene
  MUTAR y AUSENCIA a 0; gris = apagada con ARP. La raya marca la pista elegida.
- De fábrica, en el banco A **solo evolucionan los sintes** (acorde y bajo cada 4 compases, dron cada 8). Bombo,
  platos y percusión vienen quietos. En los bancos generados pasa lo mismo, en dosis distintas.
- ARP en estas páginas cambia MUTAR, CADA, VUELTA y AUSENCIA al azar (AUSENCIA como mucho 25 %); QUÉ no lo toca.

### MOVIMIENTO: el LFO de cada pista
Cada pista tiene su propio LFO, que se guarda con el patrón. Con el secuenciador en marcha va enganchado al compás.
- **FORMA:** SENO, TRIANG, SIERRA, CUADRA, S&H (un valor nuevo cada ciclo), AZAR (de un valor al siguiente, suave) y
  **DERIVA**: un paseo al azar, lento y sin ciclo, que nunca se repite. Es lo que hace "vivir" un sonido fijo.
- **VELOC:** lo que dura un ciclo, de 1/16 a 32 compases. En DERIVA, cuánto vaga en ese tiempo.
- **DESTINO:** TONO, CAÍDA, CARÁCTER, los dos knobs del motor (con su nombre: ENVOLV, DERIVA, CUERPO…), NIVEL, PAN,
  ENVÍO A, ENVÍO B, **GOLPES** o **ROTAR**. Con GOLPES o ROTAR es el ritmo el que respira: la pista gana y pierde
  golpes o los desplaza durante el ciclo.
- **CANT:** cuánto, de −100 a +100. A +100 recorre medio knob arriba y abajo; en negativo, al revés. 0 = apagado.
- No mueve el knob: se suma a lo que marque. Al guardar el patrón, el LFO se guarda con sus ajustes y sigue
  moviéndose al cargarlo.
- De fábrica, en el banco A, el acorde respira hacia el eco (ENVÍO A, 4 compases) y el tono del bajo hace DERIVA.

## 6. Patrones y bancos (SAVE)
- **Teclas negras = bancos A a K. Teclas blancas = los 16 patrones del banco.**
- Con el secuenciador parado el cambio es inmediato; sonando, espera al final del compás (parpadea "en cola").
- Mantener SAVE + tecla hace lo mismo desde cualquier vista.
- Un patrón guarda la secuencia y el sonido de cada pista. Mezcla, tempo, **mute y solo son del proyecto**:
  no cambian al cambiar de patrón.
- **Banco A, dub techno** (en re): 16 patrones hechos a mano, de NEBULA a CYKLOS.
- **Banco B, techno hipnótico**: de UMBRAL a RETORNO. Bombo a negras, bajo MACRO en un ciclo impar (7, 9, 11, 13
  o 15 pasos) que gira contra el bombo, acorde oscuro y escaso, percusión en ciclo corto.
- **Banco C, ambient y drone**: de ALBA a OCASO. Sin bombo (solo uno suave a medio tiempo en 9, 11 y 15), dron
  siempre presente, acordes lentos, una melodía de cuerda, armónicos o grano que va cambiando de notas, una campana
  lejana.
- Los tres siguen el mismo recorrido: entrada (1-3), subida (4-7), pausa (8), segunda subida (9-14), pico (15) y
  salida (16). B y C están generados con reglas, no compuestos a mano: son siempre los mismos.
- Bancos D-K: patrones generados libres (GEN D01…), con el bajo en MACRO.
- **Líneas ácidas en lugar del dron** (pista 5 con MACRO ACIDO): en el banco A, en PULSAR, ECLIPSE, FOSFORO y
  ZENIT; en el banco B, en los patrones pares (los impares llevan dron); en los generados, uno de cada tres.
  Vienen con el filtro subiendo y bajando en 16 compases (página MOVIMIENTO: TRIANG a TONO) hasta cerrarse del
  todo, y con AUSENCIA 20 (descansan algún tramo entero). Para que entre y salga a mano: TONO de la pista 5, o
  una escena.

### Guardar un patrón (HOME + tecla blanca)
1. Pulsa SAVE para ir a la vista PATRONES y elige el banco de destino con una tecla negra.
2. Con HOME mantenido, pulsa la tecla blanca de la ranura: pregunta "GUARDAR EN B02 ?".
3. Vuelve a pulsar la misma tecla en menos de 3 s: "GUARDADO".
- Se guarda el patrón **tal como suena en ese momento**, con su evolución dentro. Así, si un aleatorio o una
  mutación te gusta, lo conservas.
- Un patrón guardado lleva `*` tras el nombre. Al cargar su banco, sustituye al de fábrica o al generado.
- Si guardas en la ranura que está sonando, lo que oías pasa a ser el patrón escrito.
- **Dónde se guarda:** en el emulador, en el navegador (sobrevive a recargar la página, pero no a borrar los
  datos del sitio ni a cambiar de navegador). En el FM-1, en la flash, con el mismo código: *pendiente en el
  dispositivo*, porque no se ha instalado nada en él.
- **Lo que no se guarda todavía:** lo que edites sin guardar se pierde al cambiar de banco; la mezcla, los
  efectos, el tempo y la modulación (son del proyecto) se pierden al apagar. Tampoco hay forma de borrar un
  patrón guardado y volver al de fábrica: llegará con el menú de proyecto (fase 6).

## 7. Pasos (SEQ)
- Las 16 teclas blancas son los pasos; OCT-/OCT+ cambia entre 1-16 y 17-32.
- Toque: enciende o apaga el paso (la pista pasa a "A MANO").
- **Mantener un paso + K1:** probabilidad (100, 75, 50, 25 %). **+ K2:** condición (1:2, 2:2, 1:4…: suena solo
  en esa vuelta del ciclo).
- **Mantener un paso + K3:** micro, mueve ese golpe antes (−) o después (+) en octavos de paso, hasta 3/8.
- Girar GOLPES en una pista hecha a mano la devuelve al reparto euclidiano.

### Adelantar y atrasar (página GROOVE)
- **SWING:** atrasa los pasos impares de la pista (hasta 0,4 de paso).
- **DESPLAZA:** mueve la pista entera antes o después (±45 % de un paso). Por ejemplo, la palma un poco
  adelantada o los platos un poco tarde.
- Swing, desplazamiento y micro se suman, con un tope de 0,9 de paso.

## 8. Aleatorio (ARP)
- **ARP:** cambia los parámetros de la página que estás viendo, en la pista elegida. No toca motor, longitud,
  división ni nivel.
- **ARP en la vista PATRONES:** patrón nuevo entero (sonidos y secuencias de las seis pistas). No es un bucle de
  patrones: cada pulsación sale de una semilla distinta.
  - El bombo sale de ocho familias: sin bombo, a negras (algo menos de la mitad de las veces), a negras con un golpe
    fantasma al 50 %, roto (euclidiano de 3, 5 o 7), medio tiempo, 2-step, con puntillo (0-6-12) y en un ciclo
    propio de 12, 14 o 15 pasos.
  - Cada vez sale en una **tonalidad menor nueva**; la pantalla la dice ("TODO EN SOL m") y arriba a la derecha de
    la vista PATRONES se ve la actual.
  - **ARP mantenido + K1** (en la vista PATRONES): **tonalidad fija**. Los aleatorios siguientes se quedan en la
    tonalidad actual (pone FIJA). Útil para que los patrones encajen entre sí. Otra vez, libre.
  - Cada banco tiene su tonalidad: el A en re menor, los generados repartidos.
  - **ARP mantenido + K2** (en la vista PATRONES): **qué parte renueva**. TODO, **GROOVE** (pistas 1, 2 y 6: bombo,
    platos y percusión) o **SINTES** (pistas 3, 4 y 5). La parte elegida se ve en amarillo bajo la tonalidad.
    - GROOVE deja los sintes y la tonalidad como están.
    - SINTES deja el groove y cambia también los **motores**: la pista 3 puede salir con ACORDE o con MACRO (cuerda,
      armónicos o grano), la 4 con cualquier modelo de MACRO y la 5 con DRONE o con una línea ácida.
- **HOME + ARP:** deshace el último aleatorio (un solo nivel).
- **ARP mantenido + ALGORITHM:** cantidad (10-100 %). **ARP mantenido + un knob:** bloquea ese parámetro.

## 9. Efectos de envío (vista EFECTOS)
Cada pista manda parte de su sonido al **eco (A)** y a la **reverb (B)** desde la página MEZCLA (K3 y K4). El
envío sale después del nivel: si silencias una pista deja de enviar, pero la cola sigue sonando.
- **Envío PRE (antes del nivel):** en la página MEZCLA, **HOME + K3** (o K4) cambia ese envío a PRE; el knob pasa a
  llamarse "A PRE" o "B PRE". En PRE el envío no depende del NIVEL: puedes bajar la pista a 0 y dejar sonando solo
  su eco o su reverb (pista "fantasma", muy de dub). Cada pista tiene un solo ajuste PRE para A y B a la vez. Es de la
  mezcla, como NIVEL y PAN: no cambia al cambiar de patrón. Ojo: el MUTE sí corta también el envío PRE.

En la vista EFECTOS, SELECT cambia de página y K1-K4 son los controles:
- **ECO:** TIPO (TAPE, BBD, PINGPONG), TIEMPO (en semicorcheas, de 1 a 16: prueba 3, 5 o 7), REPITE (arriba del
  todo el eco se realimenta solo), TONO (cada repetición más oscura).
- **ECO 2:** DESGASTE (cinta gastada en TAPE, reloj sucio en BBD), A REVERB (el eco entra en la reverb), NIVEL,
  DUCK (el bombo de la pista 1 baja el eco y la reverb mientras suena; vuelven en ~150 ms).
- **REVERB:** K1 **TIPO** elige **PLACA** o **NUBE**; los otros tres knobs dependen del tipo. Al cambiar de tipo,
  la reverb empieza vacía. Solo suena una de las dos (comparten memoria).
- **REVERB 2:** CAÍDA, TONO (lo oscura que es la cola), MOVER (movimiento interno), NIVEL. En NUBE cambia (ver abajo).
- **PLACA**, a la manera de la reverb **Plateau** de Valley (sus controles; el código es propio): la placa clásica
  de Dattorro.
  - **PREDELAY:** silencio antes de la cola, de 0 a 500 ms (sale en ms). Separa el golpe de su reverb.
  - **TAMAÑO:** de sala pequeña (0) a una nave enorme (100): de un cuarto a dos veces la placa clásica. Viene en
    43, que es el tamaño clásico. Moverlo mientras suena hace saltos y cambios de tono, como en Plateau.
  - **DIFUSIÓN:** abajo se oyen los primeros ecos sueltos; arriba todo se funde desde el principio.
  - **HOME + REC** congela también la placa: deja de entrar sonido y la cola se queda sonando.
- **NUBE**: una nube de granos con la idea del módulo Clouds de Mutable Instruments (el código es propio). Escucha
  lo que le mandan los envíos B, guarda los últimos 2 segundos y los reparte en trocitos (granos) por el estéreo. En
  REVERB:
  - **POSICIÓN:** de dónde salen los granos: de lo que acaba de sonar (0) a casi 2 segundos atrás (100).
  - **TAMAÑO:** granos cortos (~20 ms, chisporroteo) a largos (~1 s, nube).
  - **DENSIDAD:** en el centro, NADA (no salen granos nuevos); a la izquierda, granos regulares (REG); a la derecha,
    al azar (AZAR) y con la posición algo desordenada. Cuanto más lejos del centro, más granos (12 a la vez como
    máximo).
  - En NUBE la página **REVERB 2** cambia: **REPITE** (los granos vuelven a entrar en la nube), **TEXTURA** (granos de
    borde duro a la izquierda, suaves a la derecha, y algo más oscuros), **AFINA** (de −24 a +24 semitonos, de uno en
    uno) y NIVEL.
  - **HOME + REC** congela también la nube: deja de grabar y los granos siguen saliendo de lo que tiene. Arriba se
    ve FREEZE.
  - En FLOW el bloque 3 se llama NUBE cuando está puesta.
  - **Shimmer en NUBE:** AFINA +12 y REPITE alto. Los granos vuelven a entrar una octava más arriba.
  - **Reverb de la nube** (en REVERB 3, como en Clouds): los granos pasan por una placa pequeña propia. **REVERB**
    es cuánta (viene en 45; 0 = la nube seca) y **CAIDA** cuánto dura. Con HOME + REC también se congela.
- **REVERB 3:**
  - En NUBE, los dos primeros knobs son **REVERB** y **CAIDA** de la nube (ver arriba).
  - **SHIMMER** (solo en PLACA): la cola sube de tono y vuelve a entrar en la reverb, así que va subiendo y
    brillando. 0 = NO. **INTERVALO:** +12 (octava), +7 (quinta), +19 (octava y quinta), +24 (dos octavas) o −12 (una
    octava abajo: se oscurece). Con la reverb congelada (HOME + REC) el shimmer se para.
  - **GRAVES:** corte de graves antes de la reverb, de 20 a ~400 Hz (viene en ~90 Hz). Más alto = menos barro del
    bombo y el bajo en la cola. Vale para PLACA y NUBE.
  - **ANCHO:** 0 = reverb en mono, 50 = tal cual, 100 = más abierta. Vale para PLACA y NUBE.
- **FILTRO** (del master, después de los retornos): TIPO, FILTRO, RESO y un cuarto knob que cambia con el tipo.
  - **DJ:** a la izquierda cierra un paso bajo, a la derecha abre un paso alto, en el centro no hace nada.
  - **LP, HP, BP, NOTCH:** paso bajo, paso alto, paso banda y notch (quita una banda); FILTRO es el corte.
  - **MORPH:** el cuarto knob (FORMA) pasa de paso bajo a banda a paso alto.
  - DOBLE (en DJ, LP, HP, BP y NOTCH) añade un segundo pico por encima del corte, como un filtro doble.
  - HOME + ALGORITHM mueve FILTRO desde cualquier vista. También es un destino de los LFO.
- **MASTER:** COMP (compresor de bus: más es más pegado y más bombeo; arriba se ve cuántos dB está bajando) y
  SATURA (cinta: calor y redondez). Al final hay siempre un limitador suave.

- **MAREA** (páginas MAREA, MAREA 2 y MAREA 3): ver el apartado siguiente.
- **DIGITAL** (al final del master: después del compresor y la cinta, antes del limitador):
  - **BITS:** NO, o de 16 a 4 bits (reducción de resolución: grano, luego ruido áspero).
  - **MUESTREO:** NO, o de /1 a /32 (baja la frecuencia de muestreo: aliasing metálico).
  - El STUTTER se quitó el 12 de noviembre para liberar memoria para la reverb y para Plaits. Para algo parecido:
    ECO con TIEMPO 1 o 2, mantener REC (throw) y HOME + REC (freeze).
- En la parte baja se ven la página anterior, la actual y la siguiente, y el número de página (1/11 … 11/11).

Al cambiar el TIEMPO en TAPE el tono se desliza, como una cinta que cambia de velocidad. Al cambiar de TIPO el
eco empieza vacío. El tiempo máximo es 1,5 s (0,75 s en PINGPONG); si no cabe, se divide por dos.

Los efectos son del proyecto: no cambian al cambiar de patrón.

### MAREA: banco de filtros con olas
Un efecto que se pone **en la pista que elijas**, pensado para acordes, pads y diseño sonoro. Son 16 filtros de
banda (de 40 Hz a 15 kHz, en puntos fijos) cuyo volumen mueve una ola. Inspirado en el Slink de Hypnus Records: los
controles hacen lo que su manual describe, pero el algoritmo es propio y no suena igual.
- **MAREA:** PISTA (en cuál va; hay una sola MAREA, si la pones en otra pista se mueve), MEZCLA (seco a mojado),
  VELOC y MULTI.
  - **VELOC** tiene el cero en el centro: a la derecha la ola avanza (sube hacia los agudos), a la izquierda vuelve, y
    en el centro se queda **quieta** con su forma. **HOME + VELOC** cambia entre LIBRE (Hz) y TEMPO (de 32 compases a
    1/4 de compás por ciclo).
  - **MULTI:** cuántos ciclos de ola caben a lo ancho.
- **MAREA 2:** RIZO (ondas pequeñas encima de la ola), DESFASE (coloca la ola a mano; con un LFO, la barre),
  INVIERTE (profundidad: 100 entera, 0 nada, −100 al revés: las crestas pasan a valles), GRAVEDAD (tira de la ola
  hacia abajo: valles anchos y crestas finas; arriba del todo, formas extremas).
- **MAREA 3:** DESDE y HASTA (la zona del espectro donde actúa la ola; fuera de ella las bandas suenan a su nivel, sin
  moverse), RESO (bandas anchas o finas) y ANCHO (las bandas se reparten a izquierda y derecha; 0 = mono).
- La pantalla dibuja la ola sobre las frecuencias, con los bordes de la zona punteados. Arriba a la izquierda se ve
  la velocidad (> avanza, < vuelve; en amarillo si va al tempo).
- Sale en estéreo: el PAN de esa pista funciona como balance. Los envíos A y B reciben ya el sonido con MAREA.
- Destinos de modulación: MAR VELOC, MAR MULTI, MAR RIZO, MAR DESFASE, MAR GRAVEDAD y MAR MEZCLA.
- De inicio está en la pista 3, toda mojada (MEZCLA 100), avanzando a unos 3 s por vuelta y con ANCHO a tope.

### FLOW: la cadena de efectos (HOME hasta FLOW)
La pantalla dibuja la cadena tal como corre: las pistas, MAREA en su pista, los envíos, el eco y la reverb, y el
master (filtro, compresor, cinta y limitador). Cada bloque tiene un número.
- **Teclas blancas 1-6:** activan o quitan un bloque: 1 MAREA, 2 ECO, 3 REVERB, 4 FILTRO, 5 COMP, 6 CINTA
  (saturación). Las teclas encendidas son los bloques activos. Al quitar uno, se funde en ~10 ms (sin clic) y deja
  de calcularse: en el FM-1 eso libera CPU (*pendiente en el dispositivo*). Al volver, el eco o la reverb empiezan
  vacíos.
- **K1 FILTRO EN:** dónde va el filtro del master: TODO (como hasta ahora), SECO (solo las pistas: los ecos y la
  reverb siguen abiertos mientras cierras, el gesto clásico de dub) o EFECTOS (solo los retornos).
- **K2 ESPACIO:** ECO>REV (el eco alimenta la reverb, como hasta ahora) o REV>ECO (la reverb alimenta el eco: ecos
  de una nube).
- **K3 DRIVE:** COMP>SAT o SAT>COMP. La cinta antes del compresor suena más contenida; después, más sucia.
- **K4 CRUCE:** cuánto del primero del ESPACIO entra en el segundo. Es el mismo knob que A REVERB en la página
  ECO 2; en FLOW vale para los dos sentidos.
- El limitador del final no se puede quitar. FLOW es del proyecto, como los efectos.
- No hay orden libre: solo estas opciones fijas, para que no haya combinaciones que rompan el sonido.

### Gestos de dub (REC)
- **Mantener REC:** throw. La pista elegida va al eco a tope mientras lo mantienes, aunque su envío esté a 0.
  Sin soltar REC, pulsa otras pistas para lanzarlas también. Al soltar, todo vuelve a su envío.
- **HOME + REC:** freeze. El eco deja de escuchar y repite lo que tiene sin apagarse; otra vez HOME + REC lo
  suelta. Con freeze puesto el LED de REC queda encendido y la vista EFECTOS muestra FREEZE.
- El bucle congelado se oscurece muy despacio tras muchas vueltas (efecto de la interpolación).

## 10. Modulación (vista MOD)
Cuatro fuentes del proyecto: **LFO 1, LFO 2, SEGUIDOR 1 y SEGUIDOR 2**. SELECT elige la fuente; la pantalla
muestra las cuatro moviéndose y a dónde van.
- **LFO:** FORMA (seno, triángulo, sierra, cuadrada, S&H, azar suave) y VELOC (de 1/16 a 32 compases). Con el
  secuenciador en marcha va enganchado al compás: un LFO de 1 compás empieza y acaba con el compás.
- **SEGUIDOR:** SIGUE (la pista que escucha) y CAÍDA (lo que tarda en bajar). Sube con cada golpe de esa pista.
- **DESTINO y CANT** (K3 y K4): a dónde va y cuánto, en positivo o en negativo. Cada fuente tiene **dos destinos**:
  mantén HOME y gira K3 y K4 para el segundo.
- Destinos: FILTRO, RESO, ECO TIEMPO, ECO REPITE, ECO TONO, ECO NIVEL, REV CAÍDA, REV NIVEL, los de MAREA, y el TONO
  o el NIVEL de cada pista. La modulación se suma a lo que marque el knob sin moverlo.
- De inicio, el LFO 1 mueve el tono del eco (+25 %, 8 compases).

## 11. Escenas (HOME + SEQ)
Una escena es una foto de varios ajustes (hasta 8) a la que entras y de la que sales con una tecla, con un fundido.
Hay 8 escenas en el proyecto.
- **HOME + SEQ:** entra o sale del modo ESCENAS. En este modo las **teclas negras 1-8** son las escenas (y no tocan
  notas). Arriba se ve "ESC" y las escenas que están sonando.
- **Mantener una tecla negra:** la escena entra mientras la mantienes; al soltar, sale con el mismo fundido.
- **HOME + tecla negra:** la deja puesta (o la quita). Una escena puesta tiene su tecla encendida.
- **Mientras mantienes una escena:**
  - La pantalla muestra su nombre, una barra de cuánto ha entrado, el fundido y lo que fija.
  - **Gira un knob** para grabar su valor en la escena: en EFECTOS, el parámetro de esa página; en FLOW, sus ajustes;
    en las vistas de pista, el parámetro de la página actual de la pista elegida (P1-P6 delante del nombre).
  - **SELECT:** el fundido: AL MOMENTO, 1/16, 1/8, 1/4, 1/2, 1, 2 o 4 compases. Va al tempo.
  - **ARP:** vacía la escena.
- Los ajustes continuos se deslizan durante el fundido. Los de pasos (un tipo, un orden, SÍ/NO) cambian a mitad del
  fundido. Si dos escenas tocan lo mismo, manda la de número más alto.
- Las escenas no mueven los knobs: al salir, todo vuelve a donde estaba.
- **De fábrica:** 1 SUMERGIR (filtro cerrado y eco largo, 1 compás), 2 CATEDRAL (todo reverb, 2 compases), 3 CINTA
  ROTA (cinta y desgaste), 4 DIGITAL (bits y muestreo), 5 TÚNEL (fino y lejos, 1/2 compás), 6 SECO (sin eco ni
  reverb), 7 SHIMMER (placa con shimmer, larga, 2 compases), 8 ECO INFINITO (el eco a 97 % y ninguna pista le envía: lo que hay se queda).
- **Aún no se guardan** al apagar: vuelven a las de fábrica (guardado del proyecto, fase 6).

## 12. Pendiente
Siguiente: guardado del proyecto (mezcla, efectos, escenas, tempo), nombres, borrar un patrón guardado, reloj y MIDI
(fase 6). Después: más motores, bancos de demostración por estilo, prueba en el aparato (7, solo cuando se decida).
- FILL ya no tiene atajo: HOME + PLAY ahora congela la EVOLUCIÓN.
