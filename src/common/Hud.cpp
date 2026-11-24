// Hud.cpp — la clase Hud: la duena de los recursos de GPU del HUD.
//
// Aca SI se toca OpenGL. Todo lo que crea (programas, atlas, VAO/VBO) se borra
// en esta misma clase (clear()). La geometria NO se calcula aca: viene de
// hud::build_hud() (HudGeometry.cpp), que es CPU puro y se puede probar aparte.
//
// El dibujo usa DOS llamadas instanciadas (ver Hud.h, decision [4]):
//   * LINEAS  : un VBO con un segmento de 2 vertices + un VBO con N instancias
//               (origen, fin, alfa). glDrawArraysInstanced(GL_LINES, 0, 2, N).
//   * TEXTO   : un VBO con un cuadrilatero de 6 vertices + un VBO con N glifos
//               (pos, tamano, UV, color, alfa). glDrawArraysInstanced(GL_TRIANGLES, 0, 6, N).
// Ambos usan el divisor 1 sobre el VBO de instancias (glVertexArrayBindingDivisor).

#include "Hud.h"

#include <glad/gl.h>       // funciones DSA de buffers y dibujo instanciado
#include <glm/gtc/matrix_transform.hpp>  // glm::ortho
#include <cstddef>         // offsetof (offsets de atributos)
#include <iostream>        // std::cout (reporte)

namespace {

// Registro de UNA instancia de linea (ver hud_lines.vs). Un vec4 (origen/fin)
// seguido de un float (alfa). 20 bytes, sin relleno.
struct InstanciaLinea {
    glm::vec4 linea;   // x0, y0, x1, y1 (px)
    float     alpha;   // opacidad
};
static_assert(sizeof(InstanciaLinea) == 20, "InstanciaLinea debe ocupar 20 bytes");

// Registro de UN glifo (ver hud_text.vs). 12 floats = 48 bytes, sin relleno.
struct InstanciaGlifo {
    glm::vec2 pos;      // esquina inferior-izquierda (px)
    glm::vec2 size;     // tamano (px)
    glm::vec2 uv;       // UV inferior-izquierda
    glm::vec2 uv_size;  // tamano en UV
    glm::vec3 color;    // color del texto
    float     alpha;    // opacidad
};
static_assert(sizeof(InstanciaGlifo) == 48, "InstanciaGlifo debe ocupar 48 bytes");

// Dos vertices del segmento unidad (para las lineas) y seis del cuadrilatero
// unidad (para el texto). Ambos son estaticos: se suben una sola vez en init.
const glm::vec2 kSegmento[] = {
    glm::vec2(0.0f, 0.0f),
    glm::vec2(1.0f, 0.0f),
};

const glm::vec2 kCuadrilatero[] = {
    glm::vec2(0.0f, 0.0f),
    glm::vec2(1.0f, 0.0f),
    glm::vec2(1.0f, 1.0f),
    glm::vec2(0.0f, 0.0f),
    glm::vec2(1.0f, 1.0f),
    glm::vec2(0.0f, 1.0f),
};

} // namespace

// -----------------------------------------------------------------------------
// Destructor: libera todo lo que posee (clear()).
// -----------------------------------------------------------------------------
Hud::~Hud() {
    clear();
}

