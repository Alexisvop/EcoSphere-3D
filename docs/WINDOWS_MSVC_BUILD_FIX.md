# EcoSphere: corrección MSVC y validación nativa Windows

## Alcance y seguridad

- Fecha: 2026-11-08. Windows 11 x64, Debug, C++17, OpenGL 3.3 Core.
- HEAD inicial: `12d9fb15875f98c387ae4fefb112ce294e21a1ff`.
- Rama existente reutilizada: `fix/windows-msvc-srgb-header`; inicialmente limpia,
  sin diferencias con main. Remoto: `https://github.com/Alexisvop/EcoSphere-3D.git`.
- No se modifican main, recursos artísticos, SDK, controles, renderizador,
  configuraciones Linux ni dependencias. No se incorpora cromo experimental.
- Se conserva `build/windows-dev`; sin limpieza, descargas, Release, instalador,
  push, PR ni merge. La compatibilidad Linux se conserva por diseño; no se
  ejecutó una compilación Linux en esta intervención.

## Errores y cambios

| Archivo | Diagnóstico y corrección mínima |
| --- | --- |
| `src/app/SrgbFramebuffer.hpp` | C2039/C3861 para `std::to_string` y C2512 derivado al construir `std::runtime_error`: faltaba `<string>`. Se añade; `<stdexcept>` ya estaba presente. No cambia la validación sRGB. |
| `src/modules/EcoCycle/Module.cpp:104` | C4244 reproducida mediante `/Zs` con las opciones del build: el argumento yaw `i<3?0:90` se convertía de int a float. Se usa `0.0f:90.0f`; ambos valores son exactamente representables, sin cambio numérico. La recompilación elimina esta advertencia. |
| `tests/presentation_textures_regeneration.py:62` | En Windows, `str(Path)` producía claves con `\`, incompatibles con las claves `/` del manifiesto; `KeyError` en la prueba de preservación de recursos. Se utiliza `as_posix()`, sin cambiar hashes, recursos ni expectativas. |
| `tools/validate_plaza_inventory.py` | `core.autocrlf=true` producía CRLF y fallaban los hashes LF del baseline del renderizador. Se normaliza únicamente CRLF→LF al calcular esos hashes de fuentes de texto. Los 17 archivos coinciden con HEAD tras esa normalización. No se cambian fuentes gráficas ni hashes esperados; los hashes binarios de GLB siguen siendo estrictos. Se añaden autocomprobaciones LF/CRLF y rechazo de cambios de código. |
| `docs/WINDOWS_MSVC_BUILD_FIX.md` | Este informe, sin rutas personales ni secretos. |

Una comprobación adicional en memoria sobre las 17 fuentes reales confirmó que
LF/CRLF conservan sus hashes esperados y añadir código los invalida.

## Configuración y compilación

- Preset existente `windows-dev`: Ninja, `CMAKE_BUILD_TYPE=Debug`,
  `BUILD_TESTING=ON`, x64. Se reutiliza la configuración CMake ya satisfactoria.
- CMake **3.29.2**, Ninja **1.12.0**, Python **3.12.10**.
- Compilador real: **MSVC 19.51.36260 x64**, Visual Studio 2026.
  El directorio del toolset se denomina `14.51.36231`; no sustituye la versión
  reportada por `cl`. Linker 14.51.
- GLFW **3.4.0**, instalación local `dependencies/glfw-3.4-debug` fuera del
  repositorio, enlazada estáticamente mediante `glfw3.lib`.

```powershell
cmake --build --preset windows-dev --parallel 2
ctest --preset windows-dev --output-on-failure
```

Compilación completa satisfactoria: primero 63 acciones pendientes y después
20 acciones incrementales al corregir C4244; ambos builds terminaron con código
0. Todos los componentes, incluidos los ejecutables gráficos opt-in, enlazaron.
El último build no emitió advertencias nuevas. En la primera pasada quedaron
advertencias no bloqueantes preexistentes:

- C4702, `tests/aquabio_tests.cpp:34` y `tests/ecocycle_tests.cpp:33`: retorno
  inalcanzable para algunas especializaciones de `if constexpr` en `std::visit`.
- C4996, `tests/graphics_failure_tests.cpp:54,65`: `strcpy` en funciones GL
  simuladas con mensajes constantes que caben en el buffer anunciado de 64 bytes.

No se suprimen advertencias ni se modifican esas pruebas por motivos cosméticos.

## CTest

La primera ejecución fue interrumpida por el límite de 120 s de la herramienta;
no constituye una pasada completa. La siguiente terminó **39/40** en **452,67 s**,
con el único fallo `plaza_inventory` por CRLF. El error anterior de regeneración
ya estaba corregido. No se deshabilitaron pruebas ni se modificaron expectativas.

**Repetición final: 40/40 pruebas satisfactorias, 0 fallos, 478,53 s, código 0.**
Incluye `interaction_scale_limits_cpu`, regeneración de texturas,
`plaza_inventory`, contratos SDK y consumidor externo. Se conserva la cobertura
completa, sin exclusiones ni cambios en las expectativas.

## Ejecutable, recursos y DLL

- Existe `build/windows-dev/ecosphere.exe`, PE x64, **3.393.536 bytes**.
- Existen `build/windows-dev/assets/` y `build/windows-dev/shaders/`.
- Comparación byte a byte satisfactoria: **113 archivos de modelos/recursos**
  copiados (1 común, 27 Plaza, 28 EcoEnergy, 28 AquaBio, 24 EcoCycle y 5
  compartidos) y **6 shaders**. Las texturas de modelos GLB se cargan correctamente.
- `dumpbin /DEPENDENTS`: `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, `SHELL32.dll`,
  `MSVCP140D.dll`, `VCRUNTIME140D.dll`, `VCRUNTIME140_1D.dll`, `ucrtbased.dll`.
