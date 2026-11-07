# Acabado metálico final

Diagnóstico previo a cambios, rama `fix/presentation-textures-sync`, HEAD
`da1ed249f2c3fb64daed5228b8be45c0c0c92bfe`. Se verificaron directorio, árbol
limpio, remoto, fetch, igualdad del HEAD local/remoto y ascendencia de main
`0732ad343434f2983f2d98a2454dc516b09488b2`. Tamaño inicial asignado:
8.379.842.560 bytes. No se crean ramas ni worktrees.

El [diagnóstico por material](evidence/presentation-metal-finish/initial-diagnosis.json)
registra color base, metallic, roughness, specular/reflection efectivos, UV,
normales, texturas y hashes de los seis modelos afectados. `Energy_aluminum`
usa metallic 0,88 / roughness 0,24; `Energy_steel` y `Cycle_steel`, 0,82 / 0,30.
Specular=0,5 y reflection=1 son valores del motor, no parámetros importados de
estos GLB. El cargador admite baseColor, metallic y roughness, y rechaza extensiones
de material no soportadas. No se ampliará el cargador ni el SDK para esta tarea.

La textura anterior, neutra, varía entre 200–252 sRGB: bandas de seis ciclos en
v, cepillado secundario de 18 ciclos y variación lateral débil. Los UV existentes
proyectan aproximadamente cuatro metros por tile y colapsan en algunas caras.
Los soportes más finos ocupan pocos píxeles: un patrón principalmente vertical
no produce cepillado reconocible en todas sus orientaciones.

Phong y Blinn-Phong cambian la evaluación especular directa, conservando luces y
material. En metales, el color base texturizado influye tanto en el color especular
como en F0 del cubemap; no se trata de una nueva BRDF. La reflexión ambiental
estática y los lóbulos estrechos pueden ocultar detalles finos del albedo.
Fresnel-Schlick conserva su fórmula y su exponente. Se probará primero un patrón
con variación en ambas direcciones y líneas más finas superpuestas, con luces,
parámetros de material y shader intactos.

La intervención final cambia solamente el patrón de `metal.png`. Sigue siendo
RGB neutro, alfa 255 y 128×128, con los mismos filtros lineales/mipmaps y REPEAT.
El nuevo patrón combina bandas amplias en u y v con cepillado fino superpuesto;
así algunas caras con una coordenada UV constante conservan variación en la otra.
No introduce relieve, anisotropía ni un nuevo modelo de iluminación.

| Parámetro del patrón | Anterior B | Nuevo C |
|---|---|---|
| Base sRGB | 226 | 210 |
| Variación amplia | 6 ciclos en v, amplitud 17 | 4 ciclos en u, amplitud 20; 3 en v, amplitud 10 |
| Detalle fino | 18 ciclos, amplitud 5 | 24 ciclos en u, amplitud 12; 32 en v, amplitud 6 |
| Rango sRGB observado | 200–252 | 163–255 |
| Desviación estándar | 12,85 | 18,44 |
| PNG comprimido | 5.908 bytes | 14.490 bytes |

[PNG anterior](evidence/presentation-metal-finish/metal-before.png),
[PNG nuevo](evidence/presentation-metal-finish/metal-after.png) y
[estadísticas/hashes](evidence/presentation-metal-finish/texture-statistics.json).
Las fórmulas exactas permanecen en `tools/apply_presentation_textures.py`.

Primero se evaluó la textura sola, sin cambiar materiales. Después se probaron
solo dos variantes locales en copias del build, en vistas de soporte y banco:

| Variante aislada | Cambios propuestos | Decisión |
|---|---|---|
| R1 | Metallic 0,96 y roughness 0,40 en los siete materiales con `metal.png` | Rechazada: oscurece piezas y no mejora convincentemente la lectura media |
| R2 | Metallic original; roughness 0,50 para ensanchar el brillo satinado | Rechazada: diferencia tenue, sin mejora convincente a distancia media |

[Parámetros R1](evidence/presentation-metal-finish/material-candidate.json),
[parámetros R2](evidence/presentation-metal-finish/material-candidate-2.json).
Se conserva una captura representativa por ensayo:
[textura sola](evidence/presentation-metal-finish/texture-preview.png),
[R1](evidence/presentation-metal-finish/material-preview.png),
[R2](evidence/presentation-metal-finish/satin-preview.png).
Los ensayos cambiaron únicamente JSON de copias temporales, sin añadir imágenes,
reexportar Blender ni modificar los materiales del proyecto. El build final se
repobló desde los GLB del proyecto, descartando ambas variantes.

**No se aplicó ningún ajuste de propiedades de material.** Todos los factores
permanecen iguales a B/main: `Energy_aluminum`, metallic 0,88 y roughness 0,24;
`Energy_steel` y `Cycle_steel`, 0,82 y 0,30; specular 0,5, reflection 1 y colores
base originales. Las transparencias, materiales ajenos y texturas solares
también permanecen iguales. No se alteraron fórmulas, exponentes, luces o cubemap.

