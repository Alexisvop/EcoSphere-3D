# HUD de estado de iluminación

## Base y alcance

Implementado por autorización posterior en la rama existente
`feature/aquabio-layout-polish`, sobre
`b2232234f7a3f5b0f3f71c2a019717a42828c569`, coincidente con `origin/main` consultado.
Git estaba limpio; las mejoras de AquaBio estaban guardadas en `c0ce56f` y
`b223223`. No se cambió de rama, no se creó worktree y no se mezcló otro trabajo.
Se publicará exclusivamente la rama actual, conforme a esa corrección de alcance.

No cambian instalaciones, poses, modelos, texturas, Blender, shaders, materiales,
contratos SDK, navegación ni algoritmos FPS. Solo se añade UI/entrada de aplicación,
pruebas, documentación y evidencias. El anclaje FPS pasa de bool a un enum interno
con el mismo comportamiento; su prueba gráfica solo adapta ese argumento.

## Comportamiento

- **1–5:** mismas operaciones sobre el `LightingSettings` usado por `renderLit`:
  directa general, direccional, puntual, reflexiones ambientales y representación
  del cielo, respectivamente. El estado del HUD se deriva de esa misma referencia.
- **F4:** alterna únicamente la visibilidad, inicialmente activa. No procesa
  REPEAT/RELEASE ni altera luces, FPS o la preferencia al navegar.
- **0:** asigna una copia de los valores originales de la zona activa. Incluye
  colores, posiciones/direcciones, intensidades, ambiente, atenuación y parámetros
  ambientales, no solo cinco interruptores. No llama a restart ni cambia recursos.
- Los originales se obtienen de `ZoneNavigator::lighting()` al iniciar y después
  de cada transición confirmada. En modos históricos se capturan los valores tras
  la inicialización de su contenido; no existe un preset universal de restauración.
- **6:** permanece independiente. 0 no desactiva regresión. Su etiqueta tiene una
  fila específica; no se presenta como una sexta fuente de luz.
- **ON-** en direccional/puntual indica que su interruptor está activado pero no
  contribuye porque DIRECTA está OFF. **OFF** conserva su significado individual.
  La leyenda `- SIN DIRECTA` explica el marcador. En regresión los ajustes se
  conservan, pero la imagen usa el pase de comparación histórico.

La prueba de teclado descubrió un defecto anterior: al activar 6 en Plaza se
intentaba dibujar `imported`, que ese modo no carga, y cerraba con
`Cannot draw null model`. Se añade únicamente la comprobación `imported` antes
de ese draw. Los modos que sí cargan el modelo mantienen su dibujo. No se cambia
el renderizador ni se altera el modo de regresión para ocultar el fallo.

## Presentación y caché

Panel inferior derecho con fondo oscuro opaco y letras claras de la misma fuente
bitmap ASCII del HUD existente. Se conserva la presentación monocroma: la fuente
actual genera una malla común de texto, sin colores por palabra. ON/OFF y el guion
expresan el estado sin depender de distinguir colores.

| Framebuffer | Presentación |
|---|---|
| 1280×720 | Título, cinco controles etiquetados, fila independiente de regresión y ayuda de restauración. |
| 640×360 | Fuente de 1.5 px por celda y filas que agrupan direccional/puntual y reflejos/cielo. |
| 320×240 | Estados `1:ON 2:ON 3:OFF`, segunda fila 4/5/6 y ayuda 0/F4; leyenda adicional si corresponde. |

El panel ocupa como máximo el tercio inferior reservado por el layout; navegación
queda arriba a la izquierda, debajo de la banda FPS. Las pruebas comprueban que
no se trunca ninguna de las 32 combinaciones ni se solapan ambos paneles.
`NavigationHud` reutiliza el renderer/material unlit y restaura el estado prestado.
Su caché reconstruye geometría solo al cambiar texto, tamaño o anclaje. Ocultar el
panel no mantiene un segundo estado editable que pueda quedar desactualizado.

## Pruebas Linux

Arch Linux x86_64, GCC 16.2.1, CMake 4.4.5, GLFW 3.5.1; Intel Iris Xe (ADL GT2),
Mesa 26.2.6, OpenGL 4.6 Core. No se instaló ninguna dependencia.

- Configuración y compilación mediante `linux-dev`, reutilizando su build.
- Línea base **35/35**: [registro](evidence/lighting-status-hud/baseline-ctest.txt).
- Final **36/36** en 31.94 s: [CTest](evidence/lighting-status-hud/ctest.txt),
  incluidas todas las anteriores y `lighting_hud_cpu`. Sin desactivar casos ni
  modificar sus expectativas para el HUD.
- CPU: originales de las cuatro zonas, campos completos, PRESS/REPEAT/RELEASE,
  cambio oculto, independencia FPS, 32 combinaciones × tres tamaños, contribución
  enmascarada, modo histórico, restauración sin tocar poses ni referencias.
- GPU real X11, ventanas ocultas: cuatro zonas × tres tamaños exactos, con lectura
  del back buffer. F4 off/on restaura imágenes exactas; F3 solo cambia su región;
  cada tecla 1–5 cambia su indicador y 0 restaura exactamente el framebuffer de
  origen. Los cambios con el panel oculto se reflejan al mostrarlo. Se comprueba
  el efecto de apagar la directa y el estado OpenGL antes/después del overlay.
