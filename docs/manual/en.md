# EPICYKLOS · User manual (draft)

> Status: **Phase 3a, browser emulator only.** None of this has been tested on a real FM-1 yet.
> The manual grows with each phase and only describes what already works. Español: [es.md](es.md).
> On-screen text is in Spanish for now; the Spanish label is given in brackets.

## 1. What it is
Open firmware (GPL-3.0) for the M-VAVE FM-1: six tracks with a euclidean, polymetric sequencer, made for dub
techno, hypnotic techno, ambient and drone.

## 2. The emulator
- **Click the screen or PLAY** to enable sound (browsers require it).
- **Hold a button and turn a knob** (e.g. HOME + SELECT): **Shift + click** (or right-click) latches the button,
  shown with a blue edge; turn the knob; click normally to release.
- Knobs turn by dragging or with the mouse wheel.

## 3. Panel map
| Control | Alone | With HOME held |
|---|---|---|
| FX SCL ENV LFO EDIT GLO | select track 1-6 | mute the track |
| (in the MIX view) | mute the track | solo the track |
| HOME | tap: ORBIT → MIX → EFFECTS [ÓRBITA / MEZCLA / EFECTOS] | the shift key |
| PLAY | start / stop | — |
| SELECT | change page | **tempo** (1 BPM per click) |
| PRESETS | track level | the track's send to the delay |
| K1-K4 | the page's 4 parameters | fine adjust |
| OCT- / OCT+ | keyboard octave | — |
| SAVE | PATTERNS view [PATRONES] | — |
| SEQ | STEPS view [PASOS] | — |
| ARP | random | **undo** |
| REC | hold: **throw** the track to the delay | **freeze** the delay (on / off) |
| ALGORITHM | a later phase | — |

While HOME is held, the lower part of the screen shows this cheat sheet.

## 4. Tracks and engines
Each track has a sound engine (ENGINE page [MOTOR], K1). So far:
- **GRAVE** (track 1): kick. Note, drive (clean to industrial), click, decay, sweep [BARRIDO], rumble.
- **METAL** (track 2): 808/909-style hats. Tune [AFINA], metal (noise to metallic), brightness [BRILLO], decay,
  ring [ANILLO], variation [VARÍA].
- **GOLPE** (track 6): clap, snare or rim. Tune [AFINA], type [TIPO: PALMA → CAJA → ARO], brightness, decay,
  body [CUERPO], tail [COLA].
- **TAMBOR** (chosen in ENGINE; the default on track 6 in some patterns): toms, congas and metallic
  percussion. Tune, timbre (round tom to metallic), FM (how bright the attack is), decay, sweep, click.
- **PRUEBA** (tracks 3-5): a placeholder voice until the engines of phases 4 and 5 arrive. It has attack
  [ATAQUE] (ENGINE page, K4): the sound comes in at once or slowly.

## 5. Pages (SELECT)
SOUND [SONIDO] · ENGINE [MOTOR] · RHYTHM [RITMO] (length, hits, rotate, division) · GROOVE (swing, shift
[DESPLAZA]) · MIX [MEZCLA] (level, pan, A delay [A ECO], B reverb).

## 6. Patterns and banks (SAVE)
- **Black keys = banks A to K. White keys = the bank's 16 patterns.**
- Stopped, the change is immediate; playing, it waits for the end of the bar (the queued pattern blinks).
- Holding SAVE + a key does the same from any view.
- A pattern stores each track's sequence and sound. Mix, tempo, **mute and solo belong to the project**: they
  do not change with the pattern.
- Bank A: 16 demo patterns. Banks B-K: generated patterns.
- **Current limitation:** nothing is saved at power-off yet, and edits in a bank are lost when you change bank.
  Saving arrives in phase 6.

## 7. Steps (SEQ)
- The 16 white keys are the steps; OCT-/OCT+ switches between 1-16 and 17-32.
- Tap: step on or off (the track becomes hand-made, "A MANO").
- **Hold a step + K1:** probability (100, 75, 50, 25 %). **+ K2:** condition (1:2, 2:2, 1:4…: it plays only on
  that turn of the cycle).
- **Hold a step + K3:** micro-timing, moves that hit earlier (−) or later (+) in eighths of a step, up to 3/8.
- Turning hits [GOLPES] on a hand-made track returns it to the euclidean spread.

### Early and late (GROOVE page)
- **SWING:** delays the track's odd steps (up to 0.4 of a step).
- **Shift [DESPLAZA]:** moves the whole track earlier or later (±45 % of a step). For example, the clap a
  little early or the hats a little late.
- Swing, shift and micro-timing add up, limited to 0.9 of a step.

## 8. Random (ARP)
- **ARP:** changes the parameters of the page shown, on the selected track. It never touches engine, length,
  division or level.
- **ARP in the PATTERNS view:** a whole new pattern (sounds and sequences of all six tracks).
- **HOME + ARP:** undoes the last random (one level).
- **ARP held + ALGORITHM:** amount (10-100 %). **ARP held + a knob:** locks that parameter.

## 9. Send effects (EFFECTS view [EFECTOS])
Each track sends part of its sound to the **delay (A)** and the **reverb (B)** from the MIX page (K3 and K4).
The send is taken after the level: a muted track stops sending, but the tail rings on.

In the EFFECTS view, SELECT changes page and K1-K4 are the controls:
- **Delay [ECO]:** type [TIPO: TAPE, BBD, PINGPONG], time [TIEMPO] in sixteenths, 1 to 16 (try 3, 5 or 7),
  feedback [REPITE] (at the top the delay feeds itself), tone [TONO] (each repeat darker).
- **Delay 2 [ECO 2]:** wear [DESGASTE] (worn tape in TAPE, a dirty clock in BBD), to reverb [A REVERB], level,
  DUCK (track 1's kick pushes the delay and the reverb down while it sounds; they come back in ~150 ms).
- **Reverb (PLACA, a plate):** decay [CAÍDA], tone, movement [MOVER], level.

Changing the time in TAPE bends the pitch, like a tape changing speed. Changing the type starts the delay
empty. The longest time is 1.5 s (0.75 s in PINGPONG); a time that does not fit is halved.

The effects belong to the project: they do not change with the pattern.

### Dub gestures (REC)
- **Hold REC:** throw. The selected track goes to the delay at full while you hold it, even with its send at 0.
  Keep REC held and press other tracks to throw them too. On release, everything goes back to its send.
- **HOME + REC:** freeze. The delay stops listening and repeats what it holds without fading; HOME + REC again
  lets it go. While frozen the REC light stays on and the EFFECTS view shows FREEZE.
- A frozen loop darkens very slowly over many turns (a side effect of the interpolation).

## 10. To come
Master filter, compressor, modulation and scenes (phase 3b), harmony and generative tools (4), more engines (5), saving, names, clock and
MIDI (6), testing on the device (7, only when decided).
