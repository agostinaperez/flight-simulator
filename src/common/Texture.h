// Texture.h
// -----------------------------------------------------------------------------
// Módulo Texture — el DUEÑO ÚNICO de una textura de OpenGL
//
// Las texturas se trabajan igual que los buffers: hay que reservar espacio en
// memoria de GPU y, al terminar, liberarlo. Por eso siguen las MISMAS tres
// reglas de propiedad que Mesh:
//   * El destructor libera el identificador (glDeleteTextures vive SOLO acá).
//   * Copiar está PROHIBIDO (= delete): dos dueños del mismo nombre = doble
//     liberación.
//   * Mover SÍ se permite: transfiere el nombre y deja al origen en cero.
//
// DECISIONES DE DISEÑO:
// ---------------------------------------------------------------
// [1] ¿Quién decodifica la imagen? → NO esta clase. Leer y decodificar la imagen
//     es tarea del CPU (C++ puro) y la hace ResourceManager::load_image(). Esta
//     clase recibe un `ImageData` YA decodificado y solo sube los bytes a la GPU.
//     Es la misma separación que hay entre MeshData (datos) y Mesh (GPU).
// [2] Una sola clase para 2D y cubemap → Sí. Ambos son "un nombre de textura";
//     solo cambia la forma de cargar (load_2d / load_cubemap). Una sola clase
//     evita duplicar la maquinaria de ownership (destructor, move, clear).
// [3] ¿Cuándo se invierte la imagen? → En la CARGA (ResourceManager), no acá. El
//     flag flip_vertically decide si stb_image invierte las filas al decodificar:
//     una textura 2D necesita flip=true (OpenGL tiene el origen abajo-izquierda)
//     y las caras de un cubemap NO se invierten (convención RenderMan, origen
//     arriba-izquierda). La alternativa (invertir `v` en el shader) se descarta
//     para no tener que recordar invertir una sola de las dos texturas.
// [4] Parámetros de muestreo (filtros y wrap) → se fijan dentro de load_2d /
//     load_cubemap según el uso de cada tipo, y se pueden CAMBIAR después con
//     set_filters() / set_wrap(). El terreno necesita GL_REPEAT + mipmaps; el
//     cubemap necesita GL_CLAMP_TO_EDGE para no ver costuras en las aristas.
// [5] Las 4 etapas (crear, reservar, llenar, parametrizar) se hacen con DSA
//     (glCreateTextures / glTextureStorage2D / glTextureSubImage2D / ...), sin
//     glGenTextures ni glBindTexture. Misma técnica que los buffers del Mesh.
// -----------------------------------------------------------------------------
#pragma once                          // se incluye una sola vez por archivo

#include <array>                      // std::array (las 6 caras de un cubemap)
#include <stdexcept>                  // std::runtime_error (errores no en silencio)

#include "ResourceManager.h"          // ImageData: la imagen ya decodificada

// Texture — dueño único de una textura de OpenGL (2D o cubemap).
class Texture {
public:
    // Constructor por defecto: objeto VACÍO (sin nombre de textura).
    Texture() = default;

    // Destructor: libera la textura si este objeto la posee (clear()).
    ~Texture();

    // Copiar NO existe (dueño único); mover SÍ transfiere la propiedad.
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // load_2d: sube una textura 2D desde una imagen ya decodificada.
    // Las 4 etapas (crear, reservar, llenar, parametrizar) con DSA. Reserva los
    // niveles de mipmap (1 + floor(log2(max(w,h)))) y los genera con
    // glGenerateTextureMipmap. Filtros: mipmap+linear para reducción, linear
    // para ampliación, y GL_REPEAT en S y T (adecuado para el terreno).
    void load_2d(const ImageData& image);

    // load_cubemap: sube las 6 caras en el orden +X -X +Y -Y +Z -Z (las
    // constantes GL_TEXTURE_CUBE_MAP_POSITIVE_X .. NEGATIVE_Z). Las caras llegan
    // SIN invertir (al revés que una 2D, ver decisión [3]). Almacenamiento GL_RGB8
    // (los datos RGBA se convierten) y GL_CLAMP_TO_EDGE en las 3 direcciones.
    void load_cubemap(const std::array<const ImageData*, 6>& faces);

    // bind: conecta la textura a una unidad de textura (0, 1, ...) para el
    // próximo dibujo. El sampler del shader apunta a esa unidad (layout binding).
    void bind(unsigned int unit) const;

    // set_filters / set_wrap: cambian el muestreo de una textura ya creada
    // (decisión [4]). Los valores son las constantes de OpenGL (GL_LINEAR, etc.)
    // declaradas como int para no arrastrar glad a este header.
    void set_filters(int min_filter, int mag_filter);
    void set_wrap(int wrap_s, int wrap_t, int wrap_r);

    // clear: libera la textura y deja el objeto en cero. Idempotente.
    void clear(void);

    // id: el nombre (entero) de la textura. Se presta para usar, no para borrar.
    unsigned int id(void) const { return id_; }

private:
    unsigned int id_ {0U};            // nombre de la textura en la GPU (0 = vacío)
};
