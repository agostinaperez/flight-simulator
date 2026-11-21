// CameraSystem.h
// -----------------------------------------------------------------------------
// CameraSystem — el ADMINISTRADOR del estado de la cámara. Es un módulo de C++
// puro: NO lee el mouse, NO dibuja y NO usa OpenGL. Solo:
//   * guarda el estado de la vista activa (orbital, persecución o cabina),
//   * recibe los deltas del usuario (CameraCommand) y la pose de la aeronave,
//   * calcula la matriz de VISTA con glm::lookAt según el modo activo, y
//   * calcula/actualiza la matriz de PROYECCIÓN con glm::perspective.
//
// Por qué agrupar esto en un módulo (en vez de dejarlo en el main):
//   * El resto del programa no necesita saber qué TIPO de cámara es (orbital,
//     de cabina, de persecución): solo pide, por cuadro, el par de matrices
//     (CameraData) para completar el proceso de proyección (vista V + proyección
//     P). Cambiar de vista es solo cambiar el MODO interno: la interfaz hacia
//     el main (update + data) no se modifica.
//
// LAS TRES VISTAS (requerimiento de la consigna: "sistema de cámaras, 3 vistas"):
// ---------------------------------------------------------------
//   * Orbital: la cámara gira alrededor del avión (yaw/pitch/distancia). Es la
//     vista para inspeccionar la escena y la actitud del avión desde afuera.
//   * Persecución: la cámara sigue al avión desde atrás y arriba, mirándolo.
//     Se monta sobre la orientación de la aeronave, así que "acompaña" las
//     maniobras (bancos, loop) mostrando la actitud desde un punto externo.
//   * Cabina: la cámara se coloca en el ojo del piloto (dentro de la cabina) y
//     mira hacia el morro, solidaria con la actitud del avión. Es la vista
//     subjetiva que usa el piloto para volar.
//
// DECISIONES DE DISEÑO (las "cuestiones a pensar"):
// ---------------------------------------------------------------
// [1] ¿Por qué update() recibe la ORIENTACIÓN (matriz) y no los ángulos?
//     → La vista orbital no la usa (siempre mira al objetivo), pero las vistas
//       de persecución y cabina necesitan la dirección del MORRO y el vector
//       "arriba" del avión en el mundo. Esos dos vectores salen de la matriz de
//       orientación modelo->mundo: forward = O·(0,0,-1) y up = O·(0,1,0). Pasar
//       ángulos de Euler obligaría a re-armar la matriz acá (duplicando la
//       lógica de orientacion_vuelo) o a importar FlightData.h. La matriz ya la
//       calcula el main una vez por cuadro, así que se recibe y se usa directo.
//       La vista orbital la ignora (documentado en update()).
// [2] ¿Dónde se evita la división por cero de aspect (ventana minimizada)?
//     → En set_viewport(): si el alto llega a 0 (o es negativo, que pasa al
//       minimizar), se usa 1 como alto para el cálculo. La proyección queda
//       "deforme" pero definida; en cuanto se restaura la ventana, se recalcula
//       bien. Ver el comentario dentro de set_viewport().
// [3] La proyección en perspectiva usa fovy en RADIANES. Pasar grados no da
//     error de compilación: da pantalla negra (el ángulo queda 45x más grande
//     de lo esperado y todo cae fuera del frustum). Por eso se escribe
//     glm::radians(45.0f).
// [4] El pitch de la vista orbital se acota a +-0.9·(π/2). En ±π/2 la cámara
//     quedaría JUSTO sobre el eje vertical, con la dirección de vista y el
//     vector "up" colineales: el producto vectorial interno de lookAt se hace
//     cero y la matriz queda indefinida (salto de imagen). Acotar antes evita
//     el caso degenerado. Las vistas de persecución y cabina NO tienen este
//     problema: su "up" es el vector arriba del avión, que por ser una matriz
//     de rotación propia NUNCA es paralelo a la dirección de vista (forward y
//     up del cuerpo son ortogonales y la rotación preserva la ortogonalidad).
// [5] ¿Quién consume el mouse (CameraCommand)?
//     → Solo la vista orbital. Las vistas de persecución y cabina quedan fijas
//       a la aeronave: la posición de la cámara se deriva entera de la pose del
//       avión, así que arrastrar el mouse no tendría sentido. El comando se
//       recibe igual (interfaz única) y se ignora en esos modos.
// [6] ¿Por qué cycle_mode() no recibe la tecla?
//     → La cámara NO lee el teclado (módulo puro). El main detecta la tecla y
//       llama a cycle_mode(); acá solo se avanza al modo siguiente y se informa
//       por consola. Separar "quién detecta" de "qué cambia" mantiene a
//       CameraSystem sin dependencia de GLFW.
// -----------------------------------------------------------------------------
#pragma once                          // se incluye una sola vez por archivo