| Zona | Modelos regenerados | Materiales que usan el patrón nuevo |
|---|---|---|
| EcoEnergy | ENERGY-004 | Energy_aluminum y Energy_steel |
| EcoEnergy | ENERGY-005, ENERGY-010 | Energy_aluminum |
| EcoCycle | CYCLE-005, CYCLE-007, CYCLE-023 | Cycle_steel |

Son seis GLB y siete materiales concretos, no todos los metales de una zona.
La regeneración conserva el procedimiento desde main `0732ad3`, coteja los
hashes de los 14 originales antes de escribir y nunca incrusta sobre GLB ya
texturizados. Solo los seis resultados que contienen metal difieren de B.
Los otros ocho GLB texturizados y los otros 99 GLB físicos permanecen idénticos
en bytes. Los inventarios/reportes actualizan hashes, tamaños y capturas solo
para los seis modelos afectados; AquaBio queda intacta.

[Prueba de alcance](evidence/presentation-metal-finish/scope-proof.json) e
[integridad por modelo y textura](evidence/presentation-metal-finish/integrity.json).
Water, wood, concrete, painted y belt conservan exactamente sus SHA-256 iniciales.
Los fingerprints de geometría, UV, normales, jerarquías y propiedades de material
del manifiesto coinciden con los anteriores; también siguen coincidiendo los
132 archivos protegidos. No se modificaron SDK, animaciones, controles, HUD,
FPS, navegación, Plaza, licencias, créditos o documentación/evidencia histórica.

Se conservaron **90 PNG de framebuffer sin retoques**: 54 vistas de seis escenas
(normal/media/cercana × A/B/C), 18 comparaciones de iluminación y 18 vistas de
los seis modelos aislados. El verificador existente se amplió con un perfil
metálico, reutilizando build y renderer. Exige igualdad de luces y poses entre
estados; las cámaras y modos también se cotejaron por CSV. La resolución es
1280×720, animaciones en t=0. Cada PNG decodificado es idéntico al RGB leído por
`glReadPixels`; no hay exposición, redimensionamiento o retoque de la comparación.

- A: main `0732ad3`, sin las texturas procedurales adicionales.
- B: `da1ed249`, antes del ajuste metálico.
- C: patrón metálico nuevo, con propiedades de material originales.

[Catálogo A/B/C completo](evidence/presentation-metal-finish/COMPARISONS.md),
[cámaras/modos](evidence/presentation-metal-finish/cameras.csv),
[hashes de capturas](evidence/presentation-metal-finish/capture-integrity.json).
Los PPM fuente y ensayos acotados se conservan bajo
`build/presentation-metal-finish/`; no se añaden esos builds a Git.

La comparación de iluminación usa el mismo tornillo de banco, frontal y rasante,
en Blinn-Phong con Fresnel, Phong con Fresnel y Blinn-Phong sin reflejos ambientales.
Dentro de cada pareja A/B/C no cambia la cámara ni el estado de iluminación.
Frontal y rasante son cámaras distintas: no se atribuye a la textura la diferencia
entre ambas. Reflejos OFF desactiva la contribución ambiental completa, no
únicamente la variación angular de Fresnel. Los efectos anteriores se conservan;
no se presenta el nuevo patrón como una mejora de la fórmula de Schlick.

**Dictamen visual: parcial, sin aprobación general del criterio normal/media.**
El patrón tiene más variación y detalle apreciable de cerca en el tornillo de
banco y algunas caras metálicas. La lectura media sigue siendo débil; los
soportes solares finos no permiten reconocer con claridad el cepillado durante
navegación. No se aprueba por conteos de píxeles. La geometría plana, UV
proyectados/colapsados, pocos píxeles y mezcla con un cubemap de paisaje limitan
las señales de acabado. La reflexión existente no proporciona anisotropía ni
un prefiltrado físico por roughness. No se cambian geometría, UV, luces,
reflectancia global o renderer para forzar un resultado más llamativo.

El ensayo B/C de rendimiento alterna B/C, C/B y B/C. Cada vista normal usa 30
frames de calentamiento y 100 medidos por pasada, con VSync solicitado en cero
y `glFinish` por frame. Se toma la mediana de tres pasadas. Se registran tiempo
de dibujo/presentación/espera GPU, FPS y p95; las capturas quedan fuera del periodo
medido. GPU: Intel Iris Xe ADL GT2, Mesa 26.2.6-arch1.1, OpenGL 4.6, Linux/X11.

