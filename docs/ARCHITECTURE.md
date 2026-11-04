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
