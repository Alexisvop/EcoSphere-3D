# Correcciones multiplataforma y FPS

## Alcance y procedencia

Trabajo exclusivo en el clon público `Alexisvop/EcoSphere-3D`, rama
`fix/cross-platform-stability-fps`, creada después de `git fetch origin` desde
`origin/main` = `1e8e334f9482cfb78626a59179abd5e948d8c15c` (merge del PR #1).
El árbol estaba limpio. No se toca el repositorio privado ni el candidato histórico.
No se modifica main, ni se crea PR, ni se hace merge/rebase/force push.
Implementación/pruebas: `9a435ff` (`fix: stabilize cross-platform checks and add real-time FPS overlay`).
El segundo commit contiene documentación/evidencias; su SHA y el HEAD remoto
se entregan con el informe de publicación.

## Entorno y auditoría inicial

Arch Linux x86_64; GCC 16.2.1 (20260810), CMake 4.4.5, GLFW 3.5.1,
Python 3.14.9; Intel Iris Xe (ADL GT2), Mesa 26.2.6, OpenGL 4.6 Core.
Windows/MSVC y GPU AMD no disponibles; no se certifica compatibilidad nativa.

La primera configuración falló porque `build/linux-dev/CMakeCache.txt` conservaba
la raíz de otra clonación. Mensajes de CMake: `The current CMakeCache.txt directory
... is different than the directory ... where CMakeCache.txt was created` y
`The source ... does not match the source ... used to generate cache`.
Se conservó todo el build anterior en `build/linux-dev-before-path-fix`, sin
borrarlo, y se usó de nuevo la ruta oficial `build/linux-dev`. No se atribuye un
resultado CTest a ese intento de configuración fallido.

Después se ejecutaron, antes de corregir código:

```bash
cmake --preset linux-dev
cmake --build --preset linux-dev
ctest --preset linux-dev --output-on-failure
```

Configuración y compilación aprobadas; CTest **33/34** en 57.24 s.
[Salida completa inicial](evidence/cross-platform-fps/baseline-ctest.txt).
Las rutas personales se sustituyen por `<repo>`; se conserva el mensaje del fallo:
`ValueError: Renderer/shader modificado: src/engine/graphics/Environment.cpp`.
Es un fallo del validador Python, no de compilación ni de renderizado.
Los avisos existentes de variable sin usar en `zone_navigation_tests.cpp` y de
indentación en `ecocycle_tests.cpp` no impidieron compilar.

| Prueba señalada en PR #1 | Linux inicial | Linux final |
|---|---|---|
| plaza_assets_cpu | PASS | PASS |
| plaza_inventory | FAIL: hash anterior al PR #1 | PASS |
| content_modules_integration_cpu | PASS | PASS |

El PR #1 informó 31/34 en Windows y cuatro pruebas AquaBio aprobadas. Aquí las
cuatro pruebas AquaBio pasan desde la línea base. No se reprodujeron los otros
dos fallos de ejecución Windows; no se inventan sus mensajes ni causas definitivas.

## Causas y cambios

- GLFW: `EXACT` en el template de versión compara cadenas: `3.4` y `3.4.0`
  no son iguales. Windows pide ahora 3.4.0; Linux conserva 3.5.1. Se mantiene
  `EXACT CONFIG REQUIRED`. La reproducción del template CMake rechaza 3.4 y
  3.5.0 y acepta 3.4.0: [resultado](evidence/cross-platform-fps/glfw-version-check.txt).
- Cabecera GLFW: `content_module_tests` incluía GLFW sin declarar su target;
  las cabeceras del sistema Linux ocultaban el defecto. Enlaza ahora `glfw`
  de forma PRIVATE, sin rutas absolutas ni includes globales. Compila en Linux.
- sRGB: el chequeo anterior convertía cualquier resultado distinto de `GL_SRGB`
  (incluido un error de consulta) en cierre. Se consulta explícitamente framebuffer
  0 / `GL_BACK_LEFT` y se registra el error. La decisión depende de una prueba
  real de conversión con `glClear`/`glReadPixels`, no de una suposición sobre AMD.
  Entrada lineal .25: 64 sin conversión y 137 con conversión (tolerancia 3 códigos
  para cuantización/dithering). Se conserva la corrección de color y los shaders.
  Consulta inconcluyente + conversión correcta permite continuar; conversión
  fallida muestra un error. El fallo AMD no fue reproducido y requiere prueba nativa.
- Inventario: el SHA registrado de `Environment.cpp` era
  `18ac0637615f9b483237c55de69c5dae2476283973ac058dc929bff434010ba6`,
  idéntico al archivo del primer padre del merge #1. Se actualiza únicamente esa
  entrada al cielo ya aprobado:
  `22277d64afccd20064ff27b3fd97fbbc7addc7c2b33af5cdba4a5f5ddf8d00b3`.
  No se regeneran inventarios, transformaciones, modelos o evidencias históricas.
- Precisión: la plaza comparaba CSV numérico como texto y la integración contaba
  movimiento mediante desigualdad exacta de matrices float. Se comparan números
  con `8 * epsilon(float) * max(1, abs(a), abs(b))` (~9.54e-7 relativo;
  ~29 micrómetros para coordenadas de 30 m). Esto cubre pequeños redondeos de
  trigonometría/recomposición; no admite desplazamientos geométricos de milímetros.
  Header, IDs, rutas, orden, cantidades y hashes permanecen exactos. La integración
  sigue exigiendo ocho mecanismos móviles por instalación y 6/7/7 interacciones.
  Estas fragilidades se corrigen preventivamente; no prueban la causa Windows.

Hipótesis restantes: el parser numérico usa locale clásico y rechaza NaN/Inf,
campos faltantes o adicionales; Python `float` no depende del separador regional.
La suite Linux no muestra cambios de orden ni rutas de recursos incorrectas.
El único desfase de evidencia confirmado es el SHA del cielo. Dependencias,
representación decimal y caché de rutas se evaluaron por separado; no se atribuyen
todos los fallos a una causa común.

## FPS y pruebas

F3 estaba libre. El contador aparece inicialmente, usa el reloj monotónico GLFW,
acumula intervalos de ~0.5 s y reinicia con foco/minimización, tiempo inválido o
saltos >2 s. `FPS: -` representa muestra no disponible. No se muestra frametime.
Se reutiliza el HUD, agregando los glifos numéricos que faltaban. El panel oscuro
queda arriba a la derecha y el HUD educativo debajo de la banda superior de 40 px.
La medición no cambia el delta de animaciones ni el SDK.

CTest final: **35/35** (34 existentes + `fps_cpu`), 56.03 s.
[Salida completa final](evidence/cross-platform-fps/final-ctest.txt).
La prueba CPU verifica 64 FPS simulados, intervalos inválidos, reinicio de foco,
PRESS/REPEAT/RELEASE de F3, controles reservados, glifos y tamaños, y rechazo de
cambios de IDs/desplazamientos fuera de tolerancia en CSV.

La aplicación real completó cuatro zonas y 72 transiciones, verificó recursos
exclusivos/compartidos y 12 contribuciones de framebuffer de modelos compartidos.
La sesión Wayland concedió **951×1023**, no 1280×720: se informa su tamaño real.
[Registro](evidence/cross-platform-fps/navigation-final.txt).
La entrega de teclas X11 real con xdotool confirmó F3 off/on, F1/F2 y salida limpia;
P/R se enviaron en plaza, donde no hay mecanismo seleccionado. La semántica de
P/R/interacciones en instalaciones se comprueba en las pruebas CPU existentes.
[Registro de entrada](evidence/cross-platform-fps/native-input.txt).

El ejecutable gráfico opcional prueba el framebuffer real X11 en ventanas ocultas:
cuatro zonas × tres resoluciones, diferencia de píxeles confinada al panel,
y equivalencia exacta al ocultar/mostrar F3. Espera el resize asíncrono y calienta
los buffers después de redimensionar antes de comparar. Las capturas `hub-*`,
`ecoenergy-*`, `aquabio-*`, `ecocycle-*` usan una lectura simulada de 64 FPS para
comparación determinista; `live-*` muestran la lectura real de la aplicación.
Los PNG son conversiones sin pérdida de los PPM de `glReadPixels`.

## Rendimiento y almacenamiento

Se conservan CSV antes/después del recorrido y pares overlay apagado/encendido
por zona a 1280×720, 300 frames con 30 de calentamiento, swap solicitado 0 y 1.
Los pares comparten ejecutable, cámara, materiales y HUD; aíslan el draw adicional.
Las mediciones son observaciones locales, no una garantía para otros equipos.
Las primeras ejecuciones del recorrido y pruebas CPU coincidieron parcialmente;
no se usa su diferencia como estimación causal del coste del contador.

El árbol inicial ocupaba **246226789 bytes** (du -sb, incluye .git y build).
El build previo incompatible está preservado, no se eliminaron archivos.
No se duplicaron recursos fuente ni se añadieron dependencias; CMake continúa
preparando sus copias de ejecución habituales. Medición final antes del commit documental: **427306698 bytes**
(~407.5 MiB), frente a ~234.8 MiB iniciales: crecimiento ~172.7 MiB, inferior
a 2 GiB. Los PNG/CSV/registros nuevos ocupan ~1.4 MB; el crecimiento principal
es el build nuevo junto al build incompatible preservado. El commit/publicación
puede aumentar ligeramente los bytes de .git; se entrega también la lectura
posterior al push en el informe final. Los PPM temporales se conservan en /tmp,
fuera de la medición del árbol; no se ejecutó limpieza automática.

## Límites y fuentes

Windows: configuración y lanzador revisados estáticamente; falta ejecutar
`cmake --preset windows-dev`, build, CTest, prueba gráfica y teclas en MSVC/AMD.
No hay resultados CTest Windows en esta sesión. No se altera la lógica de los lanzadores (solo se precisa 3.4.0 en mensajes Windows),
las cuatro zonas, GLB/Blender, contratos SDK v1.0, créditos ni licencias.
Se conservan las metas de 110 modelos, 252 instancias, 20 interacciones y 24
animaciones; no se autoriza ampliación ni se cambian sus inventarios.

Referencias oficiales: [GLFW window guide](https://www.glfw.org/docs/3.4/window_guide.html),
[consulta de attachment OpenGL](https://wikis.khronos.org/opengl/GLAPI/glGetFramebufferAttachmentParameter),
[GLFW 3.4 CMake](https://github.com/glfw/glfw/blob/3.4/CMakeLists.txt),
[PR #1 público](https://github.com/Alexisvop/EcoSphere-3D/pull/1).

## Archivos modificados

- Build: `CMakeLists.txt`, `CMakePresets.json`; mensajes de `run-windows.ps1`.
- Aplicación: `src/app/main.cpp`, `FpsCounter.hpp`, `SrgbFramebuffer.hpp`,
  `NavigationHud.hpp/.cpp`, `NavigationHudLayout.cpp`.
- Pruebas: `tests/plaza_tests.cpp`, `tests/content_module_tests.cpp`,
  `tests/InstanceCsv.hpp`, `tests/fps_tests.cpp`, `tests/fps_graphics_tests.cpp`.
- Baseline aprobado: solo una entrada de `docs/assets/PLAZA_RENDER_BASELINE.sha256`.
- Documentación: `README.md`, `docs/BUILD.md`, `docs/ARCHITECTURE.md`, este informe
  y las capturas/CSV/registros nuevos bajo `docs/evidence/cross-platform-fps/`.

Se revisó el diff completo de código, documentación y manifiesto; búsqueda de
rutas personales y patrones de credenciales en los registros sin coincidencias;
`git diff --check` satisfactorio. No se desactivaron pruebas. No se añadieron
archivos binarios de modelos ni cambios en `include/` (SDK).

### Resultados emparejados finales

| Zona | FPS sin overlay, swap 0 | FPS con overlay, swap 0 | Cambio | FPS swap 1 sin/con |
|---|---:|---:|---:|---:|
| hub | 284.4 | 279.7 | -1.6% | 57.4 / 57.4 |
| ecoenergy | 238.9 | 241.7 | +1.2% | 57.5 / 57.5 |
| aquabio | 292.2 | 294.7 | +0.9% | 57.1 / 56.9 |
| ecocycle | 382.5 | 380.2 | -0.6% | 57.4 / 57.4 |

Tres pares de 300 frames sin VSync solicitado, alternando el orden; un par
con VSync solicitado por zona. El contador de esta prueba usa texto fijo para
aislar el coste del dibujo; el recorrido de aplicación mide también muestreo y
actualización de texto. Las diferencias pequeñas no acreditan una mejora de
rendimiento. Se conserva la dispersión de cada pasada en el CSV, sin descartarla.
[CSV completo](evidence/cross-platform-fps/overlay-performance.csv).
[Registro gráfico final](evidence/cross-platform-fps/gpu-check.txt).
Las 12 combinaciones zona/resolución aprobaron; F3 restauró el framebuffer
exactamente y no hubo cambios de píxeles fuera de la región del contador.


## Validación gráfica adicional de instalaciones

Los tres smoke tests originales completaron con salida 0 y `Clean exit` en la
aplicación real Linux/Wayland, framebuffer concedido 951×1023:

| Comando | Interacciones | Animaciones | Resultado |
|---|---:|---:|---|
| `--ecoenergy-smoke` | 6 | 8 | PASS |
| `--aquabio-smoke` | 7 | 8 | PASS |
| `--ecocycle-smoke` | 7 | 8 | PASS |

Comprobaron selección, manipulación y movimiento mediante framebuffer, además
de los mecanismos CPU ya probados. Registros completos:
[EcoEnergy](evidence/cross-platform-fps/installation-energy.txt),
[AquaBio](evidence/cross-platform-fps/installation-aqua.txt),
[EcoCycle](evidence/cross-platform-fps/installation-cycle.txt).
Los ~60 FPS de sus bucles de 600 frames corresponden a swap solicitado 1 y no
se usan como benchmark causal del overlay.

Comparación exacta antes/después de cuatro vistas de entrada: **0 píxeles RGB
modificados en los 744633 píxeles de escena por vista**, excluyendo las primeras
240 filas donde cambiaron el HUD y FPS. Las resoluciones coinciden. Esto respalda
que iluminación, materiales, cielo y texturas conservan su salida en esas vistas
Intel/Mesa; no certifica otros controladores ni todas las posiciones.
[Detalle](evidence/cross-platform-fps/scene-comparison.txt).

**Aviso de almacenamiento:** los PPM/CSV de los smoke tests existentes conservados
en `/tmp/ecosphere-*` ocupan conjuntamente ~2.23 GiB al cierre, fuera del árbol Git.
Se informó al usuario al superar 2 GiB. No se borraron ni se publicaron esos PPM:
se publicaron únicamente PNG seleccionados y registros/CSV necesarios. El árbol
Git sigue por debajo de 2 GiB y su crecimiento ronda 174 MiB. La limpieza de los
temporales permanece pendiente de autorización explícita.
