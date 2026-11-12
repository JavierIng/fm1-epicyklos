# EPICYKLOS · User manual (draft)

> Status: **Phase 3b, browser emulator only.** None of this has been tested on a real FM-1 yet.
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
| HOME | tap: ORBIT → MIX → EFFECTS → MOD [ÓRBITA / MEZCLA / EFECTOS / MOD] | the shift key |
| PLAY | start / stop | — |
| SELECT | change page | **tempo** (1 BPM per click) |
| PRESETS | track level | the track's send to the delay |
| K1-K4 | the page's 4 parameters | fine adjust |
| OCT- / OCT+ | keyboard octave | — |
| SAVE | PATTERNS view [PATRONES] | — |
| SEQ | STEPS view [PASOS] | — |
| ARP | random | **undo** |
| REC | hold: **throw** the track to the delay | **freeze** the delay (on / off) |
| ALGORITHM | a later phase | **master filter**, from any view |

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
- **Filter [FILTRO]** (the master's, after the returns): type [TIPO], filter, resonance [RESO] and a fourth knob
  that changes with the type.
  - **DJ:** left closes a low-pass, right opens a high-pass, the middle does nothing.
  - **LP, HP, BP, NOTCH:** low-pass, high-pass, band-pass and notch (takes a band out); the filter knob is the
    cutoff [CORTE].
  - **MORPH:** the fourth knob [FORMA] goes from low-pass to band-pass to high-pass.
  - Double [DOBLE] (in DJ, LP, HP, BP and NOTCH) adds a second peak above the cutoff, like a dual filter.
  - HOME + ALGORITHM moves the filter from any view. It is also an LFO destination.
- **Master:** COMP (bus compressor: more is more glue and more pumping; the screen shows how many dB it takes
  off) and saturation [SATURA] (tape: warmth and roundness). A soft limiter always sits at the end.

- **MAREA** (pages MAREA, MAREA 2 and MAREA 3): see the next section.
- The bottom row shows the page before, this one and the next, and the page number (1/8 … 8/8).

Changing the time in TAPE bends the pitch, like a tape changing speed. Changing the type starts the delay
empty. The longest time is 1.5 s (0.75 s in PINGPONG); a time that does not fit is halved.

The effects belong to the project: they do not change with the pattern.

### MAREA: a filterbank moved by waves
An effect you put **on the track you choose**, meant for chords, pads and sound design. Sixteen band-pass
filters whose levels are moved by several crossing waves, like water. Inspired by Hypnus Records' Slink (the idea
only; not the same algorithm).
- **MAREA:** track [PISTA] (which one it is on; there is one MAREA, putting it on another track moves it), mix
  [MEZCLA] (dry to wet), speed [VELOC] (0 = the water stands still), shape [FORMA] (one smooth swell to choppy).
- **MAREA 2:** length [LONGITUD] (how many waves fit across the bands), depth [PROFUND] (how far the troughs
  sink), stereo [ESTÉREO] (the right side reads the water further along: the sound opens and moves between the
  sides), range [RANGO] (lower or higher bands).
- **MAREA 3:** RESO (wide or narrow bands; narrow sounds more like resonances).
- The screen draws the 16 bands: left on top, right below.
- It comes out in stereo: that track's PAN works as a balance. Sends A and B already get the sound with MAREA.
- Modulation destinations: MAR VELOC, MAR FORMA, MAR ESTÉREO, MAR RANGO and MAR MEZCLA (a slow LFO on range, or a
  follower on shape, makes it breathe).
- By default it sits on track 3 with mix 75.

### Dub gestures (REC)
- **Hold REC:** throw. The selected track goes to the delay at full while you hold it, even with its send at 0.
  Keep REC held and press other tracks to throw them too. On release, everything goes back to its send.
- **HOME + REC:** freeze. The delay stops listening and repeats what it holds without fading; HOME + REC again
  lets it go. While frozen the REC light stays on and the EFFECTS view shows FREEZE.
- A frozen loop darkens very slowly over many turns (a side effect of the interpolation).

## 10. Modulation (MOD view)
Four sources, the project's: **LFO 1, LFO 2, FOLLOWER 1 and FOLLOWER 2 [SEGUIDOR]**. SELECT picks the source;
the screen shows all four moving and where they go.
- **LFO:** shape [FORMA: sine, triangle, saw, square, S&H, smooth random] and rate [VELOC] (1/16 to 32 bars).
  While the sequencer runs it is locked to the bar: a 1-bar LFO starts and ends with the bar.
- **Follower:** the track it listens to [SIGUE] and its release [CAÍDA]. It rises with each hit of that track.
- **Destination and amount [DESTINO, CANT]** (K3, K4): where it goes and how much, positive or negative. Each
  source has **two destinations**: hold HOME and turn K3 and K4 for the second.
- Destinations: filter, resonance, delay time, feedback, tone and level, reverb decay and level, MAREA's, and
  each track's TONE or LEVEL. Modulation adds to the knob's setting without moving it.
- By default LFO 1 moves the delay's tone (+25 %, 8 bars).

## 11. To come
Scenes (phase 3), harmony and generative tools (4), more engines (5), saving, names, clock and
MIDI (6), testing on the device (7, only when decided).
