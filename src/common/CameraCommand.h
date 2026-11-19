// CameraCommand.h
// -----------------------------------------------------------------------------
// CameraCommand — el comando que el USUARIO produce para mover la cámara, en
// COORDENADAS ESFÉRICAS. Es un struct de DATOS puro.
//
// La cámara orbital se describe con tres números alrededor de un objetivo:
//   * yaw   : ángulo de LONGITUD sobre la órbita (girar alrededor del objetivo).
//   * pitch : ángulo de LATITUD (subir/bajar el punto de vista).
//   * dist  : distancia al objetivo (acercar/alejar).
//
// Este struct NO trae los valores absolutos de la órbita, sino los CAMBIOS
// (deltas) acumulados durante UN cuadro. Los produce el módulo que lee el mouse
// (InputHandler) y los consume CameraSystem::update(), que los suma a su estado
// interno. Esa separación mantiene la lectura de entrada (px -> rad) FUERA del
// cálculo de matrices (rad -> view).
//
// Convención de signos:
//   * yaw_delta  > 0 : rotar la cámara en sentido antihorario (visto de arriba).
//   * pitch_delta > 0 : subir la cámara (mirar el objetivo más desde arriba).
//   * dist_delta > 0 : ALEJAR la cámara del objetivo (distancia mayor).
// -----------------------------------------------------------------------------
#pragma once                          // se incluye una sola vez por archivo

// CameraCommand — el comando del usuario para la cámara, acumulado en un cuadro.
struct CameraCommand {
    float yaw_delta   = 0.0f;   // [rad]  cambio de longitud (acimut), en el cuadro
    float pitch_delta = 0.0f;   // [rad]  cambio de latitud (elevación), en el cuadro
    float dist_delta  = 0.0f;   // [unidades de escena] (+) se aleja del objetivo
};
