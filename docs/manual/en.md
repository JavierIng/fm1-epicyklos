# EPICYKLOS · User manual (draft)

> Status: **Phase 4b, browser emulator only.** None of this has been tested on a real FM-1 yet.
> The manual grows with each phase and only describes what already works. Español: [es.md](es.md).
> On-screen text is in Spanish for now; the Spanish label is given in brackets.

## 1. What it is
Open firmware (GPL-3.0) for the M-VAVE FM-1: six tracks with a euclidean, polymetric sequencer, made for dub
techno, hypnotic techno, ambient and drone.

## 2. At power-on
A spiral galaxy turns for about 3 seconds with EPICYKLOS pulsing in the middle. Touching any button, key or knob
skips it.

## 2b. The emulator
- **Click the screen or PLAY** to enable sound (browsers require it).
- **Hold a button and turn a knob** (e.g. HOME + SELECT): **Shift + click** (or right-click) latches the button,
  shown with a blue edge; turn the knob; click normally to release.
- Knobs turn by dragging or with the mouse wheel.

## 3. Panel map
| Control | Alone | With HOME held |
|---|---|---|
| FX SCL ENV LFO EDIT GLO | select track 1-6 | mute the track |
| (in the MIX view) | mute the track | solo the track |
| HOME | tap: ORBIT → MIX → EFFECTS → FLOW → MOD [ÓRBITA / MEZCLA / EFECTOS / FLOW / MOD] | the shift key |
| PLAY | start / stop | **freeze the evolution** of every track |
| SELECT | change page | **tempo** (1 BPM per click) |
| PRESETS | track level | the track's send to the delay |
| K1-K4 | the page's 4 parameters | fine adjust |
| OCT- / OCT+ | keyboard octave | — |
| SAVE | PATTERNS view [PATRONES] | in PATTERNS, HOME + white key: **save** |
| SEQ | STEPS view [PASOS] | **SCENES** mode [ESCENAS] (black keys) |
| ARP | random; held + track: evolution on / off | **undo** |
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
- **ACORDE DUB** (track 3): the dub techno chord. Four voices, each two slightly detuned saws, through a resonant
  low-pass with its own envelope.
  - Note, chord [ACORDE] (which chord of the table; the screen shows its name: MIN7, MIN9, SUS4…), tone (where the
    filter closes) and decay.
  - On the ENGINE page: table [TABLA] (Original, Colour Chords, Disastrous Peace or Tonnetz Walk, the Plaits Palette
    tables), envelope [ENVOLV] (how far the filter opens on a hit) and attack.
  - In Tonnetz Walk the names (C, AM, F…) are relative: with the note on D, "C" sounds as D major.
- **DRONE** (track 5): it **sustains**. Each hit opens it or keeps it going; a new note glides to it.
  - Note, timbre [TIMBRE] (sine → triangle → saw → saw with breath), tone (filter) and sustain [SOSTÉN] (how long
    it holds after the last hit: 0.2 to 30 s; then it fades over about 2 s).
  - On the ENGINE page: drift [DERIVA] (a slow wander of tuning, spread and filter: what keeps it alive), interval
    [INTERVALO] (the second voice: unison, minor 3rd, fourth, fifth, octave, fifth + octave) and attack (up to 8 s).
  - When the sequencer stops, the drone fades by itself.
