# Licencias

EPICYKLOS se distribuye bajo la **GNU General Public License v3.0 only** (`LICENSE`). Quien distribuya el
firmware debe ofrecer también su código fuente completo.

| Parte | Licencia | Dónde |
|---|---|---|
| Código propio de EPICYKLOS | GPL-3.0-only | todo lo que no se lista abajo |
| fm1-base (Charles Vestal) | MPL-2.0 | `base/` (submódulo, sin modificar) |
| felucca-bsp (Leo Kuroshita, Hügelton Instruments, y colaboradores) | MPL-2.0 | `base/bsp/` (submódulo, sin modificar) |
| Plantilla de fm1-base copiada como punto de partida | MPL-2.0 | `app/app.c`, `app/app.h`, `app/tests/`, `app/docs/`, `app/site/`, `app/words.tsv`, `app/fm1.toml` (hasta que se reescriban) |
| Tablas de acordes de Plaits Palette | MIT | `assets/chords/` (autores en cada archivo) |
| Partes del SDK AC79 de JieLi que lleva el paquete | Apache-2.0 | solo en los `.fwsc` compilados |

MPL-2.0 es compatible con GPL-3.0: los archivos MPL conservan su licencia y el conjunto se distribuye como GPL-3.0.
Bluetooth usa la pila propia de fm1-base (`ble_stack = "own"`), sin bibliotecas binarias de JieLi, para que el
firmware GPL se pueda distribuir.

## Desktop plugin (`desktop/`)
The VST3 / AU / standalone build links the JUCE framework (https://juce.com), fetched at build time and used under
its AGPLv3 option. Our own code in `desktop/` is GPL-3.0-only; GPL-3.0 section 13 allows combining it with
AGPLv3 code, and the combined binaries are then also subject to the AGPLv3's terms.
