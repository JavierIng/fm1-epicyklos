# EPICYKLOS

*(nombre provisional)* — firmware abierto para el **M-VAVE FM-1**: instrumento de 6 pistas para dub techno,
deep/hypnotic techno, ambient y drone. Licencia **GPL-3.0-only**.

> **Estado: Fase 0 (entorno).** Por ahora `app/` es la plantilla de fm1-base renombrada: compila, suena en el
> emulador y pasa sus pruebas. Nada de esto se ha instalado en un FM-1. **No lo flashees.**

## Estructura

| Ruta | Qué es |
|---|---|
| `app/` | La aplicación (hoy: la plantilla de fm1-base, MPL-2.0, a sustituir en la Fase 1) |
| `base/` | [fm1-base](https://github.com/charlesvestal/fm1-base) como submódulo, con [felucca-bsp](https://github.com/hugelton/felucca-bsp) dentro |
| `assets/chords/` | Tablas de acordes de Plaits Palette (MIT) |
| `docs/diseno/` | Diseño y decisiones |
| `tests/` | Pruebas: simulador en el ordenador, emulador sin navegador, prueba de C++ para el compilador del FM-1 |
| `tools/` | `wasi-emcc` (compilar el emulador sin Emscripten), `headless.mjs`, `rgb565_to_png.py` |

## Compilar

```sh
git clone --recursive https://github.com/JavierIng/fm1-epicyklos
cd fm1-epicyklos
./build.sh --web      # emulador -> app/build/emu/  (python3 -m http.server en esa carpeta)
./build.sh --host     # simulador en el ordenador
sh tests/run_tests.sh # pruebas
./build.sh            # firmware del FM-1 (toolchain de JieLi + SDK AC79; ver base/README.md)
```

## Licencia y créditos

GPL-3.0-only (ver `LICENSE`). Partes con otras licencias en `LICENSING.md`; créditos en `CREDITS.md`.
M-VAVE y FM-1 son marcas de su propietario. Este firmware no está hecho, respaldado ni soportado por M-VAVE.
Instalar otro firmware en un FM-1 es bajo tu responsabilidad.
