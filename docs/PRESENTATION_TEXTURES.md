# Texturizado selectivo para la exposición

Rama local: `feature/presentation-textures`. Base: `origin/main` consultado en el
repositorio, `f82c2881758555fb54bc9c4d41440bbee0b9bf95`. No se hizo fetch, push,
PR ni merge. El `main` local permanece en
`1e8e334f9482cfb78626a59179abd5e948d8c15c`.

Se incorporaron, con autorización expresa, los tres commits de iluminación que
estaban en la rama de trabajo. Sus nuevos identificadores son `a7a41fe`, `99048f0`
y `046dba7`; el árbol de `046dba7` es idéntico a la versión inicial `962cb9c`.
Las comparaciones de texturizado usan esa base, conservando Phong, Blinn-Phong y
Fresnel. Commit de implementación: `5ddf79d388ba742e7cb7cbda2382a691372ed952`.
El commit posterior contiene este informe, las evidencias y sus referencias en
los inventarios y el postproceso.

La intervención añade seis texturas originales de 128×128 a 14 GLB. No cambia
posiciones, geometría, normales, UV, nodos, animaciones, interacciones, navegación,
controles, cámaras, HUD, shaders ni contratos del SDK. Plaza no recibió cambios
artísticos. Se conservan las celdas solares propias y los diagramas existentes.

| Zona | Modelo | Material → textura |
|---|---|---|
| AquaBio | AQUA-001, plataforma | Aqua_stone → concrete |
| AquaBio | AQUA-003, sendero | Aqua_stone → concrete |
| AquaBio | AQUA-010, sedimentador | Aqua_water → water |
| AquaBio | AQUA-023, humedal | Aqua_water → water; Aqua_stone → concrete |
| AquaBio | AQUA-027, refugio de polinizadores | Aqua_bark → wood |
| EcoEnergy | ENERGY-001, plataforma | Energy_concrete → concrete |
| EcoEnergy | ENERGY-003, caminos | Energy_pavement → concrete |
| EcoEnergy | ENERGY-004, soportes solares | Energy_aluminum / Energy_steel → metal; Energy_concrete → concrete |
| EcoEnergy | ENERGY-005, módulo solar | Energy_aluminum → metal; celdas solares conservadas |
| EcoEnergy | ENERGY-010, gabinete de baterías | Energy_steel → painted; Energy_aluminum → metal; Energy_concrete → concrete |
| EcoCycle | CYCLE-001, plataforma | Cycle_ground / Cycle_stone → concrete |
| EcoCycle | CYCLE-005, contenedores | Cycle_blue / Cycle_yellow / Cycle_white → painted; Cycle_steel → metal |
| EcoCycle | CYCLE-007, cinta | Cycle_charcoal → belt; Cycle_steel → metal |
| EcoCycle | CYCLE-023, banco de reparación | Cycle_wood → wood; Cycle_steel → metal |

Los seis PNG están en [assets/common/textures/presentation](../assets/common/textures/presentation/README.md):
`water.png`, `wood.png`, `concrete.png`, `metal.png`, `painted.png` y `belt.png`.
Se generan con la biblioteca estándar de Python, sin descargas, terceros ni
dependencias nuevas. Su procedencia y régimen de arte propio están documentados
en esa carpeta, bajo el alcance de `LICENSE_SCOPE.md`.

El RGB neutro en sRGB multiplica el color base original; el alfa es siempre 255.
Se conservan todos los factores de material, incluyendo metallic, roughness,
transparencia y opciones unlit/doubleSided. Se usa repetición, filtrado lineal y
mipmaps del renderer existente. No se añaden normal maps, animaciones de agua ni
modificaciones de iluminación. Los patrones son detalles estilizados; no simulan
propiedades físicas ni rendimiento de los equipos educativos.

Los PNG fuente suman 7.506 bytes. Los GLB aumentan **28.196 bytes** en total.
Hay 21 copias de imágenes embebidas, repartidas en 6/8/7 entre AquaBio/EcoEnergy/
EcoCycle, para conservar el empaquetado offline. Se reutilizan los mismos seis
patrones y, dentro de cada GLB, una textura por patrón. No se cambia la caché del
motor para deduplicar imágenes entre GLB. El máximo añadido por zona es de ocho
imágenes: 512 KiB de RGBA decodificado y aproximadamente 683 KiB de texels con
mipmaps, sin contar sobrecarga del driver. Son estimaciones de almacenamiento,
no mediciones de VRAM.

