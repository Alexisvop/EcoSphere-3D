# Sincronización segura de la rama de texturas

Repositorio auditado: `Alexisvop/EcoSphere-3D`, directorio
`/home/alexis/Documents/CG/EcoSphere-3D`. Único remoto configurado: `origin`,
`https://github.com/Alexisvop/EcoSphere-3D.git`. No se accedió ni modificó ningún
repositorio privado original ni otro proyecto. No se creó ningún worktree.

Antes de modificar archivos se comprobó `pwd`, `git status --porcelain=v1`,
`git remote -v` y `git --no-pager branch -vv`; el árbol estaba limpio en
`feature/presentation-textures`. Se ejecutó `git fetch origin` y se verificaron
las referencias contra GitHub con `git ls-remote --heads origin ...`.
Los SHA coincidieron exactamente con la auditoría proporcionada por el usuario.
El nombre `fix/presentation-textures-sync` no existía local ni remotamente.

| Referencia inicial | SHA conservado |
|---|---|
| `main` local | `1e8e334f9482cfb78626a59179abd5e948d8c15c` |
| `origin/main`, base utilizada | `0732ad343434f2983f2d98a2454dc516b09488b2` |
| `feature/presentation-textures` local y remoto | `74f3b2e6343aa55db621375a648fc93a7325c271` |
| Ancestro común de main remoto y texturas | `f82c2881758555fb54bc9c4d41440bbee0b9bf95` |

La divergencia original era cinco commits de texturas/iluminación frente a un
commit exclusivo de main. El PR #4 incorporó los tres cambios de iluminación
como un solo commit, `0732ad3`, cuyo padre es `f82c288`. La otra rama conservó
los cherry-picks `a7a41fe`, `99048f0` y `046dba7`, seguidos por los dos cambios
de texturas. Por eso divergen los grafos aunque la iluminación tenga el mismo
contenido. El árbol completo de `0732ad3` es idéntico al de `046dba7`.

Los cinco commits se comprobaron como objetos existentes y ancestros de la rama
publicada `origin/feature/presentation-textures`. Se conservaron todos, sin
reescribirlos ni mover esa rama. La [auditoría inicial](evidence/presentation-textures-sync/initial-audit.json)
registra sus SHA completos, las referencias iniciales, árboles y hashes.
El [grafo original](evidence/presentation-textures-sync/original-graph.txt)
permite comprobar las relaciones de ancestros.

La rama nueva se creó desde `0732ad3` sin establecer seguimiento de main.
Después se trasladaron exclusivamente
los dos parches de texturas, en orden:

| Commit original preservado | Cherry-pick nuevo | Contenido |
|---|---|---|
| `5ddf79d388ba742e7cb7cbda2382a691372ed952` | `92a42d1349121c8b7e1cf5054f369a3f8a7e5914` | Texturas, materiales y verificaciones auxiliares |
| `74f3b2e6343aa55db621375a648fc93a7325c271` | `8b4780d955166db58391b7957cabfa66e75fe89a` | Documentación, inventarios y evidencias originales |

No hubo conflictos ni se utilizaron estrategias `ours` o `theirs`. Se conservaron
los autores, sus correos y las fechas de autor de ambos commits. Los nuevos SHA
son normales al cambiar el padre; los originales permanecen publicados.
El padre de `92a42d1` es exactamente `0732ad3`. Los tres cherry-picks de iluminación
no son ancestros de la rama corregida; sí siguen siendo ancestros de la rama
histórica. La iluminación vigente llega por main.

Justo después de ambos cherry-picks y antes de conciliar documentos, el árbol
completo resultó idéntico al de `74f3b2e`. No fue necesario resolver cambios de
colaboradores, regenerar texturas, reexportar modelos o modificar el motor.
El tercer commit nuevo contiene solamente la conciliación documental y las
evidencias de esta validación. Su SHA final, que identifica también este informe,
se entrega en la respuesta de cierre y se verifica contra la rama publicada.

Los 14 GLB de la rama original coinciden byte a byte con los trasladados. Los
seis PNG originales y el manifiesto tampoco cambiaron. El hash de cada GLB sin
texturas en main coincide con `original_sha256` del manifiesto, por lo que sus
comprobaciones de geometría, UV, estructura, materiales y datos binarios siguen
siendo válidas. La [auditoría inicial](evidence/presentation-textures-sync/initial-audit.json)
incluye la comparación individual de los 14 modelos.

Los inventarios, reportes de exportación y script de postprocesamiento conservan
el contenido original completo. Los 132 archivos de
`docs/evidence/presentation-textures/protected-files.json` coinciden tanto con
sus hashes registrados como con main actual. Esto incluye shaders, cámaras,
controles, HUD, SDK, Plaza y fuentes Blender; no se invalidó ningún hash por
esta sincronización. Los shaders Phong/Blinn-Phong/Fresnel no recibieron parches.

Toda la carpeta histórica `docs/evidence/presentation-textures/` se conservó
byte a byte, incluyendo sus 48 PNG y registros de las pruebas originales.
No se sobrescribieron capturas antes/después ni se atribuyeron aquellas pruebas
a la nueva base. Los seis nuevos framebuffers de entrada de las instalaciones,
en Phong y Blinn-Phong, coinciden exactamente con los hashes RGB históricos:
[comparación de framebuffers](evidence/presentation-textures-sync/framebuffer-equivalence.json).
Las nuevas pruebas producen sus capturas temporales en
`build/presentation-textures-sync/`, sin añadir ese directorio a Git.

La documentación se concilió de forma selectiva:

