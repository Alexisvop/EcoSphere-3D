# AquaBio — recorrido y composición espacial

## Alcance

Rama `feature/aquabio-layout-polish`, creada desde `origin/main` actualizado
(`72cd734`, PR #2). Árbol inicialmente limpio. Solo cambia AquaBio y sus fixtures,
plano y evidencias. Plaza, EcoEnergy, EcoCycle, recursos compartidos, shaders,
materiales, iluminación, GLB, Blender y SDK permanecen intactos.

## Auditoría y decisiones

La panorámica inicial mostraba máquinas concentradas en el fondo occidental,
precipitación separada del inicio del relato, muestras pequeñas sueltas y amplios
vacíos frontales. Desde la entrada se cruzaban visualmente paneles situados a
diferentes profundidades. La respuesta es reubicar lo existente, no añadir relleno.

| Subzona | Organización y motivo |
|---|---|
| Captación / precipitación | Nube y ciclo pasan del fondo al frente izquierdo, junto al pórtico. Techo, canaleta y rejilla avanzan hacia ese ámbito para abrir el relato del agua. |
| Almacenamiento / conducción | Cisterna, tuberías, válvula, indicador y bomba forman la secuencia occidental. El impulsor manipulable queda junto a su bomba. |
| Tratamiento / filtración | Sedimentación, carbón, arena, UV y tanque tratado se escalonan entre el oeste y el laboratorio. Arena/UV al fondo y carbón/tanque en la fila anterior separan las líneas de lectura desde la entrada. Los cartuchos se agrupan con los filtros. |
| Medición | Muestreo se acerca al eje central derecho, en vez de quedar aislado hacia el perímetro. Su cámara de comprobación acompaña el traslado. |
| Humedal / ecosistema | Vaso y vegetación avanzan juntos hacia el recorrido oriental. El pez se acerca al borde visitable; los árboles existentes enmarcan humedal y biodiversidad. Se mantienen volumen y escala del humedal. |

El antiguo extremo central sin una estación próxima se convierte en un ramal de
laboratorio: `AQUA-003-path-2` gira 90° y conecta con el eje a z=-9. El circuito
occidental queda entre captación y tratamiento; los tres tramos orientales rodean
el frente de observación del humedal. Se conservan **diez caminos conectados**,
con ancho útil **3.8 m** en eje/laboratorio y **2.8 m** en ramales secundarios.

Los letreros integrados conservan su orientación de lectura +Z. Se ordenan sus
posiciones y profundidades, sin girar rótulos contra sus mecanismos ni cambiar
las mallas. La lectura cercana se comprueba desde once puntos a altura de 1.7 m.
No se promete que todos los letreros sean legibles simultáneamente desde cualquier
punto: el recorrido conserva la observación por estaciones.

[Registro de los 46 elementos movidos](evidence/aquabio-layout-polish/moved-elements.csv)
con coordenadas, cuaterniones, escalas antes/después y motivo por instancia.
Las únicas escalas de instancia modificadas son las longitudes de dos caminos;
no se escalan equipos ni vegetación.
[Plano actualizado](assets/AQUABIO_PLAN.svg).

## Evidencia comparable

Las tres parejas usan **las mismas cámaras, objetivos, resolución real 951×1023,
renderizador y materiales**. Los PNG proceden de `glReadPixels`, convertidos sin
pérdida desde PPM, sin retoque de imagen.

| Vista | Antes | Después |
|---|---|---|
| Panorámica aérea | [Antes](evidence/aquabio-layout-polish/before/overview.png) | [Después](evidence/aquabio-layout-polish/after/overview.png) |
| Desde la entrada | [Antes](evidence/aquabio-layout-polish/before/entry.png) | [Después](evidence/aquabio-layout-polish/after/entry.png) |
| Interior / humedal | [Antes](evidence/aquabio-layout-polish/before/wetland.png) | [Después](evidence/aquabio-layout-polish/after/wetland.png) |

Se añade una vista [frontal del pórtico](evidence/aquabio-layout-polish/after/arrival.png).
Las capturas «antes» reutilizan los PPM reales de la validación anterior de este
clon, evitando repetir cientos de imágenes. Se comprobó mediante Git que entre
ese layout (`1e8e334`) y el main actual (`72cd734`) no cambiaron AquaBio, su smoke,
los GLB, ZoneSettings, shaders ni renderer. La línea base de pruebas relevantes
sí se ejecutó de nuevo antes de editar: **7/7 aprobadas**.
[Registro inicial](evidence/aquabio-layout-polish/baseline-tests.txt).
[Registro de cámaras inicial](evidence/aquabio-layout-polish/before/views.csv).

## Qué se preserva y por qué cambian los fixtures

- **28 modelos, 52 instancias propias**, más las tres instancias compartidas de
  AquaBio sin cambios: ningún modelo o instancia se añade o elimina.
- **7 interacciones** con los mismos IDs, modelos, operaciones y asignaciones.
- **8 animaciones** (4 simples, 2 keyframe, 2 procedurales), con parámetros,
  velocidades, amplitudes y relojes intactos. Los montajes se trasladan juntos.
- Cámara inicial, portal de entrada y retorno H, controles y contador FPS.
- Iluminación, cielo, materiales, geometría y estilo visual existentes.

`AQUABIO_INSTANCES.csv` se exporta con `aquabio_tests --write-instances` desde los
descriptores reales; no se genera a mano. Conserva IDs, rutas y flags exactos.
El plano se genera con el validador espacial desde las AABB importadas.
Los tres consumidores de evidencia actual (CMake inventario/espacio y prueba C++
de matrices) apuntan a la carpeta nueva. Las evidencias históricas de `phase6b`
se conservan byte a byte. No se regeneran hashes, catálogos de modelos ni evidencias
históricas para acomodar el cambio.

Las matrices capturadas t=0 y t=2 se extraen del smoke real. Para el fixture de
AquaBio se conservan sus **104 filas**; las seis filas SHARED capturadas se omiten
porque su estado se valida por separado y el contrato existente exige 52×2.
No se cambian tolerancias, conteos esperados ni aserciones de las pruebas.

## Reproducción

```bash
cmake --preset linux-dev
cmake --build --preset linux-dev --parallel 2
./build/linux-dev/ecosphere --aquabio-smoke build/linux-dev/aquabio-layout-polish-work/reviewed
ctest --preset linux-dev --output-on-failure
```

La sesión gráfica es Linux/Wayland, Intel Iris Xe, Mesa 26.2.6, OpenGL 4.6 Core,
GLFW 3.5.1; GCC 16.2.1 y CMake 4.4.5. El compositor no concedió los 1280×720
solicitados: se informa el framebuffer real de 951×1023. Falta validación nativa
Windows y de otros controladores.

## Almacenamiento y límites

Las pruebas conservan PPM y CSV sin registrarlos en Git; solo se versionan siete
PNG seleccionados y los registros/fixtures necesarios. Una ejecución intermedia
falló por cuota de `/tmp` (`basic_ios::clear: iostream error`), no por una aserción
de geometría. Se preservó y trasladó íntegro el directorio temporal propio al
build existente y se repitió la prueba allí. No se borraron archivos ni se hicieron
limpiezas automáticas. Las iteraciones y la ejecución incompleta siguen conservadas.

La auditoría usa AABB conservadoras y líneas de lectura muestreadas, no colisiones
exactas de triángulos ni una simulación de fluidos. La composición se verificó en
las vistas publicadas; la valoración estética final corresponde al director del
proyecto. No se crean PR ni se hace merge. La rama queda local: no se solicitó
publicar esta rama y la autorización de push anterior se limitaba a la rama de FPS.


## Resultado final Linux

- Configuración y compilación aprobadas en el build existente.
- **35/35 CTest** en 30.13 s, sin desactivar pruebas ni suavizar expectativas.
  Incluye AquaBio assets/inventario/espacio, navegación de zonas, controles de
  visitante, integración de módulos, animaciones, recursos compartidos y SDK.
  [Salida completa](evidence/aquabio-layout-polish/final-ctest.txt).
- Smoke gráfico final: **PASS**, siete interacciones y ocho animaciones visibles;
  capturas con profundidad válida y sin errores OpenGL; salida limpia.
  [Registro](evidence/aquabio-layout-polish/after/graphics-smoke.txt).
- Auditoría espacial: 52 bases + 1600 muestras animadas, 200 pasos de ocho
  mecanismos; diez caminos conectados; separación mínima de estaciones **3.70 m**
  (requisito >=2.5 m); once puntos de lectura despejados y 120 cámaras del recorrido
  sin entrar en AABB de objetos. Los ejemplos negativos del validador también
  rechazan camino desconectado, ancho incorrecto y objeto fuera del terreno.
  [Informe](evidence/aquabio-layout-polish/after/spatial-audit.json).
- Se compararon contra origin/main los IDs, recursos, flags interactivos y familias
  de animación de las 52 instancias: idénticos. Únicamente cambian sus poses y dos
  longitudes de camino.

No se interpreta el bucle de 600 frames a ~60 FPS con swap solicitado 1 como una
medida causal de mejora de rendimiento: el objetivo de esta rama es la composición.
La validación Windows y el juicio estético final del director quedan pendientes.
El directorio de trabajo de capturas conservadas hace que el árbol local supere
2 GiB; esas capturas completas permanecen ignoradas bajo build. No se limpiaron.
