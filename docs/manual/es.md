# EPICYKLOS · Manual de usuario (borrador)

> Estado: **Fase 3b, solo emulador de navegador.** Nada de esto se ha probado todavía en un FM-1 real.
> Este manual crece con cada fase y solo describe lo que ya funciona. English: [en.md](en.md).

## 1. Qué es
Firmware abierto (GPL-3.0) para el M-VAVE FM-1: seis pistas con secuenciador euclidiano y polimétrico, pensado
para dub techno, techno hipnótico, ambient y drone.

## 2. El emulador
- **Clic en la pantalla o en PLAY** para activar el sonido (el navegador lo exige).
- **Mantener un botón y girar un knob** (por ejemplo HOME + SELECT): **Mayús + clic** (o clic derecho) deja el
  botón pulsado, con borde azul; gira el knob; clic normal para soltarlo.
- Los knobs giran arrastrando o con la rueda del ratón.

## 3. Mapa del panel
| Control | Solo | Con HOME mantenido |
|---|---|---|
| FX SCL ENV LFO EDIT GLO | elegir pista 1-6 | mute de la pista |
| (en vista MEZCLA) | mute de la pista | solo de la pista |
| HOME | toque: ÓRBITA → MEZCLA → EFECTOS → MOD | es la tecla de función |
| PLAY | arrancar / parar | — |
| SELECT | cambiar de página | **tempo** (1 BPM por clic) |
| PRESETS | nivel de la pista | envío de la pista al eco |
| K1-K4 | los 4 parámetros de la página | ajuste fino |
| OCT- / OCT+ | octava del teclado | — |
| SAVE | vista PATRONES | — |
| SEQ | vista PASOS | — |
| ARP | aleatorio | **deshacer** |
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
- **PRUEBA** (pistas 3-5): voz provisional hasta que lleguen los motores de las fases 4 y 5. Tiene ATAQUE
  (página MOTOR, K4): el sonido entra de golpe o poco a poco.

## 5. Páginas (SELECT)
SONIDO · MOTOR · RITMO (LONGITUD, GOLPES, ROTAR, DIVISIÓN) · GROOVE (SWING, DESPLAZA) · MEZCLA (NIVEL, PAN, A ECO, B REVERB).

## 6. Patrones y bancos (SAVE)
- **Teclas negras = bancos A a K. Teclas blancas = los 16 patrones del banco.**
- Con el secuenciador parado el cambio es inmediato; sonando, espera al final del compás (parpadea "en cola").
- Mantener SAVE + tecla hace lo mismo desde cualquier vista.
- Un patrón guarda la secuencia y el sonido de cada pista. Mezcla, tempo, **mute y solo son del proyecto**:
  no cambian al cambiar de patrón.
- Banco A: 16 patrones de muestra. Bancos B-K: patrones generados.
- **Limitación actual:** todavía no se guarda nada al apagar, y lo que edites en un banco se pierde al cambiar
  de banco. El guardado llega en la fase 6.

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
- **ARP en la vista PATRONES:** patrón nuevo entero (sonidos y secuencias de las seis pistas).
- **HOME + ARP:** deshace el último aleatorio (un solo nivel).
- **ARP mantenido + ALGORITHM:** cantidad (10-100 %). **ARP mantenido + un knob:** bloquea ese parámetro.

## 9. Efectos de envío (vista EFECTOS)
Cada pista manda parte de su sonido al **eco (A)** y a la **reverb (B)** desde la página MEZCLA (K3 y K4). El
envío sale después del nivel: si silencias una pista deja de enviar, pero la cola sigue sonando.

En la vista EFECTOS, SELECT cambia de página y K1-K4 son los controles:
- **ECO:** TIPO (TAPE, BBD, PINGPONG), TIEMPO (en semicorcheas, de 1 a 16: prueba 3, 5 o 7), REPITE (arriba del
  todo el eco se realimenta solo), TONO (cada repetición más oscura).
- **ECO 2:** DESGASTE (cinta gastada en TAPE, reloj sucio en BBD), A REVERB (el eco entra en la reverb), NIVEL,
  DUCK (el bombo de la pista 1 baja el eco y la reverb mientras suena; vuelven en ~150 ms).
- **REVERB (PLACA):** CAÍDA, TONO, MOVER (movimiento interno), NIVEL.
- **FILTRO** (del master, después de los retornos): TIPO, FILTRO, RESO y un cuarto knob que cambia con el tipo.
  - **DJ:** a la izquierda cierra un paso bajo, a la derecha abre un paso alto, en el centro no hace nada.
  - **LP, HP, BP, NOTCH:** paso bajo, paso alto, paso banda y notch (quita una banda); FILTRO es el corte.
  - **MORPH:** el cuarto knob (FORMA) pasa de paso bajo a banda a paso alto.
  - DOBLE (en DJ, LP, HP, BP y NOTCH) añade un segundo pico por encima del corte, como un filtro doble.
  - HOME + ALGORITHM mueve FILTRO desde cualquier vista. También es un destino de los LFO.
- **MASTER:** COMP (compresor de bus: más es más pegado y más bombeo; arriba se ve cuántos dB está bajando) y
  SATURA (cinta: calor y redondez). Al final hay siempre un limitador suave.

- **MAREA** (páginas MAREA, MAREA 2 y MAREA 3): ver el apartado siguiente.
- En la parte baja se ven la página anterior, la actual y la siguiente, y el número de página (1/8 … 8/8).

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
- De inicio está en la pista 3 con MEZCLA 75.

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

## 11. Pendiente
Escenas (fase 3), armonía y generativos (4), más motores (5), guardado, nombres, reloj y MIDI (6),
prueba en el aparato (7, solo cuando se decida).
