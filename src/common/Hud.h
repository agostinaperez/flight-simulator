// Hud.h
// -----------------------------------------------------------------------------
// HUD (Head-Up Display): los instrumentos de vuelo dibujados SOBRE la escena.
//
// La etapa 5 pide "mostrar y actualizar al menos tres instrumentos" en pantalla.
// Este modulo dibuja cuatro: ACTITUD (escalera de cabeceo + arco de alabeo),
// VELOCIDAD (cinta lateral), ALTITUD (cinta lateral) y RUMBO (texto), con los
// valores numericos como texto. Sigue las ideas de la presentacion 12:
//   * Dibujo por INSTANCIAS: un solo segmento para todas las lineas y un solo
//     cuadrilatero para todos los glifos; cada copia llega como un registro por
//     instancia (metodo 2: atributos con divisor 1).
//   * TEXTO con fuente bitmap: los glifos se rasterizan UNA vez en un atlas
//     (imagen), y cada caracter es un cuadrilatero que muestrea su rectangulo.
//   * MEZCLA DE COLORES (blending): el alfa del atlas y el desvanecido de los
//     bordes de las cintas se combinan con la escena.
//   * DOS PASADAS: la escena se dibuja primero (perspectiva) y el HUD despues,
//     con proyeccion ORTOGRAFICA en pixeles (glm::ortho(0, w, 0, h)).
//
// SEPARACION "datos / GPU" (igual que Primitives vs Mesh):
//   * La geometria es CPU puro y vive en HudGeometry.cpp: hud::build_hud(...)
//     genera las lineas y los glifos a partir de un HudState. Se puede probar
//     sin OpenGL (ver tests/).
//   * La clase Hud (Hud.cpp) es la duena de los recursos de GPU (shaders, atlas,
//     VAO/VBO) y solo sube y dibuja lo que build_hud ya calculo.
//
// DECISIONES DE DISEÑO:
// ---------------------------------------------------------------
// [1] ¿Por que una fuente embebida en vez de un archivo .fnt/.png?
//     -> No hay una fuente BMFont en el proyecto y generarla en el codigo evita
//        una dependencia externa y un formato de archivo mas. La fuente 5x7
//        cubre digitos, mayusculas y algunos simbolos (los que usa el HUD).
//        Reemplazarla por una fuente real es solo cambiar build_atlas() y la
//        tabla de glifos: la interfaz (hud::glyph_uv / atlas) no cambia.
// [2] ¿Por que ORTOGRAFICA en pixeles (0..w, 0..h)?
//     -> Asi 1 unidad = 1 pixel y las posiciones del HUD coinciden con la
//        ventana. Al redimensionar, build_hud recibe el nuevo tamano y todo se
//        recoloca; no hay estiramiento porque el ancho y alto visibles son los
//        reales del framebuffer (a diferencia de una orto FIJADA a un tamano).
// [3] ¿Por que el HUD se dibuja con GL_DEPTH_TEST apagado y blending encendido?
//     -> El HUD va siempre POR ENCIMA de la escena (no debe ocluirse), y sus
//        bordes se mezclan con el fondo. draw() guarda el estado previo y lo
//        restaura al terminar, como pide la presentacion (dos pasadas).
// [4] ¿Por que dos llamadas de dibujo (lineas y texto) y no una sola?
//     -> Son dos formatos de instancia distintos (una linea lleva 2 extremos +
//        alfa; un glifo lleva posicion, tamano, UV, color y alfa). Juntarlos
//        forzaria a un registro "unico" mas grande y desperdiciado. Con dos
//        llamadas el HUD completo sigue siendo O(1) llamadas por cuadro.
// [5] Los valores numericos usan glifos monoespaciados (avance fijo 6 celdas).
//     -> Facilita el layout y alinea bien las cifras de los instrumentos.
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>                 // glm::vec2 / glm::vec3
#include <string>                      // std::string (textos)
#include <vector>                      // std::vector (lineas y glifos)

#include "ResourceManager.h"           // ShaderSource (para Hud::init)
#include "Shader.h"                    // los programas del HUD
#include "Texture.h"                   // el atlas de la fuente

// HudState — los datos de vuelo que muestra el HUD (unidades de pantalla).
// Lo arma el main a partir del estado del FDM (dlfdm::AircraftState). No sabe
// nada de GPU: es el "contrato" entre el vuelo y el dibujo del HUD.
struct HudState {
    float pitch_deg   = 0.0f;   // cabeceo (positivo = nariz arriba)
    float roll_deg    = 0.0f;   // alabeo (positivo = ala derecha abajo)
    float heading_deg = 0.0f;   // rumbo (0..360)
    float altitude_m  = 0.0f;   // altitud en metros
    float speed_ms    = 0.0f;   // velocidad (TAS) en m/s
    float vspeed_ms   = 0.0f;   // velocidad vertical en m/s (positivo = sube)
};