- Las cuatro DLL CRT Debug están presentes en System32; no falta ninguna en
  este equipo. GLFW estática no requiere `glfw3.dll`. OpenGL/WGL requiere
  `opengl32.dll` (presente) y el controlador gráfico del sistema.
- No se copian DLL al build. El CRT Debug depende del entorno de desarrollo;
  este binario no es un paquete redistribuible para usuarios finales.

## Validación gráfica real

Pruebas sobre **Intel Iris Xe**, controlador **31.0.101.4032**, OpenGL **3.3.0
Core**, GLSL **3.30**, GLFW **3.4.0 Win32 WGL**. No son simulaciones CPU.
La sonda sRGB informó `query_error=0`, `probe_error=0`, lineal **64** y codificado
**137**, sin modificar su implementación.

1. Desde el directorio del binario se ejecutó `ecosphere.exe --plaza`: ventana
   visible 1280×720, modelos, texturas, iluminación, cielo, HUD y FPS presentes.
2. Automatización Win32 `SendInput`, con PID propio y foco comprobados: WASD
   hasta cada acceso, Enter a **EcoEnergy, AquaBio y EcoCycle**, y H de vuelta a
   Plaza. Los tres módulos cargaron; P pausó/reanudó sus animaciones. Capturas y
   registros confirman las seis transiciones y salida normal mediante Esc.
3. F3/F4 ocultaron/restauraron FPS/HUD; F5 alternó Phong y Blinn-Phong, confirmado
   por los registros. Se observaron aproximadamente 60 FPS estables con VSync;
   no es una medición de rendimiento Release ni un ensayo sin carga concurrente.
4. `shading_graphics_tests.exe . windows-shading-evidence
   ../../docs/evidence/phong-fresnel/reference-model.frag`: **SHADING GPU PASS**,
   código 0. Comprueba Phong/Blinn-Phong, Fresnel, reflejos, cubemap, texturas,
   HUD y las cuatro zonas. Las cinco comparaciones contra el shader de
   referencia dieron **0 píxeles distintos**, diferencia máxima **0**.

### Regresión crítica AquaBio

Se ejecutó `ecosphere.exe --module aquabio --content` en una ventana Win32 real,
800×600. Mediante teclado Win32: P pausa, Tab selecciona **AQUA-009-demo**
(confirmado en log), C mantenida **20,125 s**, después V **2,125 s**, P reanuda y
Esc cierra con código **0** y `Clean exit`. El foco se comprobó durante toda la
pulsación; no hubo cierre inesperado. Las capturas muestran reducción y
ampliación posterior. La cobertura CPU de escala se registra separadamente
en CTest; no sustituye esta prueba nativa.

Con cámara fija, el marco amarillo del objeto en la misma región de captura
ocupó **139 píxeles** inicialmente, **2** después de C y **30** después de V:
evidencia visual adicional de reducción y recuperación, sin leer memoria del juego.

## Evidencia local e integridad

Los registros y herramientas de automatización permanecen en el build ignorado
por Git, sin publicar rutas personales:

- `build/windows-dev/windows-msvc-build.log` y `windows-msvc-build-final.log`.
- `build/windows-dev/windows-msvc-ctest.log` y `windows-msvc-ctest-final.log`;
  también `Testing/Temporary/LastTest.log`.
- `build/windows-dev/windows-msvc-ecocycle-warning.log` (reproducción previa).
- `build/windows-dev/windows-baseline-guard.json`.
- `build/windows-dev/windows-resource-integrity.json`: hashes del ejecutable y
  de los 119 archivos copiados, con rutas relativas.
- `build/windows-dev/windows-native-check.py` y `windows-native-evidence/`:
  capturas PNG, logs, duraciones/foco en `results.json` y `navigation-results.json`.
- `build/windows-dev/windows-msvc-shading-gpu.log` y `windows-shading-evidence/`.

`git diff --check` satisfactorio. El diff de `assets/`, `include/`,
`CMakeLists.txt` y `CMakePresets.json` está vacío; no se cambiaron recursos
artísticos ni el SDK público. Los archivos nuevos versionables se revisaron para
evitar rutas personales y credenciales; las evidencias locales siguen ignoradas.

## Riesgos, pendientes y decisión

- Falta la revisión externa del diff y del tratamiento de CRLF; no se alteran
  manifiestos para hacer pasar hashes incorrectos.
- Se recomienda aceptación manual en el equipo de exposición: recorrido visual,
  texturas, todas las animaciones, cambio F5, HUD/FPS, redimensionado y
  pérdida/recuperación de foco. Repetir AquaBio con P, Tab, C durante 20 s, V y Esc.
- No se ejecutó la suite gráfica opt-in completa de benchmarks HUD/FPS ni una
  prueba prolongada de estabilidad; el ensayo gráfico de sombreado sí pasó.
- Debug y sus 40 pruebas están validados en este equipo. Falta la auditoría
  externa antes de autorizar Release; después, planificar por separado su
  compilación, aceptación manual y redistribución correcta del CRT.

**Intervención detenida para auditoría externa de ChatGPT; sin instalación.**
