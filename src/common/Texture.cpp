// Texture.cpp — implementación del módulo Texture. Acá SÍ se toca la API de
// OpenGL. Todo lo que crea (el nombre de textura) se borra en esta misma clase.

#include "Texture.h"

#include <glad/gl.h>                  // las funciones DSA de textura
#include <cmath>                      // std::log2 / std::floor (niveles de mipmap)
#include <algorithm>                  // std::max (lado más largo de la imagen)

// -----------------------------------------------------------------------------
// Destructor: libera lo que este objeto posee.
// -----------------------------------------------------------------------------
Texture::~Texture() {
    clear();
}

// -----------------------------------------------------------------------------
// load_2d — textura 2D con mipmaps, para el terreno.
// -----------------------------------------------------------------------------
void Texture::load_2d(const ImageData& image) {
    // Errores no en silencio: una imagen vacía no define una textura.
    if (image.width <= 0 || image.height <= 0 || image.pixels.empty()) {
        throw std::runtime_error("Texture::load_2d: la imagen está vacía.");
    }

    clear();                          // el almacenamiento no se reserva dos veces

    // ETAPA 1 (crear) y 2 (reservar). La cantidad de niveles de mipmap se calcula
    // como 1 + floor(log2(max(w,h))): 9 para 256 píxeles, 11 para 1024. El
    // almacenamiento es INMUTABLE: una vez reservado, tamaño y formato no cambian.
    const int niveles = 1 + static_cast<int>(std::floor(std::log2(
        static_cast<float>(std::max(image.width, image.height)))));
    glCreateTextures(GL_TEXTURE_2D, 1, &id_);
    glTextureStorage2D(id_, niveles, GL_RGBA8, image.width, image.height);

    // ETAPA 3 (llenar): subo el nivel 0 y dejo que OpenGL genere el resto.
    //   GL_RGBA + GL_UNSIGNED_BYTE describe el FORMATO DE LOS DATOS (los bytes ya
    //   decodificados); GL_RGBA8 (etapa 2) es cómo se GUARDA en la GPU. Pueden no
    //   coincidir y OpenGL se encarga de convertir.
    glTextureSubImage2D(id_, 0, 0, 0, image.width, image.height,
                        GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
    glGenerateTextureMipmap(id_);

    // ETAPA 4 (parametrizar): reducción con mipmaps + trilineal, ampliación
    // lineal y GL_REPEAT en S y T (la textura se repite sobre el terreno).
    glTextureParameteri(id_, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(id_, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(id_, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

// -----------------------------------------------------------------------------
// load_cubemap — las 6 caras del cielo. Orden +X -X +Y -Y +Z -Z.
// -----------------------------------------------------------------------------
void Texture::load_cubemap(const std::array<const ImageData*, 6>& faces) {
    for (const ImageData* cara : faces) {
        if (cara == nullptr || cara->width <= 0 || cara->height <= 0) {
            throw std::runtime_error("Texture::load_cubemap: una cara es nula o vacía.");
        }
    }

    clear();
    const int w = faces[0]->width, h = faces[0]->height;

    // El cubemap se RESERVA como una textura 2D (un solo nivel, sin mipmaps: el
    // cielo no se reduce porque no tiene parallax) y se LLENA como si fuese una
    // capa de una textura 3D: el índice de capa (cara) indica a qué cara del cubo
    // corresponde cada imagen.
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &id_);
    glTextureStorage2D(id_, 1, GL_RGB8, w, h);

    for (int cara = 0; cara < 6; ++cara) {
        // Las caras NO se invierten al leerlas (flip_vertically = false), porque
        // un cubemap usa la convención RenderMan (origen arriba-izquierda), que
        // coincide con el orden en que stb_image entrega las filas.
        glTextureSubImage3D(id_, 0, 0, 0, cara, w, h, 1,
                            GL_RGBA, GL_UNSIGNED_BYTE, faces[cara]->pixels.data());
    }

    // Sin mipmaps (LINEAR) y con GL_CLAMP_TO_EDGE en las TRES direcciones: con
    // GL_REPEAT el filtrado tomaría texels del borde opuesto y se verían líneas
    // de cambio de imagen en las aristas del cubo.
    glTextureParameteri(id_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(id_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

// -----------------------------------------------------------------------------
// bind — conecta esta textura a una unidad de textura (DSA).
// -----------------------------------------------------------------------------
void Texture::bind(unsigned int unit) const {
    glBindTextureUnit(unit, id_);
}

// -----------------------------------------------------------------------------
// set_filters / set_wrap — reconfigura el muestreo (decisión [4]).
// -----------------------------------------------------------------------------
void Texture::set_filters(int min_filter, int mag_filter) {
    glTextureParameteri(id_, GL_TEXTURE_MIN_FILTER, min_filter);
    glTextureParameteri(id_, GL_TEXTURE_MAG_FILTER, mag_filter);
}

void Texture::set_wrap(int wrap_s, int wrap_t, int wrap_r) {
    glTextureParameteri(id_, GL_TEXTURE_WRAP_S, wrap_s);
    glTextureParameteri(id_, GL_TEXTURE_WRAP_T, wrap_t);
    glTextureParameteri(id_, GL_TEXTURE_WRAP_R, wrap_r);
}

// -----------------------------------------------------------------------------
// clear — libera la textura y deja el objeto en cero. Idempotente.
// -----------------------------------------------------------------------------
void Texture::clear(void) {
    if (id_ != 0U) {
        glDeleteTextures(1, &id_);
        id_ = 0U;
    }
}

// -----------------------------------------------------------------------------
// Movimiento: el nuevo dueño se lleva el nombre y el origen queda en cero.
// -----------------------------------------------------------------------------
Texture::Texture(Texture&& other) noexcept
    : id_(other.id_) {
    other.id_ = 0U;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        clear();
        id_ = other.id_;
        other.id_ = 0U;
    }
    return *this;
}
