# Visibilidad final de las seis texturas

Diagnóstico registrado antes de modificar recursos: HEAD
`014d2461968c09e7ad29f2fa713cf4f8c07fa2c4`, main verificado
`0732ad343434f2983f2d98a2454dc516b09488b2`, rama
`fix/presentation-textures-sync`, árbol inicialmente limpio y remoto coincidente.
Se ejecutó fetch y se comprobó la relación ancestral. No se creó rama ni worktree.

El [diagnóstico inicial](evidence/presentation-textures-visibility/initial-diagnosis.json)
registra los hashes anteriores, estadísticas de los PNG y UV por material.
Los 14 GLB originales extraídos de main coinciden con `original_sha256` del
manifiesto. Se conservaron copias verificadas A/main y B/textura anterior en
`build/presentation-textures-visibility/`, sin sobrescribir la evidencia histórica.

Los PNG anteriores son neutros, casi blancos: desviación estándar de solo
2,68–7,63 códigos sRGB. El agua varía entre 227–255; madera, 226–246; metal,
238–254; pintura, 237–252. La textura multiplica el color base en RGB lineal y
el resultado vuelve a sRGB; no cambia los factores del material. La iluminación
y la mezcla con el cubemap pueden reducir aún más la lectura del metal.

Los samplers ya usan LINEAR / LINEAR_MIPMAP_LINEAR y REPEAT, correctamente
asociados a TEXCOORD_0. No hay fallo de carga. Las líneas de alta frecuencia
(24 ciclos por tile en metal, detalles muy estrechos en otros patrones) se
promedian al alejar la cámara. La solución requiere bandas más anchas y detalles
en varias escalas, además de contraste moderado.

La proyección original es plana, de aproximadamente cuatro metros por repetición:
u=x/4; v=(y−z)/4 en AquaBio/EcoCycle y v=(y+z)/4 en EcoEnergy, con offset en esta
última. Algunas caras laterales tienen UV de área cero y solo muestran una línea
del patrón. El dato por material está en el diagnóstico. No se modificarán UV,
geometría, shaders, parámetros de material ni iluminación para resolverlo.

El proyecto ocupaba inicialmente 7.341.543.424 bytes asignados, incluidos los
builds existentes. El umbral de aviso corresponde al crecimiento de esta tarea,
no a ese tamaño previo. No se realizarán limpiezas automáticas.

Se ajustaron las seis texturas, sin cambiar su tamaño de 128×128, samplers,
materiales asociados ni el número de imágenes embebidas (21). Se conservaron
las celdas solares, metallic, roughness, colores base, alfa, nodos y todos los
datos originales. No se reexportó Blender ni se descargaron recursos.

| Textura | Parámetros anteriores | Parámetros nuevos | Rango sRGB nuevo, canal G |
|---|---|---|---|
| Agua | Base 235, onda de 3 ciclos, cresta estrecha de potencia 12 | Base 224, onda deformada de 2 ciclos y modulación secundaria; cresta y valle suaves gaussianos; tinte RGB −3/0/+3 | 175–255 |
| Madera | 8 ciclos horizontales, veta de baja amplitud | 32 ciclos para los postes de 12 cm, veta ancha de potencia 4 y detalle secundario; no cambia el color base | 168–240 |
| Concreto | Cuadrícula de 2 m, junta −24 códigos y variación ±3 | Junta cada 4 m, −14 códigos, manchas minerales suaves en varias escalas y detalle fino de menor amplitud | 205–251 |
| Metal | 24 ciclos, amplitud 5; banda secundaria de amplitud 3 | Banda de 6 ciclos, amplitud 17, más cepillado fino de 18 ciclos y variación lateral suave | 200–252 |
| Pintura | Variación máxima muy pequeña y detalle estrecho | Variación amplia de 2/3 ciclos más acabado fino discreto; RGB neutro | 212–251 |
| Cinta | 8 ciclos, surco estrecho de potencia 12 y amplitud 27 | 6 nervaduras más anchas, potencia 2 y amplitud 142, con borde secundario; RGB neutro | 89–249 |

