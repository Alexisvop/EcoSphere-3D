# Arquitectura

EcoSphere está desarrollado en **C++17**, con **OpenGL 3.3 Core / GLSL 330**, GLFW, GLAD y GLM. Su arquitectura separa las responsabilidades de aplicación, renderizado, administración de recursos y contenido educativo.

```text
Aplicación / entrada / HUD / navegación
           │
           ▼
  SDK de contenido v1.0
  (IModule, ModuleServices, ModuleSession)
           │
    ┌──────┼────────┬────────────┐
    ▼      ▼        ▼            ▼
  Plaza  EcoEnergy AquaBio     EcoCycle
           │
           ▼
AssetManager / ContentRegistry / escenas
           │
           ▼
      OpenGL y shaders
```

Las instalaciones reutilizan modelos compartidos mediante rutas canónicas y referencias a recursos; no se copian mallas distintas para cada propietario. La navegación gestiona entradas, salidas y reentradas; las animaciones tienen mecanismos simples, por keyframes y procedurales.

## Ubicación del código

- `include/` — contratos públicos y tipos reutilizables.
- `src/` — motor, módulos, renderizado, navegación e interfaz.
- `assets/` — shaders, texturas, GLB y bibliotecas `.blend`.
- `tests/` — batería CTest y validadores de funcionalidad.
- `tools/` — generadores de modelos y auditorías de inventarios.
- `docs/assets/` — inventarios y datos de geometría utilizados en pruebas.

Para modificar o extender la interfaz del SDK, redacta primero un caso de compatibilidad y conserva las pruebas de consumidores externos.

## Contador FPS de aplicación

`src/app/FpsCounter.hpp` mide intervalos monotónicos del loop mediante
`glfwGetTime()`, sin alimentar el tiempo de simulación. Acumula fotogramas y
segundos durante al menos 0.5 s. Primer fotograma, reloj inválido, intervalos
no positivos o superiores a 2 s, foco perdido y minimización reinician la muestra.
Una pausa de animaciones con P no pausa la medición del renderizado.
F3 alterna visibilidad solo en PRESS; los repeats/releases no la alternan.
`NavigationHud` reutiliza fuente geométrica, material unlit y caché del modelo;
solo reconstruye al cambiar texto, tamaño o anclaje. La banda superior de 40 px
queda reservada para FPS; el HUD educativo conserva su espacio debajo.
No se cambia el SDK, ni se añade un framework o hilo.
