# Dependencias vendorizadas

Las siguientes dependencias se copiaron de `ogl-project-template/third_party/`
sin modificar sus fuentes:

- **GLAD 2.0.8**: loader para OpenGL 4.6 Core. Los archivos conservan la
  declaracion SPDX `(WTFPL OR CC0-1.0) AND Apache-2.0`.
- **FastNoiseLite**: generador de ruido procedural, bajo licencia MIT. El
  aviso de copyright y licencia se conserva en `FastNoiseLite/FastNoiseLite.h`.
- **stb_image 2.30**: decodificador de imagen de un solo header, de dominio
  publico segun su aviso. El texto original se conserva en `stb/stb_image.h`.

GLFW, GLM, GCC y Make son paquetes del entorno MSYS2 MINGW64 y se declaran en
`../dependencies/msys2-packages.txt`.