Las fórmulas exactas están en `tools/apply_presentation_textures.py`. Hubo dos
iteraciones acotadas: la segunda corrigió la escala de la madera para los postes,
ensanchó los surcos de la cinta y suavizó el moteado del concreto. No se siguió
aumentando indefinidamente el contraste. Las fórmulas siguen siendo propias,
deterministas y periódicas; no simulan nuevos fenómenos de iluminación.

| Zona | Modelos afectados, mismas asignaciones que antes |
|---|---|
| AquaBio | AQUA-001, AQUA-003, AQUA-010, AQUA-023, AQUA-027 |
| EcoEnergy | ENERGY-001, ENERGY-003, ENERGY-004, ENERGY-005, ENERGY-010 |
| EcoCycle | CYCLE-001, CYCLE-005, CYCLE-007, CYCLE-023 |

El [manifiesto actual](../assets/common/textures/presentation/manifest.json)
enumera cada material y su textura. No se añadieron asignaciones nuevas.

La regeneración usa `--regenerate`: primero valida el estado actual completo,
extrae los 14 originales del commit main verificado y comprueba todos sus hashes
antes de escribir. Genera cada resultado en memoria desde esos originales, nunca
desde un GLB texturizado. Los tres inventarios y tres reportes de exportación
actualizan hashes, tamaños, procedencia y enlaces a las capturas nuevas por modelo.
El manifiesto conserva los hashes originales y registra el nuevo resultado.

[Integridad individual de los 14 GLB](evidence/presentation-textures-visibility/integrity.json).
La verificación reconstruye el JSON original y coteja el bloque binario original
completo: posiciones, índices, normales y UV permanecen idénticos. También se
conservan los materiales no incluidos y las imágenes solares/diagramas originales.
Los 132 archivos protegidos permanecen intactos, incluidos shaders, SDK, cámaras,
navegación, controles, HUD/FPS, animaciones, interacciones, Plaza y fuentes Blender.

[Idempotencia comprobada](evidence/presentation-textures-visibility/idempotency.json):
dos regeneraciones producen exactamente los mismos bytes, sin acumular imágenes.
`--check` conserva tanto bytes como fechas de modificación. La nueva prueba de
regeneración usa un árbol temporal, comprueba repetición y rechazo de originales
o PNG corruptos antes de escribir; no introduce Git como requisito de CTest.
El comando de regeneración sí usa el Git ya existente para comprobar procedencia.

Se capturaron **63 vistas de escena**: siete encuadres × tres distancias × tres
estados. Además hay 42 vistas de modelo aislado para documentar los 14 GLB de
cada estado. Todas usan 1280×720, Blinn-Phong, luces originales de cada zona y
pose inicial t=0. El verificador GPU exige igualdad de las luces y matrices de
colocación entre estados. Los tres estados usan el mismo ejecutable, renderer,
shader y cubemap; únicamente cambia la carpeta de GLB.

- A/main: `0732ad3`, sin las seis texturas nuevas; conserva las texturas solares
  y educativas que ya formaban parte de main.
- B/anterior: `014d246`, conservado en Git y en una copia verificada de assets.
- C/mejorada: recursos producidos por este pulido.

[Catálogo completo A/B/C](evidence/presentation-textures-visibility/COMPARISONS.md),
[cámaras exactas](evidence/presentation-textures-visibility/cameras.csv) y
[hashes de los 105 framebuffers](evidence/presentation-textures-visibility/capture-integrity.json).
Todos los PNG se decodificaron y compararon contra los RGB de `glReadPixels`,
sin redimensionamiento, retoque, exposición ni alteración de las comparaciones.
Los PPM fuente se conservan en `build/presentation-textures-visibility/final/`.
Las hojas reducidas usadas para inspección no sustituyen estos originales.