- `docs/PRESENTATION_TEXTURES.md` identifica ahora la rama vigente y distingue
  explícitamente el informe histórico de la validación nueva. Conserva las
  referencias históricas legítimas a `046dba7`, `5ddf79d` y `74f3b2e`.
- `assets/common/textures/presentation/README.md` indica main `0732ad3` como
  fuente actual para reproducir las texturas y explica su igualdad con los GLB
  originales históricos. No cambia el procedimiento ni el manifiesto.
- `README.md`, `docs/BUILD.md` y `docs/PHONG_FRESNEL_COMPARISON.md` se revisaron
  y permanecen idénticos a main. No necesitaban actualización para reparar Git.
- No cambiaron licencias, créditos ni avisos de procedencia.

Se ejecutaron de nuevo `cmake --preset linux-dev`, `cmake --build --preset
linux-dev` y `ctest --preset linux-dev --output-on-failure`, sin instalar
dependencias ni borrar archivos de compilación. Configuración y compilación
correctas; **38/38 pruebas CTest correctas**, incluyendo integridad de texturas,
assets de las cuatro zonas, iluminación, sombreado, FPS, HUD, cámaras,
navegación, animaciones, interacciones y contratos del SDK.

[Configuración](evidence/presentation-textures-sync/configure.txt),
[compilación](evidence/presentation-textures-sync/build.txt) y
[CTest nuevo](evidence/presentation-textures-sync/ctest.txt).

También se ejecutaron de nuevo, de forma secuencial y con acceso local a X11/GPU:

| Verificación nueva | Resultado y evidencia |
|---|---|
| Phong/Blinn, Fresnel, cubemap y cuatro zonas | [SHADING GPU PASS](evidence/presentation-textures-sync/shading.txt); referencia Blinn con delta cero |
| HUD de iluminación y FPS | [LIGHTING HUD GPU PASS](evidence/presentation-textures-sync/hud.txt), cuatro zonas × tres tamaños; F3/F4, reset y estado OpenGL |
| Controles nativos | [SHADING NATIVE PASS](evidence/presentation-textures-sync/native-input-result.txt), F5 mantenida, F3/F4, reset y teclas 1–6 |
| Navegación, carga y descarga de zonas | [NAVIGATION GPU PASS](evidence/presentation-textures-sync/navigation-app.log), visitas repetidas y salida limpia |
| Texturas embebidas cargadas | [Transiciones](evidence/presentation-textures-sync/navigation-transitions.csv): AquaBio 6, EcoEnergy 10 y EcoCycle 7 texturas, incluyendo las dos preexistentes de EcoEnergy |
| FPS durante navegación | [Registro nuevo](evidence/presentation-textures-sync/navigation-fps.csv), 300 frames por zona |

La prueba de sombreado conserva sus [cámaras](evidence/presentation-textures-sync/shading-captures.csv)
y [comparaciones](evidence/presentation-textures-sync/shading-differences.csv).
La prueba CPU de animaciones/interacciones y la actualización de animaciones
durante la navegación GPU pasan; no se cambiaron sus mecanismos ni parámetros.
Los nuevos registros complementan las evidencias originales, sin sustituirlas.

La [comprobación de alcance](evidence/presentation-textures-sync/scope-check.txt)
verifica ancestros, documentos protegidos, evidencias históricas y rutas del
parche. La [lista de archivos trasladados](evidence/presentation-textures-sync/transferred-files.txt)
describe exactamente el delta de los dos cherry-picks contra main. Además de
ese delta, solo se modifican los dos documentos de conciliación anteriores y
se añaden este informe y `docs/evidence/presentation-textures-sync/`.
La [lista final](evidence/presentation-textures-sync/final-files.txt) permite
revisar el conjunto completo.

El delta contiene los seis PNG, el manifiesto y postproceso, los 14 GLB, seis
inventarios/reportes, una prueba CTest, los conteos de texturas de dos smokes y
documentación/evidencias relacionadas. No contiene commits duplicados de
iluminación, nuevas mejoras visuales, geometría nueva, builds, credenciales,
prompts, documentos internos u otros proyectos. Se revisaron las rutas y los
textos añadidos y no aparecieron patrones reconocibles de secretos; esa revisión
no se presenta como una auditoría general de seguridad del repositorio.

Antes de publicar se repitió la consulta de referencias remotas: main y la rama
histórica seguían en sus SHA originales y el destino nuevo no existía.
[Referencias previas al push](evidence/presentation-textures-sync/pre-push-refs.txt).
La publicación autorizada se limita a `origin/fix/presentation-textures-sync`,
con `git push --set-upstream origin fix/presentation-textures-sync`, sin force.
El cierre confirma el SHA publicado y vuelve a comprobar los SHA de las dos
ramas originales. No se actualiza main local, no se mueven etiquetas ni se borran
ramas. No se ejecuta pull, reset, rebase, PR o merge.

No se repitieron los tres smokes individuales de instalaciones ni se regeneraron
sus comparativas originales: sus contenidos se preservaron y las comprobaciones
actuales cubren el traslado sin cambios de ejecución. No se declara una nueva
validación Windows. El usuario informó una prueba funcional anterior en Windows;
esta corrección solo aporta resultados nuevos de Linux. No se hizo una auditoría
general ni se implementaron mejoras artísticas adicionales.

La rama está preparada técnicamente para auditoría externa: parte del main
verificado, contiene solo los dos parches de texturas y esta conciliación,
preserva exactamente los assets y la implementación anterior y pasa las
validaciones Linux nuevas. La revisión externa debe comparar esta rama con
main `0732ad3`. El trabajo se detiene aquí, sin mejoras visuales adicionales.