#include <glm/glm.hpp>                // glm::mat4 / glm::vec3

#include "CameraData.h"               // el par view/projection que entrega data()
#include "CameraCommand.h"            // los deltas que recibe update()

// CameraMode — los tres tipos de vista que puede tomar la cámara.
enum class CameraMode {
    Orbital,   // gira alrededor del avión (control con el mouse)
    Chase,     // sigue al avión desde atrás y arriba (persecución)
    Cockpit,   // vista desde la cabina, solidaria con la actitud del avión
};

// CameraSystem — administra el estado de la cámara y sus tres vistas.
class CameraSystem {
public:
    // Constructor: guarda el tamaño inicial de la ventana y arma la proyección
    // inicial (aspect = width/height). Sin estado de órbita aún (valores por
    // defecto de yaw/pitch/distancia) y en modo Orbital.
    CameraSystem(int width, int height);

    // update: recalcula, por cuadro, la matriz de VISTA según el modo activo.
    //   pos_avion           : dónde está el objetivo (punto de referencia del avión).
    //   orientacion_avion   : matriz modelo->mundo de la aeronave. La usa para
    //                         sacar forward y up; la vista orbital la ignora.
    //   cmd                 : los deltas del usuario (yaw/pitch/dist) de este
    //                         cuadro. Solo la vista orbital los consume.
    void update(glm::vec3 pos_avion, const glm::mat4& orientacion_avion,
                const CameraCommand& cmd);

    // set_viewport: al redimensionar la ventana, recalcula la PROYECCIÓN con el
    // nuevo aspect (el fov, near y far no cambian; solo el aspect). La VISTA no
    // depende de la ventana, así que este método es independiente del modo.
    void set_viewport(int width, int height);

    // cycle_mode: avanza al modo siguiente (Orbital -> Chase -> Cockpit ->
    // Orbital) e informa por consola la vista nueva.
    void cycle_mode();

    // mode: la vista activa (para el HUD / telemetría si hiciera falta).
    CameraMode mode() const { return modo_; }

    // mode_name: nombre legible del modo activo, para títulos y consola.
    const char* mode_name() const;

    // data: el par de matrices (view + projection) para mandar al shader.
    const CameraData& data() const { return data_; }

private:
    // Calcula la vista ORBITAL: posiciona la cámara sobre la esfera alrededor
    // del avión (yaw/pitch/distancia) y mira al avión con "up" de mundo.
    void update_orbital(glm::vec3 pos_avion, const CameraCommand& cmd);

    // Calcula la vista de PERSECUCIÓN: cámara atrás y arriba del avión, montada
    // sobre su orientación, mirando un punto algo más adelante del morro.
    void update_chase(glm::vec3 pos_avion, const glm::mat4& orientacion);

    // Calcula la vista de CABINA: cámara en el ojo del piloto, mirando al morro.
    void update_cockpit(glm::vec3 pos_avion, const glm::mat4& orientacion);

    CameraMode modo_ = CameraMode::Orbital;  // vista activa (arranca en orbital)

    // Estado de la órbita (los tres números que describen a la cámara orbital).
    float yaw_       = 0.0f;   // [rad] longitud sobre la órbita
    float pitch_     = 0.0f;   // [rad] latitud (acotada, ver decisión [4])
    float distancia_ = 35.0f;  // [m] distancia inicial para encuadrar el avión y el terreno

    // Parámetros de la proyección en perspectiva (fijos salvo aspect).
    float fovy_ = glm::radians(45.0f);   // ángulo de visión vertical (radianes)
    float near_ = 0.1f;                  // plano de recorte cercano
    float far_  = 10000.0f;              // el mundo del FDM está expresado en metros

    CameraData data_{};                  // el par view/projection ya calculado
};
