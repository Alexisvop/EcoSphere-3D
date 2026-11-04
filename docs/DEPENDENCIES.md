# Dependencias incluidas y licencias

CMake no descarga ni instala dependencias. Las versiones quedan fijadas en las
fuentes incluidas y en los presets. `third_party/SHA256SUMS` conserva el
inventario original de 439 archivos; el manifiesto de preparación incluye
también los textos completos Apache-2.0 y CC0-1.0 añadidos a esta edición.

| Componente | Versión / origen | Licencia y aviso |
|---|---|---|
| GLFW Linux | 3.5.1 exacta, sistema; https://www.glfw.org/ | zlib/libpng, `third_party/glfw/LICENSE.md` |
| GLFW Windows | 3.4 exacta, preset sin validar | zlib/libpng |
| GLAD | Generador 2.0.8; https://github.com/Dav1dde/glad/tree/v2.0.8 | Generador MIT; salida `(WTFPL OR CC0-1.0) AND Apache-2.0`; se conserva la alternativa CC0, los textos completos y los avisos Khronos en `third_party/glad/` |
| GLM | 1.0.1; https://github.com/g-truc/glm/tree/1.0.1 | MIT elegida, alternativa Happy Bunny conservada en `third_party/glm/copying.txt` |
| cgltf | 1.15, commit 360db1a95480fe102ae9c69b27c5d101167ff5ba | MIT, `third_party/cgltf/LICENSE`; https://github.com/jkuhlmann/cgltf |
| stb_image | 2.30, commit d14d46f6b44456eb88beac58cba833a4135d4c89 | MIT elegida, Unlicense conservada, `third_party/stb/LICENSE`; https://github.com/nothings/stb |

Los avisos y fuentes originales de terceros se conservan sin modificaciones.
Los hashes comprueban integridad local, no autenticidad independiente.
No se descargaron nuevas dependencias para preparar esta edición.

El código fuente propio se licencia bajo MIT, con los tres autores confirmados
del equipo. Los recursos artísticos propios siguen sin licencia abierta específica;
consulta [alcance de licencias](../LICENSE_SCOPE.md) y
[revisión de derechos](LICENSING_REVIEW.md). La publicación de esta edición
está autorizada por la dirección del proyecto tras las verificaciones técnicas;
los recursos artísticos mantienen sus derechos de autor y quedan fuera de MIT.