// -----------------------------------------------------------------------------
// init: compila shaders, arma el atlas y crea los VAO/VBO.
// -----------------------------------------------------------------------------
bool Hud::init(const ShaderSource& fuente_lineas, const ShaderSource& fuente_texto) {
    clear();                          // por si se llama dos veces

    // -- 1) Programas. Los fragment shaders no traen #version (igual que los
    //       iluminados); se concatena la linea de version delante.
    const std::string version = "#version 460 core\n";
    const bool ok = shader_lineas_.compile_from_sources(
                        fuente_lineas.vs, {version, fuente_lineas.fs}) &&
                    shader_texto_.compile_from_sources(
                        fuente_texto.vs, {version, fuente_texto.fs});
    if (!ok) {
        clear();
        return false;
    }

    // Ubicaciones de uniforms (una sola vez; el draw reutiliza estos enteros).
    loc_lineas_proj_  = shader_lineas_.loc("uProj");
    loc_lineas_color_ = shader_lineas_.loc("uColor");
    loc_texto_proj_   = shader_texto_.loc("uProj");

    // -- 2) Atlas de la fuente. load_2d genera mipmaps y usa GL_REPEAT (pensado
    //       para el terreno); aca se corrigen a NEAREST + CLAMP para texto nítido.
    atlas_.load_2d(hud::build_atlas());
    atlas_.set_filters(GL_NEAREST, GL_NEAREST);
    atlas_.set_wrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    // -- 3) VAO/VBO de LINEAS --------------------------------------------------
    glCreateVertexArrays(1, &lineas_vao_);
    glCreateBuffers(1, &lineas_esquinas_);
    glCreateBuffers(1, &lineas_instancias_);

    // Esquinas (estatico) en la ranura 0; instancias (dinamico) en la ranura 1.
    glNamedBufferData(lineas_esquinas_, sizeof(kSegmento), kSegmento, GL_STATIC_DRAW);
    glVertexArrayVertexBuffer(lineas_vao_, 0, lineas_esquinas_, 0, sizeof(glm::vec2));
    glVertexArrayVertexBuffer(lineas_vao_, 1, lineas_instancias_, 0, sizeof(InstanciaLinea));

    // atributo 0: a_corner (vec2) -> ranura 0, divisor 0 (por vertice).
    glVertexArrayAttribFormat(lineas_vao_, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(lineas_vao_, 0, 0);
    glEnableVertexArrayAttrib(lineas_vao_, 0);
    glVertexArrayBindingDivisor(lineas_vao_, 0, 0);

    // atributo 1: a_line (vec4) -> ranura 1, divisor 1 (por instancia).
    glVertexArrayAttribFormat(lineas_vao_, 1, 4, GL_FLOAT, GL_FALSE,
                              offsetof(InstanciaLinea, linea));
    glVertexArrayAttribBinding(lineas_vao_, 1, 1);
    glEnableVertexArrayAttrib(lineas_vao_, 1);
    glVertexArrayBindingDivisor(lineas_vao_, 1, 1);

    // atributo 2: a_alpha (float) -> ranura 1, divisor 1.
    glVertexArrayAttribFormat(lineas_vao_, 2, 1, GL_FLOAT, GL_FALSE,
                              offsetof(InstanciaLinea, alpha));
    glVertexArrayAttribBinding(lineas_vao_, 2, 1);
    glEnableVertexArrayAttrib(lineas_vao_, 2);
    glVertexArrayBindingDivisor(lineas_vao_, 1, 1);

    // -- 4) VAO/VBO de TEXTO ---------------------------------------------------
    glCreateVertexArrays(1, &texto_vao_);
    glCreateBuffers(1, &texto_esquinas_);
    glCreateBuffers(1, &texto_instancias_);

    glNamedBufferData(texto_esquinas_, sizeof(kCuadrilatero), kCuadrilatero, GL_STATIC_DRAW);
    glVertexArrayVertexBuffer(texto_vao_, 0, texto_esquinas_, 0, sizeof(glm::vec2));
    glVertexArrayVertexBuffer(texto_vao_, 1, texto_instancias_, 0, sizeof(InstanciaGlifo));

    // atributo 0: a_corner (vec2) -> ranura 0, divisor 0.
    glVertexArrayAttribFormat(texto_vao_, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(texto_vao_, 0, 0);
    glEnableVertexArrayAttrib(texto_vao_, 0);
    glVertexArrayBindingDivisor(texto_vao_, 0, 0);

    // atributos 1..6 -> ranura 1 (por instancia).
    struct Atributo { unsigned int loc; int comp; std::size_t off; };
    const Atributo atributos[] = {
        {1U, 2, offsetof(InstanciaGlifo, pos)},
        {2U, 2, offsetof(InstanciaGlifo, size)},
        {3U, 2, offsetof(InstanciaGlifo, uv)},
        {4U, 2, offsetof(InstanciaGlifo, uv_size)},
        {5U, 3, offsetof(InstanciaGlifo, color)},
        {6U, 1, offsetof(InstanciaGlifo, alpha)},
    };
    for (const Atributo& at : atributos) {
        glVertexArrayAttribFormat(texto_vao_, at.loc, at.comp, GL_FLOAT, GL_FALSE,
                                  static_cast<unsigned int>(at.off));
        glVertexArrayAttribBinding(texto_vao_, at.loc, 1);
        glEnableVertexArrayAttrib(texto_vao_, at.loc);
    }
    glVertexArrayBindingDivisor(texto_vao_, 1, 1);

    std::cout << "[Hud] listo: fuente " << hud::atlas_width() << 'x'
              << hud::atlas_height() << " px\n";
    return true;
}

// -----------------------------------------------------------------------------
// set_state: guarda los datos de vuelo para el proximo draw().
// -----------------------------------------------------------------------------
void Hud::set_state(const HudState& estado) {
    estado_ = estado;
}

// -----------------------------------------------------------------------------
// draw: genera la geometria, la sube y la dibuja (dos pasadas de GL).
// -----------------------------------------------------------------------------
void Hud::draw(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;                       // ventana minimizada: nada que dibujar
    }

    // -- 1) Geometria (CPU puro): estado + viewport -> lineas y glifos.
    std::vector<hud::Line> lineas;
    std::vector<hud::Glyph> glifos;
    hud::build_hud(estado_, width, height, lineas, glifos);

    // Convierto a los registros de instancia (los formatos que espera la GPU).
    std::vector<InstanciaLinea> inst_lineas;
    inst_lineas.reserve(lineas.size());
    for (const hud::Line& l : lineas) {
        inst_lineas.push_back({
            glm::vec4(l.a.x, l.a.y, l.b.x, l.b.y), l.alpha});
    }
    std::vector<InstanciaGlifo> inst_glifos;
    inst_glifos.reserve(glifos.size());
    for (const hud::Glyph& g : glifos) {
        inst_glifos.push_back({g.pos, g.size, g.uv, g.uv_size, g.color, g.alpha});
    }

    const glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(width),
                                      0.0f, static_cast<float>(height));

    // -- 2) Guardar el estado de GL para restaurarlo al terminar (dos pasadas).
    const bool depth_on = glIsEnabled(GL_DEPTH_TEST);
    const bool cull_on  = glIsEnabled(GL_CULL_FACE);
    const bool blend_on = glIsEnabled(GL_BLEND);
    GLint poly_mode = GL_FILL;
    glGetIntegerv(GL_POLYGON_MODE, &poly_mode);

    // El HUD va siempre encima (sin depth), sin cull (es 2D) y con mezcla.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // -- 3) LINEAS (una sola llamada instanciada).
    if (!inst_lineas.empty()) {
        glNamedBufferData(lineas_instancias_,
                          static_cast<GLsizeiptr>(inst_lineas.size() * sizeof(InstanciaLinea)),
                          inst_lineas.data(), GL_DYNAMIC_DRAW);
        shader_lineas_.use();
        shader_lineas_.set_uniform(loc_lineas_proj_, proj);
        shader_lineas_.set_uniform(loc_lineas_color_, hud::color_hud());
        glBindVertexArray(lineas_vao_);
        glDrawArraysInstanced(GL_LINES, 0, 2,
                              static_cast<GLsizei>(inst_lineas.size()));
    }

    // -- 4) TEXTO (otra llamada instanciada; muestrea el atlas).
    if (!inst_glifos.empty()) {
        glNamedBufferData(texto_instancias_,
                          static_cast<GLsizeiptr>(inst_glifos.size() * sizeof(InstanciaGlifo)),
                          inst_glifos.data(), GL_DYNAMIC_DRAW);
        shader_texto_.use();
        shader_texto_.set_uniform(loc_texto_proj_, proj);
        atlas_.bind(0U);                    // sampler layout(binding=0)
        glBindVertexArray(texto_vao_);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6,
                              static_cast<GLsizei>(inst_glifos.size()));
    }

    // -- 5) Restaurar el estado previo de GL.
    glBindVertexArray(0);
    if (depth_on) glEnable(GL_DEPTH_TEST);
    if (cull_on)  glEnable(GL_CULL_FACE);
    if (blend_on) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(poly_mode));
}

// -----------------------------------------------------------------------------
// clear: libera todos los recursos de GPU. Idempotente.
// -----------------------------------------------------------------------------
void Hud::clear() {
    if (lineas_vao_ != 0U)        { glDeleteVertexArrays(1, &lineas_vao_);         lineas_vao_ = 0U; }
    if (lineas_esquinas_ != 0U)   { glDeleteBuffers(1, &lineas_esquinas_);          lineas_esquinas_ = 0U; }
    if (lineas_instancias_ != 0U) { glDeleteBuffers(1, &lineas_instancias_);        lineas_instancias_ = 0U; }
    if (texto_vao_ != 0U)         { glDeleteVertexArrays(1, &texto_vao_);           texto_vao_ = 0U; }
    if (texto_esquinas_ != 0U)    { glDeleteBuffers(1, &texto_esquinas_);           texto_esquinas_ = 0U; }
    if (texto_instancias_ != 0U)  { glDeleteBuffers(1, &texto_instancias_);         texto_instancias_ = 0U; }

    shader_lineas_.clear();
    shader_texto_.clear();
    atlas_.clear();

    loc_lineas_proj_  = -1;
    loc_lineas_color_ = -1;
    loc_texto_proj_   = -1;
}