- **MACRO** (track 4, the bass; also good for melodies): six ways of making a sound behind the same knobs. The
  idea is that of Mutable Instruments Plaits; the code is EPICYKLOS's own, not a port.
  - On the ENGINE page: model [MODELO], harmonics [ARMON] (the model's second knob) and attack [ATAQUE].
  - On the SOUND page: note, timbre [TIMBRE] (the model's first knob), tone [TONO] and decay [CAÍDA].
  - **TONO** is a filter that the note itself opens and closes: fully up it is open; lower, each note starts open
    and closes as it fades (the "pluck" of a dub bass).
  - The models:
    - **ANALOG**: two oscillators. TIMBRE goes from saw to square to a narrow pulse. ARMON detunes the second
      one and, past half-way, puts it an octave down, a fifth, an octave or a fifth + octave up.
    - **PLIEGUE** (fold): a sine folded over itself. TIMBRE: how many folds. ARMON: asymmetry.
    - **FM**: two operators. ARMON: the ratio (0.5 · 1 · 1.5 · 2 · 3 · 3.5 · 4 · 5 · 7). TIMBRE: how much FM (it
      falls by itself as the note fades).
    - **GRANO** (grain): two formants, a little like a voice. TIMBRE: the formant. ARMON: where the second one sits.
    - **CUERDA** (string): a plucked string. TIMBRE: brightness. ARMON: how hard the pick is. Decay is its ring.
    - **ARMONICO** (harmonic): eight sine harmonics. ARMON: which ones sound. TIMBRE: how many at once.
  - Each model has two knobs (Plaits has three), and the rest of Plaits's models are missing (chords live in
    ACORDE; there is no speech, wavetable, noise or percussion model).
- **PRUEBA**: the placeholder voice that used to play the bass. It stays in the engine list.

## 5. Pages (SELECT)
SOUND [SONIDO] · ENGINE [MOTOR] · RHYTHM [RITMO] (length, hits, rotate, division) · GROOVE (swing, shift
[DESPLAZA], absence [AUSENCIA]) · EVOLUTION [EVOLUCION] (mutate [MUTAR], what [QUE], every [CADA], return [VUELTA]) ·
MOVEMENT [MOVIMIENTO] (shape [FORMA], rate [VELOC], destination [DESTINO], amount [CANT]) · MIX [MEZCLA]
(level, pan, A delay [A ECO], B reverb). They do not all fit, so the top line shows the previous, current and next
page, with the page number.

### EVOLUTION: the pattern changes by itself
**It is per track:** the knobs show and change the selected track; each of the six has its own, saved with the
pattern. Each track can drift little by little without you touching anything. The written pattern is not modified:
an offset over it changes, and a pattern change or PLAY starts again from what is written.
- **MUTATE [MUTAR]:** how much it changes each time. What changes: hits (one more or one fewer), rotation, tone,
  decay, character (in ACORDE, the chord) and, in ACORDE, DRONE and TEST, the note (a fourth, fifth or octave). It
  never leaves the track without hits: that is ABSENCE's job.
- **What [QUE]:** what may change: all [TODO], rhythm [RITMO] (hits and rotation), sound [SONIDO] (tone, decay,
  character/chord) or note [NOTA]. For example, hats on SOUND change colour without moving the groove; ACORDE on
  SOUND changes chord in place.
- **EVERY [CADA]:** every how many bars it changes: 1, 2, 4, 8 or 16. The first stretch always plays as written.
- **RETURN [VUELTA]:** how much it goes back towards the written pattern each time. High: it strays a little and
  comes back. Low: it wanders far.
- **ABSENCE [AUSENCIA]** (on the GROOVE page, since it decides whether the track plays): the chance that it rests
  for each stretch of EVERY bars. In dub, what you take out counts as much as what plays.
- **Freeze:** MUTATE 0 and RETURN 0 leave the track as it is at that moment. If you like it, save it (section 6).
- **ARP held + a track button:** turns that track's evolution off or on. Off, it goes back to what is written at
  the next bar and stays there; its knobs are kept for when you turn it on.
- **HOME + PLAY:** freezes every track's evolution as it is now (shows CONGELADA); again, releases it.
- On the EVOLUTION page, top right, six squares: bright = the track evolves; dim = MUTATE and ABSENCE at 0; grey =
  turned off with ARP. The line marks the selected track.
- In bank A, by default, **only the synths evolve** (chord and bass every 4 bars, drone every 8). Kick, hats and
  percussion come still. Generated banks do the same, in different amounts.
- ARP on these pages randomizes MUTATE, EVERY, RETURN and ABSENCE (ABSENCE at most 25 %); it leaves WHAT alone.

### MOVEMENT: each track's LFO
Each track has its own LFO, saved with the pattern. While the sequencer runs it is locked to the bar.
- **Shape [FORMA]:** sine, triangle, saw, square, S&H (a new value each cycle), smooth random [AZAR] and **DRIFT
  [DERIVA]**: a slow random walk with no cycle that never repeats. It is what keeps a fixed sound alive.
- **Rate [VELOC]:** how long a cycle lasts, 1/16 to 32 bars. In DRIFT, how far it wanders in that time.
- **Destination [DESTINO]:** tone, decay, character, the engine's two knobs (by their name: ENVOLV, DERIVA,
  CUERPO…), level, pan, send A, send B, **hits [GOLPES]** or **rotate [ROTAR]**. With hits or rotate the rhythm
  breathes: the track gains and loses hits, or shifts them, across the cycle.