| Vista | FPS B/C | Media B/C, ms | p95 B/C, ms | Variación FPS |
|---|---|---|---|---:|
| Solar | 37,54 / 37,50 | 26,64 / 26,67 | 28,86 / 28,84 | −0,11% |
| Soporte | 37,76 / 37,99 | 26,48 / 26,32 | 28,89 / 28,51 | +0,62% |
| Estructuras | 37,51 / 38,05 | 26,66 / 26,28 | 28,98 / 29,16 | +1,43% |
| Cinta | 40,88 / 41,32 | 24,46 / 24,20 | 26,77 / 26,39 | +1,08% |
| Maquinaria | 41,45 / 41,37 | 24,12 / 24,17 | 26,19 / 26,49 | −0,21% |
| Banco | 41,34 / 42,37 | 24,19 / 23,60 | 26,36 / 26,18 | +2,49% |

[Pasadas](evidence/presentation-metal-finish/performance.csv) y
[resumen](evidence/presentation-metal-finish/performance-summary.csv).
No se observa una pérdida importante B/C. Las fluctuaciones pequeñas, incluidas
las subidas, no se presentan como efectos causales del patrón. Las tasas de este
método sincronizado no equivalen a throughput de la aplicación ni garantizan
60 FPS en el equipo/proyector de exposición.

Configuración y compilación completa del build Linux existente correctas.
CTest: **39/39**, con exactamente los 39 nombres anteriores. La prueba de
regeneración pasa cuatro casos, incluido el nuevo caso que comprueba que cambiar
solo metal conserva las otras cinco texturas, los modelos ajenos y todos los
materiales. No se relajaron expectativas ni verificaciones de integridad.

[Configure](evidence/presentation-metal-finish/configure.txt),
[build](evidence/presentation-metal-finish/build.txt),
[CTest](evidence/presentation-metal-finish/ctest.txt),
[idempotencia](evidence/presentation-metal-finish/idempotency.json).
Dos regeneraciones producen los mismos bytes; `--check` conserva bytes y mtimes.

| Regresión GPU nueva | Resultado |
|---|---|
| A/B/C y estados de luz | [TEXTURE VISIBILITY GPU PASS](evidence/presentation-metal-finish/metal-gpu.txt) |
| Phong, Blinn-Phong, Fresnel y cubemap | [SHADING GPU PASS](evidence/presentation-metal-finish/shading.txt) |
| HUD de iluminación y FPS | [LIGHTING HUD GPU PASS](evidence/presentation-metal-finish/hud.txt), cuatro zonas y tres resoluciones |
| Teclado, repetición aislada | [SHADING NATIVE PASS](evidence/presentation-metal-finish/native-input-retry-result.txt) |
| Navegación por las cuatro zonas | [NAVIGATION GPU PASS](evidence/presentation-metal-finish/navigation-app.log), visitas repetidas y salida limpia |
| Interacción y animación | [INTERACTION SMOKE PASS](evidence/presentation-metal-finish/interactions.txt), además de CTest |

**Incidencia conservada para auditoría:** el [primer ensayo nativo](evidence/presentation-metal-finish/native-input-result.txt)
falló con 14 filas frente a las 15 esperadas; su [log](evidence/presentation-metal-finish/native-input.txt)
no contiene la primera alternancia F5. La prueba original pasó al repetirse de
forma aislada, sin cambiar código de controles, test, pausas o expectativas.
El orden de foco/arranque es una hipótesis, no una causa demostrada. No se ocultó
el fallo ni se modificó lógica de entrada fuera de esta tarea artística; debe
revisarse en la auditoría de estabilidad.

Tamaño medido tras la validación, antes del commit: **8,580 GiB** asignados,
incluyendo builds existentes y PPM conservados. Crecimiento: **0,775 GiB**,
inferior al umbral de aviso de 1 GiB.
[Medición exacta](evidence/presentation-metal-finish/footprint.json).
No se hizo limpieza automática ni se generó otro build de código; las copias
A/B y los ensayos aislados contienen assets y capturas, no builds redundantes.

El trabajo se mantiene en `fix/presentation-textures-sync` y conserva
`da1ed249` como ancestro. Main remoto sigue en `0732ad3`, main local en `1e8e334`
y la rama histórica en `74f3b2e`. Las [referencias previas a publicar](evidence/presentation-metal-finish/pre-push-refs.txt)
se cotejan con la auditoría inicial. Solo se añaden un commit y un push normal
en la misma rama autorizada, sin reset, rebase, force push, PR, merge ni worktree.

La [lista de archivos](evidence/presentation-metal-finish/changed-files.txt)
incluye exclusivamente metal.png, seis GLB y su manifiesto, los inventarios y
reportes de EcoEnergy/EcoCycle, el generador, dos pruebas auxiliares y este informe
con evidencia nueva. No se tocaron otros materiales, informes históricos,
licencias o créditos. El mensaje de cierre identifica el SHA final publicado.

No se declara una nueva validación Windows. No se repiten los tres smokes
completos individuales de instalaciones: se conservan sus evidencias y se
comprueban assets, comportamiento y navegación con la suite y GPU indicadas.
Después de esta intervención no se inician nuevas mejoras artísticas. El trabajo
se detiene para auditoría externa con la limitación visual declarada.
