# EPICYKLOS para el ordenador (VST3 / AU / app independiente)

Estado: **primer esqueleto que compila y pasa validación automática en macOS. No lo ha abierto nadie en un DAW.**

## Qué es
- El mismo núcleo de sonido del firmware (`app/dsp`), sin cambios, envuelto como plugin y como app. Carpeta `desktop/`.
- La pantalla y los botones del FM-1 (`app/app.c`) **no se usan**: la ventana es nueva y no imita al aparato.
- Formatos: VST3, Audio Unit y app independiente, para macOS (Apple Silicon e Intel en un solo binario). Windows y Linux: sin hacer.

## Decisiones (13 nov 2026)
- **Envoltorio: JUCE 8**, bajo su opción AGPLv3, que se combina con nuestra GPL-3.0. Motivo: un solo código da VST3, AU y app; es el camino más documentado. Se descarga al compilar; no está copiado en el repo.
- **Se compila en GitHub Actions** (flujo "Desktop", runner `macos-15`; `macos-14` no está disponible en esta cuenta). Aquí no hay Mac: no puedo abrir el plugin ni ver su ventana.
- **Sin firma de Apple.** Firma local ("ad hoc"). macOS bloquea lo descargado: hay que quitar la cuarentena con `xattr` (ver `desktop/LEEME.txt`). Firmar y notarizar pide cuenta de desarrollador de Apple (99 $/año).
- **Frecuencia de muestreo:** el núcleo corre a 44,1 o 48 kHz tal cual (medido: mismo nivel y mismo tempo). A 88,2 y 96 kHz corre a la mitad y se dobla con una media: se pierde algo de agudo extremo. Los tiempos máximos de eco y el tamaño de la reverb dependen de la frecuencia (a 48 kHz, un 8 % más cortos).
- **Tempo y transporte:** en un DAW manda el DAW (tempo y play). Empieza siempre desde el principio del patrón: no se alinea con el compás del DAW si arrancas a mitad. Tempo fuera de 40–200 BPM se recorta.
- **MIDI:** notas, canal 1-6 → pista 1-6; otro canal → pista elegida. Sin precisión de muestra dentro del bloque.

## Límites conocidos
- **Una sola instancia por proyecto.** El núcleo es un único motor global (como en el FM-1). La segunda instancia avisa y calla. Quitar este límite pide reescribir el núcleo como objeto.
- **Estado en el proyecto del DAW:** banco, patrón, tempo, mutes, todos los parámetros de las pistas, efectos y modulación. **No** guarda pasos editados a mano, probabilidades por paso, escenas ni patrones guardados en ranuras.
- Los parámetros no están expuestos al DAW: no se pueden automatizar todavía.
- Sin salidas separadas por pista.

## Ventana actual (provisional)
Transporte y tempo · aleatorio (TODO / GROOVE / SINTES) y deshacer · bancos A–K y 16 patrones con nombre · seis pistas con nivel, mute, solo y luz de golpe · 11 mandos de la pista elegida · 6 mandos de efectos y master. Redimensionable. Falta casi todo lo demás del firmware (ritmo, pasos, evolución completa, LFO, las 11 páginas de efectos, FLOW, MOD, escenas).

## Comprobado
- Compila universal (arm64 + x86_64).
- `auval` (el validador de Apple): AU VALIDATION SUCCEEDED (carga, suena, cambia de formato, guarda y recupera estado).
- `pluginval` nivel 5 sobre el VST3, sin pruebas de ventana: SUCCESS.

## Sin comprobar
- Que se abra y se vea bien en Ableton Live, Logic o como app en el Mac de Javier.
- Cómo suena y cuánta CPU gasta.
- La ventana: no la he visto dibujada.

## Siguiente
1. Javier lo abre y manda una captura.
2. Maqueta de la interfaz definitiva para decidir el diseño antes de programarlo.
3. Parámetros automatizables, alineación con el compás del DAW, guardado completo del patrón.
