# Flight Simulator

Simulador de acrobacias aereas, Agostina Pérez López, 2026

## Dependencias

Las dependencias distribuidas con el proyecto se guardan bajo `third_party/`: GLAD, FastNoiseLite y stb_image. GLFW, GLM, GCC y Make se instalan en el entorno MSYS2 MINGW64 usando `scripts/setup-msys2.sh`; el manifiesto esta en
`dependencies/msys2-packages.txt`. El script omite los paquetes ya instalados.

Desde una terminal MSYS2 MINGW64, ejecutar:

```bash
bash scripts/setup-msys2.sh
```

OpenGL se provee mediante el driver de Windows. El FDM `dlfdm` esta integrado desde `libs/dlfdm/`; se conserva su codigo y licencia MIT originales. No depende de GLFW ni de OpenGL.

### Controles

- `W` / `S`: baston adelante o atras (bajar o elevar el morro).
- `A` / `D`: alabeo mediante los alerones.
- Flecha izquierda / derecha: timon de direccion.
- Flecha arriba / abajo: aumentar o reducir potencia.
- `Espacio`: pausar o reanudar la simulacion.
- Mouse izquierdo y arrastre: orbitar la camara alrededor del avion.
- Mouse derecho y arrastre: acercar o alejar la camara.
- `C`: alternar entre las tres vistas (orbital, persecucion, cabina).
- `F`: alternar el modo de dibujo en alambre.
- `Escape`: cerrar la ventana.

Los mandos de las superficies (cabeceo, alabeo y timon) se auto-centran al soltar la tecla, volviendo a su posicion de trim; la potencia se mantiene donde se la dejo. Cambian mientras se mantiene la tecla, recorren su rango más rápido que antes para facilitar la recuperación de una picada, y quedan limitados por los rangos definidos en los parametros de la aeronave. La telemetria de altitud, velocidad, pitch, roll, potencia y vista activa aparece en el titulo de la ventana y en la consola.

## Camara: tres vistas

La camara tiene tres modos que se alternan con la tecla `C`:

- Orbital: gira alrededor del avion; el mouse izquierdo orbita y el derecho acerca o aleja.
- Persecucion: sigue al avion desde atras y arriba, montada sobre su
  orientacion, de modo que acompania los bancos y el loop.
- Cabina: se coloca en el ojo del piloto, mirando al morro, solidaria con la actitud del FDM.

El mouse solo afecta a la vista orbital; las otras dos quedan fijas a la
aeronave. Al redimensionar la ventana la proyeccion se recalcula en las tres vistas por igual.

## HUD

El HUD dibuja cuatro instrumentos sobre la escena, actualizados en cada cuadro con los datos del FDM:

- Actitud: escalera de cabeceo (barras que se mueven con el cabeceo y rotan con el alabeo), simbolo de referencia fijo y arco de alabeo (escala de banqueo).
- Velocidad: cinta vertical a la izquierda con marcas y lectura numerica (m/s).
- Altitud: cinta vertical a la derecha con marcas y lectura numerica (m), más la velocidad vertical.
- Rumbo: lectura numerica centrada arriba.

Los instrumentos se dibujan por instancias (una sola llamada para todas las líneas y otra para todo el texto), el texto usa una fuente bitmap 5x7 embebida (en un atlas) y los bordes se desvanecen con mezcla de colores. El HUD se dibuja en una segunda pasada con proyeccion ortografica en pixeles, siempre por encima de la escena.

## Contacto con el terreno

En cada paso fijo se compara la posición del avión con la misma superficie triangular que se dibuja. Si la envolvente del modelo toca el relieve, se corrige la posición para mantenerlo por encima y se elimina la componente de velocidad
que apunta hacia abajo; la velocidad de ascenso se conserva. El despeje depende de la orientación, usando la caja envolvente del avión para que las alas y la cola tampoco atraviesen el suelo al inclinarse. Esta es una restricción de contacto sencilla, no un modelo de tren de aterrizaje, rebote o daño.

## Compilar y ejecutar

Desde la raiz del proyecto:

```bash
mingw32-make
mingw32-make run
mingw32-make test
mingw32-make clean
.\bin\flight-simulator.exe (en windows)
```

`make test` comprueba la conversion NED/OpenGL, los limites y pausa de mandos, la respuesta del FDM al aleron y al elevador, el contacto sobre la triangulacion del terreno, la estabilidad del trim durante 60 segundos, las tres vistas de camara (orbital, persecucion y cabina, incluido el redimensionado) y la fuente y geometria del HUD (glifos, formato de numeros y construccion de instrumentos).
Las dependencias de terceros se documentan en `third_party/README.md`; la dependencia `dlfdm` conserva su propio README y licencia en `libs/dlfdm/`.