namespace hud {

// Line — un segmento del HUD, en pixeles (para el dibujo instanciado de lineas).
struct Line {
    glm::vec2 a;                   // extremo inicial (px)
    glm::vec2 b;                   // extremo final (px)
    float     alpha = 1.0f;        // opacidad (1 = opaco, 0 = desvanecido)
};

// Glyph — un glifo (caracter) de texto para el dibujo instanciado de texto.
struct Glyph {
    glm::vec2 pos;                 // esquina inferior-izquierda (px)
    glm::vec2 size;                // tamano en pixeles
    glm::vec2 uv;                  // UV inferior-izquierda dentro del atlas
    glm::vec2 uv_size;             // tamano del glifo en UV
    glm::vec3 color;               // color del texto (RGB)
    float     alpha = 1.0f;        // opacidad
};

// Fuente bitmap 5x7: caracteres soportados y sus metricas.
//   glyph_uv(c)      : true y llena u0/v0/du/dv si el caracter esta en la fuente.
//   glyph_advance(c) : cuantas CELDAS avanza el cursor tras el caracter (fijo 6).
//   atlas_width/height : tamano del atlas en pixeles (lo usa Hud para la textura).
bool glyph_uv(char c, float& u0, float& v0, float& du, float& dv);
float glyph_advance(char c);
int atlas_width();
int atlas_height();

// build_atlas: rasteriza TODOS los glifos en una imagen RGBA (una fila). La
// "tinta" va en el canal ALFA (blanco en RGB). La clase Hud la sube como textura.
ImageData build_atlas();

// color_hud: el color del HUD (rojo, para que contraste con cielo y terreno).
// Lo usan tanto la geometria (color de los glifos) como el shader de lineas
// (uniform uColor).
glm::vec3 color_hud();

// format_number: convierte un numero a texto con decimales fijos (0 o 1). Usa
// '.' como separador decimal (formato "C") para que la fuente lo pueda dibujar.
std::string format_number(float valor, int decimales);

// text_width: ancho en pixeles de un texto a una escala dada (monoespaciado).
float text_width(const std::string& texto, float escala);

// build_text: convierte un texto en una lista de glifos, con su color y escala.
//   origen : esquina inferior-izquierda del texto (px).
//   escala : factor de tamano (1 = glifo de 5x7 px, 2 = 10x14, ...).
void build_text(const std::string& texto, glm::vec2 origen, float escala,
                glm::vec3 color, std::vector<Glyph>& out);

// build_hud: genera TODA la geometria del HUD para un estado y un viewport.
//   estado          : datos de vuelo.
//   width, height   : tamano del framebuffer en pixeles.
//   lines, glyphs   : salidas (se limpian al entrar).
// Es CPU puro: no toca OpenGL, por eso se puede probar en los tests.
void build_hud(const HudState& estado, int width, int height,
               std::vector<Line>& lines, std::vector<Glyph>& glyphs);

} // namespace hud

// Hud — la duena de los recursos de GPU del HUD (shaders, atlas, buffers).
// Se usa como objeto local en el main (igual que Aircraft): no es copiable.
class Hud {
public:
    Hud() = default;
    ~Hud();

    // Sin copias: dos dueños de los mismos nombres de GPU = doble borrado.
    Hud(const Hud&) = delete;
    Hud& operator=(const Hud&) = delete;

    // init: compila los shaders, arma el atlas de la fuente y crea los VAO/VBO.
    //   fuente_lineas : ShaderSource del programa de lineas (hud_lines).
    //   fuente_texto  : ShaderSource del programa de texto (hud_text).
    // Devuelve false si algun shader no compila (el log ya se imprimio).
    bool init(const ShaderSource& fuente_lineas, const ShaderSource& fuente_texto);

    // set_state: guarda los datos de vuelo; se consumen en el proximo draw().
    void set_state(const HudState& estado);

    // draw: dibuja el HUD completo con el viewport actual (en pixeles).
    //   width, height : tamano del framebuffer (para la orto y el layout).
    void draw(int width, int height);

private:
    // clear: libera VAO/VBO/programas/atlas. Idempotente (la llama el destructor).
    void clear();

    Shader shader_lineas_;          // programa de lineas instanciadas
    Shader shader_texto_;           // programa de texto instanciado
    Texture atlas_;                 // atlas de la fuente 5x7

    // Objetos del dibujo de LINEAS (un VAO + VBO de esquinas + VBO de instancias).
    unsigned int lineas_vao_       = 0U;
    unsigned int lineas_esquinas_  = 0U;
    unsigned int lineas_instancias_ = 0U;

    // Objetos del dibujo de TEXTO (idem).
    unsigned int texto_vao_        = 0U;
    unsigned int texto_esquinas_   = 0U;
    unsigned int texto_instancias_ = 0U;

    // Ubicaciones de uniforms (se consultan una vez en init).
    int loc_lineas_proj_  = -1;
    int loc_lineas_color_ = -1;
    int loc_texto_proj_   = -1;

    HudState estado_{};             // ultimos datos de vuelo recibidos
};
