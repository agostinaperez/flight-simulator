# Flight Simulator

Simulador de acrobacias aereas, Agostina Pérez López, 2026

## Dependencias

Las dependencias distribuidas con el proyecto se guardan bajo `third_party/`:
GLAD, FastNoiseLite y stb_image. GLFW, GLM, GCC y Make se instalan en el entorno
MSYS2 MINGW64 usando `scripts/setup-msys2.sh`; el manifiesto esta en
`dependencies/msys2-packages.txt`. El script omite los paquetes ya instalados.

Desde una terminal MSYS2 MINGW64, ejecutar:

```bash
bash scripts/setup-msys2.sh
```

OpenGL se provee mediante el driver de Windows. El FDM `dlfdm` no esta en el
checkout original ni en el directorio local explorado, por lo que no se pudo
copiar ni instalar; debera incorporarse cuando se obtenga su fuente.

## Compilar y ejecutar

Desde la raiz del proyecto:

```bash
make
make run
make clean
.\bin\flight-simulator.exe (en windows)
```

Las dependencias de terceros se documentan en `third_party/README.md`. En el
entorno de desarrollo actual, la ventana base compilo con `-Wall -Wextra` y
permanecio abierta durante la prueba de arranque de seis segundos. Esta etapa
no integra el FDM; se agregara junto con su fuente en una etapa posterior.
