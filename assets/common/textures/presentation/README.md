# Texturas selectivas de presentación

Seis PNG RGBA de 128×128, originales y deterministas, generados con la biblioteca
estándar de Python por `tools/apply_presentation_textures.py`. No contienen
fotografías, descargas, recursos de terceros ni dependencias nuevas. Se aplica el
alcance de recursos artísticos propios de [LICENSE_SCOPE.md](../../../../LICENSE_SCOPE.md).

| Archivo | Uso |
|---|---|
| water.png | Ondas suaves en humedal y sedimentador de AquaBio |
| wood.png | Veta en refugio de polinizadores y banco de reparación |
| concrete.png | Losas, caminos, borde del humedal y bases técnicas |
| metal.png | Cepillado tenue en soportes, marcos y maquinaria |
| painted.png | Acabado uniforme con variación suave en gabinetes y contenedores |
| belt.png | Nervaduras discretas de la cinta transportadora |

El RGB neutro modula el color base existente; alfa siempre es 255. No se alteran
metallic, roughness, transparencia, normales, UV, iluminación ni geometría.
Las imágenes se repiten con filtrado lineal y mipmaps, mediante el pipeline actual.
Las celdas solares y diagramas existentes se conservan.

Los PNG son fuentes compartidas; cada GLB lleva sus imágenes embebidas para
conservar la carga offline y el empaquetado existente. No se añade deduplicación
entre GLB al motor. `manifest.json` enumera las asignaciones y hashes de originales,
datos preservados y resultados. CTest comprueba este manifiesto y su reproducción.

```bash
python3 -B tools/apply_presentation_textures.py --check
```

La verificación es de solo lectura. Para regenerar tras un cambio revisado del
generador, conservar el manifiesto y ejecutar:

```bash
python3 -B tools/apply_presentation_textures.py --regenerate
python3 -B tools/apply_presentation_textures.py --check
```

`--regenerate` comprueba primero los recursos actuales y los 14 originales de
Git, fija main en `0732ad343434f2983f2d98a2454dc516b09488b2` y coteja sus SHA-256
con el manifiesto. Compone cada GLB desde esos originales en memoria; nunca
incrusta sobre un GLB ya texturizado. Repetirlo produce los mismos bytes y no
acumula imágenes. Si cambia main, se requiere revisar la procedencia antes de
regenerar. La verificación y CTest no requieren consultar Git.

Los `.blend` permanecen como originales de modelado: una nueva exportación
sobrescribiría este acabado. Las comparaciones y límites de visibilidad del pulido
se documentan en [PRESENTATION_TEXTURES_VISIBILITY.md](../../../../docs/PRESENTATION_TEXTURES_VISIBILITY.md).
