# Compilación de EcoSphere

## Estado de plataformas

- **Linux / Arch:** entorno en que se desarrolló y comprobó la base; el paquete público debe pasar de nuevo sus pruebas en una copia limpia.
- **Windows:** el preset `windows-dev` existe, pero aún **no** hay validación funcional real ni instalador final.

## Requisitos

CMake 3.24+, Ninja, compilador con C++17, GLFW 3.5.1 exacta instalada con archivos de configuración de CMake; Python 3.10+ cuando `BUILD_TESTING=ON`; OpenGL 3.3 Core y un controlador compatible. GLAD, GLM, cgltf y stb_image están versionados dentro de `third_party/`; verifica los avisos originales de cada dependencia. Blender es opcional para compilar, pero necesario para editar los maestros `.blend`.

En Arch Linux, comprueba primero las versiones instaladas. Un ejemplo de preparación de herramientas es:

```bash
sudo pacman -S --needed base-devel cmake ninja python glfw
pacman -Q glfw
```

El proyecto exige `find_package(glfw3 3.5.1 EXACT CONFIG REQUIRED)`. Si el paquete del sistema tiene otra versión, instala la versión requerida de forma controlada y apunta a su `glfw3_DIR`; no alteres la versión de CMake para ocultar un problema.

## Lanzadores

| Acción | Bash / Linux | Windows PowerShell |
|---|---|---|
| Abrir plaza; compilar si falta el binario | `./run-linux.sh` | `.\run-windows.ps1` |
| Configurar y compilar sin abrir ventana | `./run-linux.sh --build` | `.\run-windows.ps1 -Build` |
| Configurar, compilar y ejecutar CTest | `./run-linux.sh --test` | `.\run-windows.ps1 -Test` |
| Ayuda sin configurar ni compilar | `./run-linux.sh --help` | `.\run-windows.ps1 -Help` |

Se pueden combinar build y test: se compila una sola vez y se ejecuta CTest.
Las opciones desconocidas producen un error. La ayuda no requiere las herramientas
de compilación. Sin opciones, se configura para comprobar dependencias y se
reutiliza el ejecutable existente; después de editar código, shaders o modelos,
ejecuta la opción de compilación para actualizarlo.

Linux requiere Bash, CMake 3.24+, Ninja y `c++` en PATH, o un ejecutable de
compilador indicado mediante `CXX`. CMake verifica C++17, el compilador C,
GLFW **3.5.1 exacta** y Python 3.10+ (el preset habilita las pruebas).
Para `--test` se necesita también `ctest` en PATH. Abrir la aplicación requiere
OpenGL 3.3 Core y una sesión gráfica compatible.

En Windows, usa **Developer PowerShell for Visual Studio** con las herramientas
de escritorio C++ y Windows SDK preparados. Se requieren `cl.exe` (MSVC C++17),
`cmake.exe`, `ninja.exe`, GLFW **3.4 exacta** con configuración CMake y Python
3.10+. `-Test` necesita `ctest.exe`. El script solicita explícitamente MSVC para
C y C++; no sustituye ese compilador por MinGW o Clang. Está escrito con sintaxis
compatible con Windows PowerShell 5.1 y PowerShell 7, **pendiente de pruebas
nativas**. No modifica ExecutionPolicy, el registro ni ajustes globales.

Ambos scripts resuelven la raíz por su ubicación, aceptan rutas con espacios y
usan exclusivamente los presets `linux-dev` / `windows-dev`, con compilación
incremental en `build/linux-dev` / `build/windows-dev` y dos trabajos simultáneos.
Al abrir la aplicación, trabajan desde el directorio del binario y conservan
`shaders/` y `assets/` a su lado. Los errores de CMake, compilación, CTest y la
aplicación se propagan como códigos de salida. No hay descargas ni instalaciones
automáticas; las dependencias incluidas se conservan bajo `third_party/`.

### Resolución de errores

- **Herramienta ausente:** revisa PATH y las dependencias de tu entorno. En Linux,
  herramienta faltante devuelve 127; una opción desconocida devuelve 2.
- **MSVC no encontrado:** abre Developer PowerShell for Visual Studio con el
  toolset C++ y SDK instalados. No basta una consola sin ese entorno.
- **GLFW no encontrada o versión distinta:** conserva la versión exacta del
  preset. Configura una vez su paquete con
  `cmake --preset linux-dev "-Dglfw3_DIR=<prefijo-local>/lib/cmake/glfw3"`, o el
  preset `windows-dev` desde Developer PowerShell; luego vuelve al lanzador.
  La caché conserva esa ubicación. No se modifica la versión requerida.
- **Python ausente o incompatible:** el preset incluye CTest y exige Python 3.10+;
  revisa el diagnóstico de CMake antes de volver a compilar.
- **Caché incompatible:** CMake informa del conflicto. No se borran ni crean
  builds alternativos automáticamente; revisa generador, compilador y raíz.
- **Binario, shaders o modelos ausentes:** usa `--build` / `-Build` y conserva la
  estructura del directorio del ejecutable. Un error gráfico requiere revisar
  también el controlador y la sesión; no se instala nada automáticamente.
- **PowerShell bloquea el script:** respeta la política del equipo y consulta a
  quien lo administre. El lanzador no la cambia; puedes usar los comandos CMake
  documentados en esta guía.

## Compilación y pruebas

Desde la raíz del clon público:

```bash
cmake --preset linux-dev
cmake --build --preset linux-dev
ctest --preset linux-dev --output-on-failure
./build/linux-dev/ecosphere --plaza
```

La base privada registró 34/34 casos CTest al cierre de la Fase 6. **Ese resultado no acredita por sí solo al nuevo paquete:** las 34 pruebas deben repetirse desde una clonación o directorio limpio de esta edición. Si alguna falla, detener la publicación y resolver el problema conservando la prueba original.

## Recursos en tiempo de ejecución

CMake prepara shaders y copia modelos bajo el directorio del ejecutable. No ejecutes un binario movido sin sus assets. Los modelos GLB, variantes, texturas y shaders presentes en `assets/` son parte de la aplicación.

## Windows

`windows-dev` solicita MSVC + Ninja y GLFW 3.4 exacta. Es una configuración de trabajo **no validada**; no se promete compatibilidad hasta probarla nativamente. La versión Release y el instalador se prepararán en una etapa posterior.

## Política de almacenamiento

Usa los directorios de compilación existentes si sus opciones son compatibles. No registres `build/` ni elimines evidencias o modelos originales de manera automática. Antes de realizar limpiezas, compara rutas y respaldos y solicita autorización del equipo.
