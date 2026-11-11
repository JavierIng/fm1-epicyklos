# EPICYKLOS · User manual (draft)

> Status: **Phase 2b, browser emulator only.** None of this has been tested on a real FM-1 yet.
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
| HOME | tap: ORBIT ↔ MIX [ÓRBITA / MEZCLA] | the shift key |
| PLAY | start / stop | — |
| SELECT | change page | **tempo** (1 BPM per click) |
| PRESETS | track level | — |
| K1-K4 | the page's 4 parameters | fine adjust |
| OCT- / OCT+ | keyboard octave | — |
| SAVE | PATTERNS view [PATRONES] | — |
| SEQ | STEPS view [PASOS] | — |
| ARP | random | **undo** |
| ALGORITHM, REC | later phases | — |

While HOME is held, the lower part of the screen shows this cheat sheet.

## 4. Tracks and engines
Each track has a sound engine (ENGINE page [MOTOR], K1). So far:
- **GRAVE** (track 1): kick. Note, drive (clean to industrial), click, decay, sweep [BARRIDO], rumble.
- **METAL** (track 2): 808/909-style hats. Tune [AFINA], metal (noise to metallic), brightness [BRILLO], decay,
  ring [ANILLO], variation [VARÍA].
- **PRUEBA** (tracks 3-6): a placeholder voice until the engines of phases 4 and 5 arrive.

## 5. Pages (SELECT)
SOUND [SONIDO] · ENGINE [MOTOR] · RHYTHM [RITMO] (length, hits, rotate, division) · MIX [MEZCLA] (level, pan;
sends arrive in phase 3).

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
- Turning hits [GOLPES] on a hand-made track returns it to the euclidean spread.

## 8. Random (ARP)
- **ARP:** changes the parameters of the page shown, on the selected track. It never touches engine, length,
  division or level.
- **ARP in the PATTERNS view:** a whole new pattern (sounds and sequences of all six tracks).
- **HOME + ARP:** undoes the last random (one level).
- **ARP held + ALGORITHM:** amount (10-100 %). **ARP held + a knob:** locks that parameter.

## 9. To come
Effects and scenes (phase 3), harmony and generative tools (4), more engines (5), saving, names, clock and
MIDI (6), testing on the device (7, only when decided).