| Encuadre | Lectura visual del resultado |
|---|---|
| Humedal | Ondas reconocibles en el agua a distancia normal y media; detalle claro de cerca. Mejora visible. |
| Tratamiento | Agua más legible desde el lado abierto del sedimentador y variación del pavimento visible. La pared oculta el agua desde el lado cerrado. |
| Refugio de polinizadores | Vetas más definidas de cerca en postes y aros. La gran placa de fondo es `Aqua_sand`, fuera de la asignación de madera; permanece intacta. No se logra una veta claramente legible a distancia normal. |
| Solar | Pavimento con variación visible; las celdas solares permanecen iguales. El cepillado de los soportes finos sigue siendo débil desde navegación. |
| Pavimento y metal | Concreto reconocible; el acabado de metal es tenue incluso en algunas caras cercanas. No se declara un acabado cepillado claramente legible a distancia normal. |
| Cinta | Los surcos se distinguen mejor de cerca, pero la cinta sigue siendo poco legible desde navegación y a distancia media. Las franjas verdes son geometría original, no evidencia del patrón nuevo. |
| Contenedores | Variación superficial discreta, más apreciable en el amarillo a distancia media/cercana. Colores de clasificación conservados; desde lejos la pintura sigue siendo sutil. |

**Aceptación visual parcial.** Agua y concreto mejoran claramente. No se declara
que las seis texturas satisfagan el criterio de lectura inmediata a distancia
normal. La variación numérica de píxeles no se usa como sustituto de ese criterio.
Los postes, aros y soportes ofrecen pocos píxeles y algunas caras tienen UV de
área cero; los mipmaps promedian los detalles. En la cinta oscura, el albedo
lineal es solo 0,035–0,050 y la contribución especular dieléctrica del shader no
se multiplica por el albedo. Esa contribución reduce el contraste final de los
surcos incluso con una textura más contrastada. No se alteran iluminación,
specular, metallic, roughness, UV, geometría o shaders para disimular ese límite.

La prueba GPU A/B/C también mide rendimiento con B y C alternados en orden
B/C, C/B, B/C. Cada pasada tiene 30 frames de calentamiento y 200 frames medidos
por encuadre normal. Se solicita VSync cero y se usa `glFinish` tras cada frame;
el tiempo incluye dibujo, presentación y espera GPU. Las capturas se hacen fuera
del intervalo medido. Se reporta mediana de tres pasadas, incluyendo el p95 de
cada pasada. GPU: Intel Iris Xe (ADL GT2), OpenGL 4.6, Mesa 26.2.6-arch1.1, X11.

| Vista | FPS B | FPS C | Variación | Media B/C, ms | p95 B/C, ms |
|---|---:|---:|---:|---|---|
| Humedal | 41,90 | 41,88 | −0,04% | 23,87 / 23,88 | 26,08 / 26,19 |
| Tratamiento | 42,12 | 41,73 | −0,93% | 23,74 / 23,97 | 26,13 / 26,00 |
| Madera | 42,20 | 41,80 | −0,94% | 23,70 / 23,92 | 26,18 / 26,16 |
| Solar | 40,57 | 40,49 | −0,19% | 24,65 / 24,70 | 27,13 / 27,46 |
| Pavimento/metal | 40,43 | 40,40 | −0,07% | 24,73 / 24,75 | 27,43 / 27,17 |
| Cinta | 43,24 | 42,68 | −1,28% | 23,13 / 23,43 | 25,62 / 25,79 |
| Contenedores | 42,67 | 42,76 | +0,22% | 23,44 / 23,38 | 25,65 / 25,65 |

[Pasadas completas](evidence/presentation-textures-visibility/performance.csv) y
[resumen calculado](evidence/presentation-textures-visibility/performance-summary.csv).
No se observa una pérdida importante entre B y C en este método. Las variaciones
pequeñas no se atribuyen exclusivamente a las texturas. Estas tasas sincronizadas
no son directamente comparables con los antiguos benchmarks de throughput ni
garantizan 60 FPS en la aplicación o en otra máquina. Tampoco representan una
prueba del equipo/proyector de exposición o una nueva validación Windows.

