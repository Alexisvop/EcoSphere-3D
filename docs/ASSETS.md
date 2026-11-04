# Recursos tridimensionales y materiales

Los recursos se organizan por zona: `assets/plaza/`, `assets/installations/ecoenergy/`, `assets/installations/aquabio/`, `assets/installations/ecocycle/` y `assets/shared/`. El motor emplea modelos **GLB**, shaders GLSL y algunas texturas PNG.

Los cinco archivos `.blend` son bibliotecas maestras editables, una por área de contenido. Los inventarios (CSV) y reportes de exportación (JSON) en `docs/assets/` registran identificadores, dimensiones, relaciones e integridad por SHA-256.

## Modificar o aportar un modelo

1. Crea tu rama y determina a qué zona pertenece el recurso.
2. Trabaja en Blender con unidades y orientación consistentes; documenta la procedencia de la geometría, materiales y texturas.
3. Exporta GLB sin dependencias externas no declaradas; conserva el maestro `.blend` editable.
4. Actualiza inventarios e instancias; documenta licencias y elementos reutilizados.
5. Ejecuta los validadores y pruebas afectadas; añade una captura real del nuevo contenido.
6. Abre un PR con imágenes, archivos cambiados y comprobaciones.

No reutilices material de terceros sin tener derechos de redistribución. Los GLB,
PNG, SVG, archivos `.blend` y demás arte propio quedan **sin licencia abierta
específica por ahora** y fuera de MIT. Antes de distribuirlos, verifica sus
titulares y los permisos necesarios, conservando la procedencia y derechos de
terceros. Los shaders propios son código MIT.
La publicación en GitHub de los recursos originales incluidos está autorizada
por los integrantes según la confirmación de la dirección del proyecto. Conservan
sus derechos de autor; esta publicación no otorga permisos abiertos adicionales.
Ver [alcance de licencias](../LICENSE_SCOPE.md).
