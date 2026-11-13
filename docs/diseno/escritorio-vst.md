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

## Ventana actual (13 nov, segunda versión)
Diseño propio, dibujado con código (sin imágenes), redimensionable. Captura real hecha en la máquina de compilación: la herramienta `EpkShot` dibuja la ventana en un PNG sin pantalla y el flujo "Desktop" lo deja en el zip y en el registro.
- **Órbita:** las seis pistas como anillos concéntricos (la 1 fuera), un punto por paso, los golpes en el color de la pista y el paso que suena resaltado. En el centro, el patrón, su ranura y el tempo.
- **Seis carriles:** nombre, motor (y modelo de MACRO), los pasos con los golpes, nivel, M y S. **Clic en un paso lo enciende o lo apaga**; clic en el carril elige la pista.
- **Mandos de la pista elegida**, con los nombres propios de su motor y su color: SONIDO (7), RITMO (longitud, golpes, rotar, swing), MEZCLA (pan, envíos), VIDA (mutar, ausencia).
- **MASTER:** eco, reverb, caída, shimmer, filtro, reso, comp.
- Arriba: PLAY, tempo, qué renueva el aleatorio, ALEATORIO, DESHACER. Bancos A–K y los 16 patrones bajo la órbita.
- **Falta** respecto al firmware: probabilidad, condición y micro por paso; división; evolución completa (QUÉ, CADA, VUELTA); LFO de pista; el resto de efectos (tipos de eco y reverb, NUBE, MAREA, DIGITAL); FLOW; MOD; escenas; guardar patrones en ranuras.
- Defectos vistos en la captura: los paneles de mandos de las dos filas se tocan; los botones de patrón son pequeños.

## Comprobado
- Compila universal (arm64 + x86_64).
- `auval` (el validador de Apple): AU VALIDATION SUCCEEDED (carga, suena, cambia de formato, guarda y recupera estado).
- `pluginval` nivel 5 sobre el VST3, sin pruebas de ventana: SUCCESS.

## Sin comprobar
- Que se abra y se vea bien en Ableton Live, Logic o como app en el Mac de Javier.
- Cómo suena y cuánta CPU gasta.
- La ventana en una pantalla real (solo la captura de la máquina de compilación) ni el uso con ratón: arrastrar mandos, clic en pasos.

## Siguiente
1. Javier opina sobre la captura y, si puede, lo abre en el Mac.
2. Completar lo que falta de la ventana por orden de uso.
3. Parámetros automatizables, alineación con el compás del DAW, guardado completo del patrón.
