# Límite seguro de reducción interactiva

Base: `main`, `c42f8a9ac361b7eda3ab60b9769c897152ee6f1b`.
Rama: `fix/aquabio-scale-limit`. No se modificó main ni el experimento de cromo.

## Causa confirmada

Secuencia original: AquaBio, `P`, `Tab` selecciona `AQUA-009-demo`, mantener `C`.
La reproducción X11 anterior a la corrección terminó con código **1** tras
aproximadamente **4,73 s** y el mensaje exacto:

```
Singular/ill-conditioned normal transform
```

El adaptador de entrada guardaba sucesivas escalas positivas sin considerar las
matrices finales de dibujo. La primera matriz rechazada es una barra del **marco
de selección**, antes que el cuerpo del objeto. No era una textura dañada, un
NaN ni una escala exactamente cero.

La reproducción CPU a 60 Hz rechazó el frame 275 (4,5833 s simulados): escala
anterior `0,0408982113`, escala ya guardada `0,04042383656`. La barra del eje Z
tenía determinante `9,908408316e-9`, inferior al umbral existente de `1e-8`.
La matriz mundial del objeto conservaba posición `(-9,5; 0,44; -5,5)` y escala
uniforme; su GLB tiene un nodo con transformación identidad. La excepción del
marco propagaba hasta el manejador fatal de la aplicación. La traza GDB del
reproductor CPU confirma el lanzamiento en `normalMatrix`, línea 56 de
`include/ecosphere/sdk/Lighting.hpp`; no se presenta como traza del proceso X11.
La diferencia entre segundos simulados y tiempo de pared depende del muestreo de
entrada y los frames iniciales; ambas reproducciones alcanzan el mismo rechazo.

## Corrección

Se limita exclusivamente la reducción solicitada por el adaptador común de entrada:

- Se consideran la transformación mundial actual, ancestros interactivos, todos
  los descendientes afectados, jerarquías GLB y barras del marco de selección.
- Para una reducción uniforme por factor `f`, el determinante se multiplica por
  `f³`. El factor mínimo se calcula mediante la raíz cúbica del margen requerido
  dividido por el menor determinante de dibujo. Se incluye también la matriz de
  la entidad, que utiliza el sistema de interacción.
- Se conserva un margen de `1e-7`, diez veces el umbral existente, calculando la
  cota en doble precisión y redondeando hacia el estado válido. Las componentes
  de las poses conservan además margen sobre el mínimo existente de `1e-4`.
- Las barras usan sus dimensiones actuales: ancho `0,025` y longitud según los
  límites reales del objeto. No se alteró su geometría.
- Al alcanzar el mínimo se deja de acumular reducción. `V` sigue ampliando; las
  otras manipulaciones conservan sus operaciones y la escala no cambia posición
  ni orientación. No se amplía automáticamente un objeto al limitar `C`.

En `AQUA-009-demo`, la cota resulta aproximadamente **0,0873581**; no es un mínimo
fijo compartido artificialmente por todos los modelos. El contenido proporciona
el menor determinante de sus nodos GLB y la demo original conserva esa información
para su modelo importado. El callback añadido pertenece al adaptador interno,
no al SDK público.

No se cambiaron `normalMatrix`, `validatePose`, la API estricta de manipulación,
shaders, cámaras, luces, HUD, geometría, UV, texturas ni contratos del SDK. No hay
un `try/catch` que descarte errores del motor. Los datos inválidos de la API
continúan rechazándose.

## Cobertura y evidencia

Configure y build finales satisfactorios, sin advertencias; **CTest 40/40 PASS**
(se conservan los 39 nombres anteriores). Pasaron los ocho grupos de validación
GPU/entrada en Intel Iris Xe, Mesa 26.2.6, OpenGL 3.3 Core: sombreado y Fresnel,
HUD, FPS, navegación, demo de interacción y pruebas X11. La navegación gráfica
completó 72 transiciones por las cuatro zonas, a 1280×720. Las 29 capturas
comparables son idénticas byte por byte a la auditoría anterior a esta corrección.

La prueba nueva `interaction_scale_limits_cpu` falló sobre el código sin corregir
con el rechazo original. Comprueba:

- Los **20 objetos educativos** a 60 Hz durante 5/10/20 segundos, con validación
  de las matrices del cuerpo y de las doce barras del marco en cada paso.
- El caso original a **30/60/144 Hz**, con el mismo mínimo dentro de tolerancia
  relativa `1e-4`, sin deriva entre los segundos 10 y 20.
- Recuperación con `V`, traslación, rotación, deselección y rechazo estricto de la API.
- Ancestros no uniformes, descendientes, un GLB con nodos transformados y límites
  de selección amplios para que la malla, no el marco, determine el mínimo.
- La demo original, y salida de AquaBio, visita a EcoEnergy y reentrada a AquaBio
  con selección y manipulación reiniciadas.

X11 real, sobre ventanas propias con PID y foco comprobados: tres procesos nuevos
mantuvieron `C` durante **5, 10 y 20 segundos**. Los tres permanecieron activos,
permitieron `V`, traslación/rotación, tomar/soltar y deselección, y terminaron con
**código 0** por Esc. Un recorrido nativo adicional mantuvo `C` 20 segundos en
AquaBio y continuó con las selecciones, animaciones y accesos/retornos de las tres
instalaciones. Las simulaciones CPU se distinguen de estos eventos X11 reales.

Resumen público sin rutas personales: [validation.json](evidence/aquabio-scale-fix/validation.json).
Los registros completos, diagnóstico, matrices, traza y herramientas temporales
se conservan localmente en `build/aquabio-scale-fix/`, ignorado por Git. No se
sobrescribieron registros de `build/audit-prepresentation/`.

Archivos de implementación: `src/app/InteractionDemoInput.cpp/.hpp` y
`src/world/InteractionAnimationDemo.cpp/.hpp`. La cobertura está en
`tests/interaction_scale_tests.cpp`, registrada desde `CMakeLists.txt`.

## Reproducción y alcance

```bash
cmake --preset linux-dev
cmake --build --preset linux-dev
ctest --preset linux-dev --output-on-failure
./build/linux-dev/ecosphere --module aquabio --content
```

En la última aplicación: `P`, `Tab`, mantener `C`, ampliar con `V`, manipular y
salir con Esc. Para navegar desde Plaza usar `./build/linux-dev/ecosphere --plaza`.
La corrección debe integrarse mediante revisión posterior; main conserva el P1
hasta esa integración. No se crea PR ni se realiza merge en esta intervención.

Los resultados Linux y la regresión gráfica se detallan en el resumen enlazado.
No se ejecutó Windows ni se reconstruyó su instalador: es necesaria una comprobación
en el equipo de exposición después de integrar y compilar la corrección. La cota
protege la reducción interactiva actual; no sustituye la validación de recursos
o transformaciones arbitrarias suministradas por consumidores del SDK.