- **Cero píxeles diferentes fuera del panel** para las doce comparaciones con/sin
  HUD. Se conserva íntegra la información educativa y FPS que queda fuera de él.
  [Registro gráfico](evidence/lighting-status-hud/graphics.txt).
- Entrada real de la aplicación con xdotool, ventana propia verificada por PID:
  F4 mantenida, F3 off/on, correspondencia exacta 1–5, restauración 0, 6 on,
  0 conservando 6 y salida limpia. [Registro](evidence/lighting-status-hud/native-input.txt).
  La primera prueba requirió esperar la liberación de F4 antes de volver a pulsar;
  la versión final verifica que solo hay un evento por pulsación.

Los registros sustituyen la raíz local por `<repo>`. No incluyen credenciales,
variables de entorno ni capturas de otras aplicaciones.

## Evidencia de framebuffer

| Zona | 1280×720 | 640×360 | 320×240 | Directa OFF, individuales ON |
|---|---|---|---|---|
| Plaza | [Completo](evidence/lighting-status-hud/hub-1280x720.png) | [Medio](evidence/lighting-status-hud/hub-640x360.png) | [Compacto](evidence/lighting-status-hud/hub-320x240.png) | [Enmascarado](evidence/lighting-status-hud/hub-masked-320x240.png) |
| EcoEnergy | [Completo](evidence/lighting-status-hud/ecoenergy-1280x720.png) | [Medio](evidence/lighting-status-hud/ecoenergy-640x360.png) | [Compacto](evidence/lighting-status-hud/ecoenergy-320x240.png) | [Enmascarado](evidence/lighting-status-hud/ecoenergy-masked-320x240.png) |
| AquaBio | [Completo](evidence/lighting-status-hud/aquabio-1280x720.png) | [Medio](evidence/lighting-status-hud/aquabio-640x360.png) | [Compacto](evidence/lighting-status-hud/aquabio-320x240.png) | [Enmascarado](evidence/lighting-status-hud/aquabio-masked-320x240.png) |
| EcoCycle | [Completo](evidence/lighting-status-hud/ecocycle-1280x720.png) | [Medio](evidence/lighting-status-hud/ecocycle-640x360.png) | [Compacto](evidence/lighting-status-hud/ecocycle-320x240.png) | [Enmascarado](evidence/lighting-status-hud/ecocycle-masked-320x240.png) |

Son 16 PNG convertidos sin pérdida de los PPM del framebuffer, sin edición visual.
Los 64 FPS de estas capturas son una muestra simulada fija para comparar píxeles;
el contador de la aplicación continúa calculando su lectura real como antes.

## Rendimiento observado

1280×720, mismas cámaras y estados; tres pares de 300 frames sin VSync solicitado,
alternando orden, y un par con VSync solicitado, tras 30 frames de calentamiento.
El navegador/animación se mantiene en la misma pose durante cada par. Se mide el
coste adicional del formato/dibujo cacheado del HUD, conservando FPS y navegación.
Las consultas instrumentales para verificar estado GL se excluyen del benchmark.

| Zona | FPS HUD oculto, swap 0 | FPS HUD visible, swap 0 | Variación | Swap 1 oculto / visible |
|---|---:|---:|---:|---:|
| Plaza | 485.4 | 494.5 | +1.89% | 57.3 / 57.5 |
| EcoEnergy | 410.2 | 406.3 | -0.94% | 57.3 / 57.3 |
| AquaBio | 513.8 | 502.5 | -2.20% | 57.2 / 57.3 |
| EcoCycle | 649.6 | 633.1 | -2.53% | 57.1 / 57.2 |

[Pasadas completas](evidence/lighting-status-hud/performance.csv). No se interpreta
la variación positiva de Plaza como una mejora; existe ruido de reloj/driver.
No se observó una regresión importante en esta muestra. Son resultados locales,
con poses estáticas; no certifican todos los equipos ni todas las cargas.
La sincronización efectiva la puede imponer el compositor/controlador.

## Archivos y almacenamiento

- Aplicación: `src/app/LightingControls.hpp`, `main.cpp`, `NavigationHud.hpp/.cpp`.
- Build/pruebas: `CMakeLists.txt`, `tests/lighting_hud_tests.cpp`,
  `lighting_hud_graphics_tests.cpp`, `lighting_hud_input.py` y el argumento de
  anclaje de `fps_graphics_tests.cpp`.
- Documentación: README, BUILD, ARCHITECTURE, este informe y una carpeta nueva
  `docs/evidence/lighting-status-hud/`. Se conservan créditos, imágenes y licencias.

Tamaño inicial del árbol completo (incluye .git, build y evidencias anteriores):
**3133218361 bytes**. Medición antes de los commits/publicación: **3183658421 bytes**;
crecimiento ~48.1 MiB, inferior a 2 GiB. El build continúa siendo `build/linux-dev`.
Se guardan 16 capturas nuevas, no cientos; los PPM de trabajo están ignorados en
build. No se borraron archivos ni se ejecutó limpieza. El informe final entrega
la lectura posterior al push, que puede variar por los metadatos Git.

## Límites y cierre

Windows no fue ejecutado: queda pendiente configurar/compilar/CTest y probar GUI
con controlador nativo. La prueba de teclado xdotool es exclusiva de Linux/X11.
El estado en regresión se etiqueta por separado; el panel no afirma que las luces
del pase normal contribuyan a esa comparación. No se añadieron colores por token.
No se crean PR ni se hace merge con main. Los SHA y commits finales se entregan
con el informe de publicación; los commits anteriores de AquaBio no se reescriben.
