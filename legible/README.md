# EcoSphere 3D, versión legible

Esta carpeta tiene el mismo programa que `src/`, pero reacomodado para leerlo y modificarlo más fácil:
una instrucción por línea, nombres completos en lugar de letras sueltas, funciones largas partidas en
pasos con nombre y comentarios cortos en español. El `src/` original no se modificó.

Usa los modelos (`assets/`), las librerías (`third_party/`), el SDK (`include/`) y las pruebas
(`tests/`) de la raíz del repositorio. Los shaders reacomodados están en `legible/shaders/`.

## Compilar y abrir

Desde **Developer PowerShell for VS**, dentro de `legible/`:

```powershell
cmake --preset windows-dev -Dglfw3_DIR=<ruta a GLFW 3.4>\lib\cmake\glfw3
cmake --build --preset windows-dev
ctest --preset windows-dev
cd build\windows-dev
.\ecosphere.exe --plaza
```

En Linux es igual con el preset `linux-dev`. Las 40 pruebas del proyecto original corren contra este
código y pasan igual.

## Dónde está cada cosa

| Quiero cambiar... | Archivo |
|---|---|
| Posición, escala, giro o visibilidad de un modelo de una zona | `src/modules/<Zona>/<Zona>Settings.hpp` (un renglón por modelo) |
| Qué modelos se agregan, cuáles se pueden tomar y qué animación tienen | `src/modules/<Zona>/Module.cpp` |
| Luces de una zona | campo `lighting` del `Settings.hpp` de la zona |
| Cámara al entrar a cada zona, velocidad al caminar | `src/world/ZoneSettings.hpp` |
| Plaza central | `src/world/PlazaSettings.hpp` y `src/world/Plaza.cpp` |
| Teclas | `handleKey()` en `src/app/main.cpp` |
| Movimiento de cámara cada cuadro | `readCameraInput()` en `src/app/main.cpp` |
| Ciclo principal | `runFrameLoop()` y `runFrame()` en `src/app/main.cpp` |
| Carga de GLB | `src/engine/assets/AssetManager.cpp` |
| VAO/VBO y uniforms | `src/engine/graphics/ModelRenderer.cpp` |
| Iluminación Phong / Blinn-Phong, Fresnel | `shaders/model.frag` |
| Matriz de normales | `shaders/model.vert` y `collectInstances` en `ModelRenderer.cpp` |
| Selección con rayo | `src/engine/interaction/Interaction.cpp` |
| Keyframes (mix y slerp) | `src/engine/animation/Animation.cpp` |
| Cielo (cubemap) | `src/engine/graphics/Environment.cpp` |

Los archivos `src/app/*Smoke.cpp` son los modos de captura automática (`--aquabio-smoke`, etc.).
No se usan al abrir la plaza normalmente.

## Pruebas después de un cambio

Algunas pruebas comparan contra el estado entregado (número exacto de modelos, posiciones del
inventario, capturas), así que al mover o agregar algo es normal que fallen unas cuantas aunque el
programa funcione bien.
