# Contribuir a EcoSphere

Gracias por contribuir a un entorno de educación ambiental 3D.

## Preparar el entorno

Consulta la [guía de compilación](docs/BUILD.md) para instalar o preparar las
dependencias de Linux o Windows. Los lanzadores no descargan ni instalan paquetes.
En Linux, `./run-linux.sh --build` compila, `./run-linux.sh` abre la plaza y
`./run-linux.sh --test` ejecuta las pruebas. En Developer PowerShell, los comandos
equivalentes son `.\run-windows.ps1 -Build`, `.\run-windows.ps1` y
`.\run-windows.ps1 -Test`; Windows sigue sin validación nativa.

Para añadir modelos o modificar instalaciones, sigue la [guía de recursos](docs/ASSETS.md):
conserva el maestro Blender, exporta GLB y actualiza inventarios, procedencia,
permisos y pruebas antes de preparar el pull request. El arte queda fuera de MIT.

## Flujo de trabajo

1. Asegúrate de que la rama `main` está actualizada y crea una rama `feature/<descripcion>`.
2. Ejecuta la compilación y pruebas documentadas en `docs/BUILD.md` antes de empezar, cuando dispongas del entorno compatible.
3. Mantén los cambios enfocados; incluye capturas del programa real para cambios gráficos.
4. No subas archivos `build/`, caches, credenciales, rutas personales ni archivos de trabajo privados.
5. Abre un pull request, explica motivación, alcance, pasos de prueba y riesgos; no fusiones tu propio PR sin revisión del equipo.
6. Para contenido nuevo, proporciona origen, autoría verificable y derechos de licencia antes del merge.

El SDK de contenido v1.0 y los contratos de carga de recursos deben seguir siendo compatibles salvo una revisión técnica explícita. La etapa de distribución Windows está pendiente; no declares pruebas que no realizaste.

## Créditos

Las contribuciones se atribuirán según commits y PRs verificables. El repositorio no asigna autoría ficticia de módulos ni afirma aportaciones individuales que todavía no se hayan documentado.