- **Amount [CANT]:** −100 to +100. At +100 it travels half the knob up and down; negative, upside down. 0 = off.
- It does not move the knob: it adds to its setting. A saved pattern keeps its LFO settings and the LFO keeps
  moving when it loads.
- In bank A by default the chord breathes into the delay (send A, 4 bars) and the bass's tone drifts.

## 6. Patterns and banks (SAVE)
- **Black keys = banks A to K. White keys = the bank's 16 patterns.**
- Stopped, the change is immediate; playing, it waits for the end of the bar (the queued pattern blinks).
- Holding SAVE + a key does the same from any view.
- A pattern stores each track's sequence and sound. Mix, tempo, **mute and solo belong to the project**: they
  do not change with the pattern.
- **Bank A, dub techno** (in D): 16 hand-made patterns, NEBULA to CYKLOS.
- **Bank B, hypnotic techno**: UMBRAL to RETORNO. Four-on-the-floor kick, a MACRO bass in an odd cycle (7, 9, 11,
  13 or 15 steps) turning against the kick, a dark, sparse chord, percussion in a short cycle.
- **Bank C, ambient and drone**: ALBA to OCASO. No kick (only a soft half-time one in 9, 11 and 15), the drone
  always there, slow chords, a string, harmonic or grain melody whose notes wander, a distant bell.
- All three follow the same arc: way in (1-3), build (4-7), break (8), second build (9-14), peak (15), way out
  (16). B and C are generated by rules, not composed by hand: they are the same every time.
- Banks D-K: freely generated patterns (GEN D01…), with the bass on MACRO.

### Saving a pattern (HOME + white key)
1. Press SAVE for the PATTERNS view and pick the destination bank with a black key.
2. Holding HOME, press the slot's white key: it asks "GUARDAR EN B02 ?" (save in B02?).
3. Press the same key again within 3 s: "GUARDADO" (saved).
- The pattern is saved **as it sounds at that moment**, its evolution included. So a random result or a mutation
  you like can be kept.
- A saved pattern has `*` after its name. When its bank loads, it replaces the factory or generated one.
- Saving into the slot that is playing makes what you heard the written pattern.
- **Where it is kept:** in the emulator, in the browser (it survives a page reload, not clearing the site's data or
  another browser). On the FM-1, in the flash, with the same code: *pending on the device*, since nothing has been
  installed on it.