El [manifiesto](../assets/common/textures/presentation/manifest.json) identifica
cada material, hash original/final, bloque binario original y estructura JSON
preservada. La verificación demuestra igualdad byte a byte de los bloques
originales, incluyendo geometría y UV, y de todos los campos JSON originales
salvo las nuevas asociaciones de textura y longitud del buffer. Los originales
se cotejaron además contra `046dba7`, no solo contra el manifiesto generado.

Archivos modificados: los 14 GLB anteriores; los tres `*_INVENTORY.csv` y tres
`*_EXPORT_REPORT.json` de las instalaciones, actualizados únicamente en hashes,
tamaños, procedencia y enlaces a capturas nuevas; `CMakeLists.txt`, para registrar la prueba de integridad;
y `src/app/AquaBioSmoke.cpp` / `src/app/EcoCycleSmoke.cpp`, para sustituir ceros
constantes por el conteo real de texturas en sus informes. Esos cambios de los
smokes solo corrigen estadísticas, sin alterar dibujo, acciones ni animaciones.

Archivos nuevos: seis PNG, su README y manifiesto,
`tools/apply_presentation_textures.py`, este informe y
`docs/evidence/presentation-textures/`. La [lista exacta de implementación](evidence/presentation-textures/implementation-files.txt)
y la [lista de evidencias](evidence/presentation-textures/evidence-files.txt)
permiten revisar todos los archivos sin incluir cambios históricos de iluminación.
Los hashes de [132 archivos protegidos](evidence/presentation-textures/protected-files.json)
confirman que permanecen idénticos a `046dba7`, incluyendo Plaza y fuentes Blender.

La configuración y [compilación final](evidence/presentation-textures/build.txt)
terminaron correctamente con `linux-dev`, Debug, Ninja y las dependencias ya
instaladas. [CTest inicial](evidence/presentation-textures/baseline-ctest.txt):
**37/37**. [CTest final](evidence/presentation-textures/ctest.txt): **38/38**,
incluyendo `presentation_textures_integrity`. No se instalaron paquetes.

Los smokes nativos a 1280×720 pasan en las tres instalaciones: importación de todos
sus modelos, siete/seis/siete interacciones y dieciocho/ocho/ocho animaciones,
respectivamente. Sus logs, matrices, cámaras e informes están en las carpetas
[antes](evidence/presentation-textures/before/) y
[después](evidence/presentation-textures/after/). En AquaBio y EcoCycle, las listas
de cámaras/tiempos y matrices de animación antes/después son idénticas byte a byte.
EcoEnergy usa las cámaras fijas del mismo smoke, cuyo código no cambió.

La prueba [HUD GPU](evidence/presentation-textures/hud.txt) pasa las cuatro zonas a
1280×720, 640×360 y 320×240: visibilidad F3/F4, indicadores, reset, estado OpenGL y
cero cambios fuera del panel. La prueba de [entrada nativa](evidence/presentation-textures/native-input.txt)
comprueba F5 mantenida, selección con HUD oculto, F3/F4, reset y teclas 1–6.
Las capturas provienen de `glReadPixels`; los PNG son conversiones sin pérdida,
sin retoque ni generación de imágenes.

La [prueba final de sombreado](evidence/presentation-textures/shading.txt) pasa
en demo y las cuatro zonas. El Blinn actual y el shader de referencia producen
cero diferencias con los materiales finales. Se conservan las comparaciones
Phong, Fresnel frontal/rasante, cubemap y redimensionamiento. Las entradas de
las tres instalaciones están también guardadas en `entry-BLINN-PHONG.png` y
`entry-PHONG.png`, antes y después. La entrada de Plaza con HUD es
[idéntica píxel por píxel](evidence/presentation-textures/plaza-framebuffer.txt).

| Vista, cámara idéntica | Antes | Después |
|---|---|---|
| AquaBio, humedal | [PNG](evidence/presentation-textures/before/aquabio/wetland.png) | [PNG](evidence/presentation-textures/after/aquabio/wetland.png) |
| AquaBio, biodiversidad | [PNG](evidence/presentation-textures/before/aquabio/biodiversity.png) | [PNG](evidence/presentation-textures/after/aquabio/biodiversity.png) |
| EcoEnergy, zona solar | [PNG](evidence/presentation-textures/before/ecoenergy/solar-zone.png) | [PNG](evidence/presentation-textures/after/ecoenergy/solar-zone.png) |
| EcoEnergy, baterías | [PNG](evidence/presentation-textures/before/ecoenergy/battery-zone.png) | [PNG](evidence/presentation-textures/after/ecoenergy/battery-zone.png) |
| EcoCycle, clasificación | [PNG](evidence/presentation-textures/before/ecocycle/classification.png) | [PNG](evidence/presentation-textures/after/ecocycle/classification.png) |
| EcoCycle, alimentación y cinta | [PNG](evidence/presentation-textures/before/ecocycle/feeding.png) | [PNG](evidence/presentation-textures/after/ecocycle/feeding.png) |
| EcoCycle, taller | [PNG](evidence/presentation-textures/before/ecocycle/workshop.png) | [PNG](evidence/presentation-textures/after/ecocycle/workshop.png) |

