# EPICYKLOS · Manual de usuario (borrador)

> Estado: **Fase 2c, solo emulador de navegador.** Nada de esto se ha probado todavía en un FM-1 real.
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
| HOME | toque: ÓRBITA ↔ MEZCLA | es la tecla de función |
| PLAY | arrancar / parar | — |
| SELECT | cambiar de página | **tempo** (1 BPM por clic) |
| PRESETS | nivel de la pista | — |
| K1-K4 | los 4 parámetros de la página | ajuste fino |
| OCT- / OCT+ | octava del teclado | — |
| SAVE | vista PATRONES | — |
| SEQ | vista PASOS | — |
| ARP | aleatorio | **deshacer** |
| ALGORITHM, REC | fases siguientes | — |

Al mantener HOME, la parte baja de la pantalla muestra esta chuleta.

## 4. Pistas y motores
Cada pista tiene un motor de sonido (página MOTOR, K1). Por ahora:
- **GRAVE** (pista 1): bombo. NOTA, DRIVE (de limpio a industrial), CLICK, CAÍDA, BARRIDO, RUMBLE (cola grave).
- **METAL** (pista 2): platos tipo 808/909. AFINA, METAL (de ruido a metálico), BRILLO, CAÍDA, ANILLO, VARÍA.
- **GOLPE** (pista 6): palma, caja o aro. AFINA, TIPO (PALMA → CAJA → ARO), BRILLO, CAÍDA, CUERPO, COLA.
- **PRUEBA** (pistas 3-5): voz provisional hasta que lleguen los motores de las fases 4 y 5. Tiene ATAQUE
  (página MOTOR, K4): el sonido entra de golpe o poco a poco.

## 5. Páginas (SELECT)
SONIDO · MOTOR · RITMO (LONGITUD, GOLPES, ROTAR, DIVISIÓN) · GROOVE (SWING, DESPLAZA) · MEZCLA (NIVEL, PAN; los envíos llegan en la fase 3).

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

## 9. Pendiente
Efectos y escenas (fase 3), armonía y generativos (4), más motores (5), guardado, nombres, reloj y MIDI (6),
prueba en el aparato (7, solo cuando se decida).