- **Not saved yet:** edits you did not save are lost when you change bank; mix, effects, tempo and modulation
  (the project's) are lost at power-off. There is no way yet to erase a saved pattern and get the factory one
  back: it comes with the project menu (phase 6).

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
- **ARP in the PATTERNS view:** a whole new pattern (sounds and sequences of all six tracks). It is not a loop of
  patterns: each press comes from a different seed.
  - The kick comes from eight families: none, four on the floor (a little under half the time), four with a ghost
    hit at 50 %, broken (euclidean 3, 5 or 7), half time, 2-step, dotted (0-6-12) and a cycle of its own of 12, 14
    or 15 steps.
  - Each time it lands in a **new minor key**; the screen says it ("TODO EN SOL m", all in G minor) and the PATTERNS
    view shows the current one top right.
  - **ARP held + K1** (in the PATTERNS view): **key fixed** [FIJA]. The next randoms stay in the current key, so the
    patterns fit together. Again, free [LIBRE].
  - Each bank has its key: A in D minor, the generated ones spread.
- **HOME + ARP:** undoes the last random (one level).
- **ARP held + ALGORITHM:** amount (10-100 %). **ARP held + a knob:** locks that parameter.

## 9. Send effects (EFFECTS view [EFECTOS])
Each track sends part of its sound to the **delay (A)** and the **reverb (B)** from the MIX page (K3 and K4).
The send is taken after the level: a muted track stops sending, but the tail rings on.
- **PRE send (before the level):** on the MIX page, **HOME + K3** (or K4) switches that send to PRE; the knob is
  then called "A PRE" or "B PRE". In PRE the send ignores the LEVEL: pull the track down to 0 and only its delay or
  reverb keeps sounding (a "ghost" track, very dub). Each track has one PRE setting for A and B together. It belongs
  to the mix, like LEVEL and PAN: it does not change with the pattern. Note: MUTE still cuts a PRE send too.

In the EFFECTS view, SELECT changes page and K1-K4 are the controls:
- **Delay [ECO]:** type [TIPO: TAPE, BBD, PINGPONG], time [TIEMPO] in sixteenths, 1 to 16 (try 3, 5 or 7),
  feedback [REPITE] (at the top the delay feeds itself), tone [TONO] (each repeat darker).
- **Delay 2 [ECO 2]:** wear [DESGASTE] (worn tape in TAPE, a dirty clock in BBD), to reverb [A REVERB], level,
  DUCK (track 1's kick pushes the delay and the reverb down while it sounds; they come back in ~150 ms).
- **REVERB:** K1 **type [TIPO]** picks **PLACA** or **NUBE**; the other three knobs follow the type. Changing it
  starts the reverb empty. Only one of the two plays (they share memory).
- **REVERB 2:** decay [CAÍDA], tone (how dark the tail is), movement [MOVER], level. In NUBE it changes (below).
- **PLACA**, in the manner of Valley's **Plateau** reverb (its controls; our own code): Dattorro's classic plate.
  - **PREDELAY:** silence before the tail, 0 to 500 ms (shown in ms). It sets the hit apart from its reverb.
  - **Size [TAMANO]:** small room (0) to a huge hall (100): a quarter to twice the classic plate. It starts at 43,
    the classic size. Moving it while it plays jumps and bends the pitch, as in Plateau.
  - **Diffusion [DIFUSION]:** down, you hear the first echoes apart; up, everything blurs from the start.
  - **HOME + REC** freezes the plate too: nothing new comes in and the tail keeps ringing.
- **NUBE** (cloud): a cloud of grains on the idea of Mutable Instruments' Clouds (our own code). It listens to the B
  sends, keeps the last 2 seconds and scatters them in small pieces (grains) across the stereo field. On REVERB:
  - **Position [POSICION]:** where grains come from: what just played (0) to almost 2 seconds back (100).
  - **Size [TAMANO]:** short grains (~20 ms, crackle) to long (~1 s, a cloud).
  - **Density [DENSIDAD]:** at the centre, none [NADA] (no new grains); left, regular grains [REG]; right, random
    [AZAR], with the position a little scattered too. The further from the centre, the more grains (12 at once at most).
  - In NUBE the **REVERB 2** page changes: **feedback [REPITE]** (grains go back into the cloud), **texture [TEXTURA]**
    (hard-edged grains on the left, smooth on the right, and a little darker), **pitch [AFINA]** (−24 to +24
    semitones, one at a time) and level.
  - **HOME + REC** freezes the cloud too: it stops recording and grains keep coming from what it holds. FREEZE shows
    at the top.
  - In FLOW, block 3 is called NUBE when it is on.
  - **Shimmer in NUBE:** pitch [AFINA] +12 and feedback [REPITE] high. Grains go back in an octave up.
  - **The cloud's reverb** (on REVERB 3, as in Clouds): the grains go through a small plate of their own.
    **REVERB** is how much (starts at 45; 0 = a dry cloud) and **decay [CAIDA]** how long. HOME + REC freezes it too.
- **REVERB 3:**
  - In NUBE, the first two knobs are the cloud's **REVERB** and **decay [CAIDA]** (see above).
  - **SHIMMER** (PLACA only): the tail is pitched up and fed back into the reverb, so it keeps climbing and
    shining. 0 = off [NO]. **Interval [INTERVALO]:** +12 (octave), +7 (fifth), +19 (octave and fifth), +24 (two
    octaves) or −12 (an octave down: darker). With the reverb frozen (HOME + REC) the shimmer stops.
  - **Low cut [GRAVES]:** cuts lows before the reverb, 20 to ~400 Hz (starts at ~90 Hz). Higher = less kick and bass
    mud in the tail. For PLACA and NUBE.
  - **Width [ANCHO]:** 0 = mono reverb, 50 = as it comes, 100 = wider. For PLACA and NUBE.
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
- **DIGITAL** (at the end of the master: after the compressor and tape, before the limiter):
  - **BITS:** off [NO], or 16 down to 4 bits (lower resolution: grain, then harsh noise).
  - **Sample rate [MUESTREO]:** off, or /1 to /32 (lowers the sample rate: metallic aliasing).
  - STUTTER was taken out on 12 November to free memory for the reverb and for Plaits. For something close: delay
    time 1 or 2, hold REC (throw) and HOME + REC (freeze).
- The bottom row shows the page before, this one and the next, and the page number (1/11 … 11/11).

Changing the time in TAPE bends the pitch, like a tape changing speed. Changing the type starts the delay
empty. The longest time is 1.5 s (0.75 s in PINGPONG); a time that does not fit is halved.

The effects belong to the project: they do not change with the pattern.

### MAREA: a filterbank moved by a wave
An effect you put **on the track you choose**, meant for chords, pads and sound design. Sixteen band-pass filters
(40 Hz to 15 kHz, at fixed points) whose levels a wave moves. Inspired by Hypnus Records' Slink: the controls do
what its manual describes, but the algorithm is our own and does not sound the same.
- **MAREA:** track [PISTA] (which one it is on; there is one MAREA, putting it on another track moves it), mix
  [MEZCLA] (dry to wet), speed [VELOC] and multiply [MULTI].
  - **Speed** has zero in the middle: right, the wave travels forwards (up towards the highs), left it comes back,
    and in the middle it stands **still** [QUIETA], keeping its shape. **HOME + speed** switches between free
    [LIBRE] (Hz) and tempo [TEMPO] (32 bars to 1/4 bar a cycle).
  - **Multiply:** how many wave cycles fit across.
- **MAREA 2:** ripple [RIZO] (small waves on top of the wave), offset [DESFASE] (places the wave by hand; with an
  LFO, sweeps it), invert [INVIERTE] (depth: 100 full, 0 none, −100 upside down: crests become troughs), gravity
  [GRAVEDAD] (pulls the wave down: wide valleys and thin crests; at the top, extreme shapes).
- **MAREA 3:** from and to [DESDE, HASTA] (the part of the spectrum where the wave acts; outside it the bands keep
  their level, still), RESO (wide or narrow bands) and width [ANCHO] (the bands alternate left and right; 0 = mono).
- The screen draws the wave over the frequencies, with the area's edges dotted. Top left shows the speed (> forwards,
  < back; yellow when locked to the tempo).
- It comes out in stereo: that track's PAN works as a balance. Sends A and B already get the sound with MAREA.
- Modulation destinations: MAR VELOC, MAR MULTI, MAR RIZO, MAR DESFASE, MAR GRAVEDAD and MAR MEZCLA.
- By default it sits on track 3, fully wet (mix 100), travelling forwards at about 3 s a cycle, full width.

### FLOW: the effects chain (HOME to FLOW)
The screen draws the chain as it runs: the tracks, MAREA on its track, the sends, the delay and the reverb, and the
master (filter, compressor, tape and limiter). Each block has a number.
- **White keys 1-6:** turn a block on or off: 1 MAREA, 2 delay [ECO], 3 REVERB, 4 filter [FILTRO], 5 COMP, 6 tape
  [CINTA] (saturation). Lit keys are the blocks that are on. A block turned off fades out in ~10 ms (no click) and is
  no longer computed: on the FM-1 that frees CPU (*pending on the device*). Back on, the delay or reverb start empty.
- **K1 filter at [FILTRO EN]:** where the master filter sits: all [TODO] (as before), dry [SECO] (the tracks only:
  echoes and reverb ring open while you close it, the classic dub move) or effects [EFECTOS] (the returns only).
- **K2 space [ESPACIO]:** ECO>REV (the delay feeds the reverb, as before) or REV>ECO (the reverb feeds the delay:
  echoes of a cloud).
- **K3 drive:** COMP>SAT or SAT>COMP. Tape before the compressor sounds more held; after it, dirtier.
- **K4 cross [CRUCE]:** how much of the first in SPACE goes into the second. It is the same knob as A REVERB on the
  ECO 2 page; in FLOW it works either way.
- The final limiter cannot be removed. FLOW belongs to the project, like the effects.
- There is no free reordering: only these fixed options, so no combination breaks the sound.

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

## 11. Scenes (HOME + SEQ)
A scene is a snapshot of several settings (up to 8) that you go into and out of with one key, with a fade. The
project has 8 scenes.
- **HOME + SEQ:** enters or leaves SCENES mode [ESCENAS]. In this mode the **black keys 1-8** are the scenes (they
  do not play notes). The header shows "ESC" and the scenes that are in.
- **Hold a black key:** the scene comes in while you hold it; on release it goes out with the same fade.
- **HOME + black key:** latches it in (or takes it out). A latched scene has its key lit.
- **While holding a scene:**
  - The screen shows its name, a bar for how far in it is, the fade and what it sets.
  - **Turn a knob** to store its value in the scene: in EFFECTS, that page's parameter; in FLOW, its settings; in
    the track views, the current page's parameter of the selected track (P1-P6 before the name).
  - **SELECT:** the fade [FUNDIDO]: at once [AL MOMENTO], 1/16, 1/8, 1/4, 1/2, 1, 2 or 4 bars. It follows the tempo.
  - **ARP:** empties the scene.
- Continuous settings glide during the fade. Stepped ones (a type, an order, on/off) switch halfway through. If two
  scenes set the same thing, the higher number wins.
- Scenes do not move the knobs: on the way out, everything returns to where it was.
- **Factory:** 1 SUMERGIR (submerge: filter closed and long delay, 1 bar), 2 CATEDRAL (all reverb, 2 bars), 3 CINTA
  ROTA (broken tape: saturation and wear), 4 DIGITAL (bits and sample rate), 5 TUNEL (tunnel: thin and far, 1/2 bar),
  6 SECO (dry: no delay or reverb), 7 SHIMMER (plate with shimmer, long, 2 bars), 8 ECO INFINITO (endless delay: feedback at 97 % and no
  track sends to it, so what is there stays).
- **Not yet saved** at power-off: they go back to the factory ones (project saving, phase 6).

## 12. To come
Next: saving the project (mix, effects, scenes, tempo), names, erasing a saved pattern, clock and MIDI (phase 6).
Then: more engines, demo banks per style, testing on the device (7, only when decided).
- FILL has no shortcut any more: HOME + PLAY now freezes EVOLUTION.
