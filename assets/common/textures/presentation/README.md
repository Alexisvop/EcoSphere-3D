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

El script aplica el postproceso una vez sobre los GLB originales y en ejecuciones
posteriores comprueba el resultado. Los `.blend` permanecen como originales de
modelado: una nueva exportación sobrescribiría este acabado. Para regenerarlo,
trabajar sobre los GLB originales de main `0732ad343434f2983f2d98a2454dc516b09488b2`,
con el script y sin el manifiesto de salida anterior. Coinciden byte a byte con
los originales de `046dba7` usados en la implementación histórica; el manifiesto
no necesita regenerarse al sincronizar Git. No reexportar los modelos antes de
la exposición.