Se ejecutaron `cmake --preset linux-dev`, la compilación completa incremental
del mismo build compatible y CTest: **39/39 correctas**, manteniendo los 38
nombres anteriores y añadiendo `presentation_textures_regeneration` (tres casos).
La nueva prueba gráfica es optativa para conservar CTest sin requisito de pantalla.

[Configuración](evidence/presentation-textures-visibility/configure.txt),
[compilación](evidence/presentation-textures-visibility/build.txt),
[CTest](evidence/presentation-textures-visibility/ctest.txt).

| Verificación GPU nueva | Resultado |
|---|---|
| A/B/C y rendimiento alternado | [TEXTURE VISIBILITY GPU PASS](evidence/presentation-textures-visibility/abc-gpu.txt), igualdad de luces y poses verificada |
| Phong, Blinn-Phong, Fresnel y cubemap | [SHADING GPU PASS](evidence/presentation-textures-visibility/shading.txt), cuatro zonas y regresión contra shader de referencia |
| HUD y FPS | [LIGHTING HUD GPU PASS](evidence/presentation-textures-visibility/hud.txt), cuatro zonas a 1280×720, 640×360 y 320×240 |
| Teclado | [SHADING NATIVE PASS](evidence/presentation-textures-visibility/native-input-result.txt), F3/F4/F5, reset y controles 1–6 |
| Navegación y descarga de recursos | [NAVIGATION GPU PASS](evidence/presentation-textures-visibility/navigation-app.log), las cuatro zonas y visitas repetidas |
| Interacciones y animaciones | [INTERACTION SMOKE PASS](evidence/presentation-textures-visibility/interactions.txt), además de los casos CPU de CTest |

No se repitieron los tres smokes completos individuales de instalaciones; se
comprobaron los 14 assets en el verificador A/B/C, los comportamientos en CTest,
las cuatro zonas en navegación y el smoke gráfico de interacción/animación.
No hay cobertura anterior eliminada ni resultados esperados relajados.

La [medición de tamaño](evidence/presentation-textures-visibility/footprint.json)
tras las pruebas y antes del commit final registra **7,789 GiB** asignados en el
proyecto, incluidos builds históricos y framebuffers fuente conservados. El
crecimiento de esta tarea es **0,951 GiB**, inferior al umbral de aviso de 2 GiB.
No se borraron builds ni se hicieron limpiezas automáticas. No se crearon builds
de código redundantes: las carpetas A/B contienen solamente assets de referencia.

La intervención sigue en `fix/presentation-textures-sync`, sin nueva rama ni
worktree. Los tres commits previos permanecen intactos y `014d246` sigue siendo
ancestro. Main remoto permanece en `0732ad3` y main local en `1e8e334`; la rama
histórica permanece en `74f3b2e`. Las [referencias antes de publicar](evidence/presentation-textures-visibility/pre-push-refs.txt)
se cotejaron con la auditoría inicial. No se modificaron licencias ni créditos.

La [lista de archivos](evidence/presentation-textures-visibility/changed-files.txt)
contiene únicamente las seis texturas, los 14 GLB, su manifiesto y documentación
de uso, seis inventarios/reportes, el generador, pruebas auxiliares/CMake y este
informe con evidencias nuevas. Los informes y capturas históricos de texturas y
sincronización se conservaron byte a byte. Se publica mediante push normal
exclusivamente en la rama autorizada, sin PR, merge, rebase, force push ni reset.
El mensaje de cierre indica el SHA final y confirma las referencias publicadas.

El trabajo queda detenido para auditoría externa, con el límite visual declarado,
sin mejoras adicionales fuera del alcance autorizado.