La comparación de vista fija usa el ejecutable existente `fps_graphics_tests`,
las mismas cámaras de entrada, 1280×720, sRGB, VSync desactivado y HUD FPS activo.
Se toma la mediana de tres pasadas de 300 frames por zona. La carpeta de assets
de referencia contiene los GLB originales y shaders idénticos; la carpeta final
contiene los GLB texturizados. GPU: Intel Iris Xe (ADL GT2), Mesa 26.2.6-arch1.1,
OpenGL 4.6, GLFW 3.5.1, Linux/X11.

El contador visible en las capturas `entry-hud.png` usa la muestra sintética de
64 FPS del verificador existente para poder comparar el HUD de forma exacta.
Los FPS de rendimiento reales están en los CSV, no en ese texto de las capturas.

| Zona | FPS antes, mediana | FPS después, mediana | Variación |
|---|---:|---:|---:|
| EcoEnergy | 394,27 | 398,40 | +1,05% |
| AquaBio | 492,85 | 477,20 | −3,17% |
| EcoCycle | 593,40 | 573,38 | −3,37% |

[Datos completos antes](evidence/presentation-textures/performance-before.csv),
[después](evidence/presentation-textures/performance-after.csv) y
[resumen](evidence/presentation-textures/performance-summary.csv).
El dato de Plaza no se usa para estimar impacto: no cambió y la primera pasada
de referencia coincidió con trabajo CPU de CTest. Las tres instalaciones se
midieron después de terminar ese trabajo. No se interpreta la pequeña subida
de EcoEnergy como una optimización.

También se repitieron de forma secuencial los recorridos animados de las tres
instalaciones, con el ejecutable principal: 600 frames, 1280×720 y VSync solicitado
en cero, sin CTest ni otra prueba GPU simultánea. Cada pareja usa el mismo
ejecutable y los mismos estados; solo cambia la carpeta de GLB. La lectura de
framebuffer y grabación quedan fuera del intervalo de medición del smoke.

| Recorrido animado | FPS antes | FPS después | p95 antes, ms | p95 después, ms |
|---|---:|---:|---:|---:|
| EcoEnergy | 143,91 | 175,13 | 11,51 | 10,19 |
| AquaBio | 207,19 | 222,41 | 8,78 | 7,42 |
| EcoCycle | 246,16 | 282,70 | 7,47 | 5,30 |

[Resumen de recorridos](evidence/presentation-textures/route-performance.csv),
[ejecución](evidence/presentation-textures/route-validation.txt) y
[logs/métricas/frames](evidence/presentation-textures/routes/).
Esta repetición no reprodujo la caída de la primera medición exploratoria de
EcoEnergy. La diferencia entre sesiones demuestra variación del entorno; tampoco
se interpreta la subida de FPS como una mejora causada por las texturas.
No aparece una pérdida consistente en las pruebas controladas: la vista fija
cambia aproximadamente entre +1% y −3,4%, y los p95 finales del recorrido están
entre 5,30 y 10,19 ms, por debajo de los 16,67 ms correspondientes a 60 Hz.
Esto da margen en el equipo probado; no equivale a una garantía de fluidez para
cualquier cámara, carga o equipo de exposición.

Las medidas se refieren a esta GPU y estas cámaras, sin prueba ciega de percepción
ni garantía para otras máquinas. Los smokes funcionales incluyen además lectura
de framebuffer y grabación separadas; sus primeras métricas, algunas concurrentes
con pruebas CPU, son exploratorias y no sustituyen la comparación controlada.

Las fuentes `.blend` permanecen como originales de modelado: reexportarlas
sobrescribiría el acabado. El postproceso es reproducible desde los GLB originales
de `046dba7`; el script y README explican su aplicación y comprobación. No hay
carga de recursos remotos en ejecución. No se validó Windows ni el equipo o
proyector de la exposición, ni se hizo una auditoría general del repositorio.

El trabajo queda detenido para auditoría externa de ChatGPT, sin PR, push ni merge.
