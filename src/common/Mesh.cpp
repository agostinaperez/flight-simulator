// Mesh.cpp — implementación del módulo Mesh. Acá SÍ se toca la API de OpenGL.
// Todo lo que crea (VAO, VBO, EBO) se borra en esta misma clase, y en ningún
// otro lado del programa.

#include "Mesh.h"                        

#include <glad/gl.h>                   
#include <cstddef>                       // offsetof: calcula desplazamientos


// Destructor: libera lo que este objeto posee.
// Cualquier Mesh muere acá; si quedó en cero (movida o clear()), no hace nada.
Mesh::~Mesh() {
    clear();                             // borra VAO/VBO/EBO si existen
}                                        

// load: sube la malla a la GPU y arma el VAO, el VBO y el EBO.
// Llamarla dos veces: libera lo anterior (clear) y vuelve a armar todo.
void Mesh::load(const MeshData& data) {
    // -- Si no hay vértices o no hay índices, no hay nada que subir 
    if (data.vertices.empty() || data.indices.empty()) {  
        count_ = 0;                                        
        return;                                            
    }                                                      

    clear(); 

    // -- 1) Crear y subir el VBO (bytes de los vértices) --
    glCreateBuffers(1, &vbo_);           // pido 1 nombre de buffer nuevo a la GPU
    // Subo TODA la lista a VRAM con glNamedBufferData (DSA, no hace falta bindear):
    //   - tamaño = vértices * bytes que ocupa cada Vertex (el buffer entrelazado)
    //   - datos  = puntero al comienzo de la lista (std::vector es contigua)
    //   - GL_STATIC_DRAW = pista al driver: se escribe una vez, se dibuja mucho
    glNamedBufferData(vbo_,                                               // target
                      data.vertices.size() * sizeof(Vertex),             // bytes
                      data.vertices.data(),                              // datos RAM
                      GL_STATIC_DRAW);                                   // uso

    // -- 2) Crear y subir el EBO (bytes de los índices) ---
    glCreateBuffers(1, &ebo_);           // segundo buffer: los índices
    glNamedBufferData(ebo_, data.indices.size() * sizeof(unsigned int), data.indices.data(), GL_STATIC_DRAW);

    // -- 3) Crear el VAO (el "manual" que dice cómo leer el VBO) --
    glCreateVertexArrays(1, &vao_);      // pido 1 nombre de VAO a la GPU

    // Ranura de datos 0 del VAO: enchufo el VBO, desde el byte 0, y le digo que cada vértice empieza cada `sizeof(Vertex)` bytes (32) → el stride.
    glVertexArrayVertexBuffer(vao_,      // en qué VAO
                              0,         // ranura (binding) 0               [data]
                              vbo_,      // qué buffer usar                  [VBO]
                              0,         // offset del primer vértice        [0]
                              sizeof(Vertex));   // stride: bytes por vértice [32]

    // Ranura DEDICADA de EBO del VAO: conecto el buffer de índices al VAO.
    // Hacer esto acá es lo que permite que glDrawElements sepa de dónde sale
    // cada índice SIN tener que bindear el EBO en cada frame (está "registrado" dentro del VAO). 
    glVertexArrayElementBuffer(vao_,     // en qué VAO
                               ebo_);    // el buffer de índices (dedicado)

    // Definir los atributos con una lista de descriptores 
    // Cada entrada dice: qué location del shader, cuántos floats, y en qué
    // byte dentro del Vertex empieza. Los offsets se CALCULAN con offsetof()
    // si Vertex cambia, el número se actualiza solo.

    struct AttribSpec {                  // descripción local de un atributo
        unsigned int location;           //   "location" (0, 1 o 2) del shader
        unsigned int componentes;        //   cuántos floats lee
        std::size_t  offset;             //   desplazamiento en bytes dentro de Vertex
    };                                  

    // PRÁCTICO 3: la tupla ahora tiene 3 atributos (posición, normal, textura).
    // El color se fue del vértice (ahora es un uniform del objeto). Cada
    // entrada apunta a un campo del struct Vertex con offsetof().
    const AttribSpec specs[] = {         // se lee una vez por atributo
        {0U, 3U, offsetof(Vertex, position)},    // 0 = posición:  3 floats (x,y,z)
        {1U, 3U, offsetof(Vertex, normal)},      // 1 = normal:    3 floats (nx,ny,nz)
        {2U, 2U, offsetof(Vertex, tex_coords)},  // 2 = textura:   2 floats (u,v)
    };                                   // (el color ya no vive en el vértice)                                   

    for (const AttribSpec& s : specs) {  // configuro cada atributo con el mismo patrón
        // digo el FORMATO del atributo
        glVertexArrayAttribFormat(vao_,               // en qué VAO
                                  s.location,         // atributo N
                                  static_cast<int>(s.componentes),  // 3 floats
                                  GL_FLOAT,           // tipo de cada componente
                                  GL_FALSE,           // no normalizo
                                  static_cast<unsigned int>(s.offset));  // byte inicial

        // Paso B: el atributo N lee desde la RANURA de datos 0 (donde enchufé el VBO).
        glVertexArrayAttribBinding(vao_, s.location, 0);   // atributo -> ranura 0

        // Paso C: ENCENDER el atributo. Si no, la GPU lo ignora y todo sale negro.
        glEnableVertexArrayAttrib(vao_, s.location);       // activo el atributo N
    }                                    

    // Guardar cuántos índices hay que dibujar
    count_ = static_cast<int>(data.indices.size());   // 36 para el cubo
}       

void Mesh::clear(void) {
    if (vao_ != 0U) {                  // ¿tengo un VAO real? (evita el doble borrado)
        glDeleteVertexArrays(1, &vao_); // libero el VAO en la GPU
        vao_ = 0U;                     // el nombre vuelve a cero (ya no poseo nada)
    }                                  // (0 = "no tengo", ver glDelete* no-op)
    if (vbo_ != 0U) {                  // ¿tengo un VBO? 
        glDeleteBuffers(1, &vbo_);     // libero el VBO
        vbo_ = 0U;                     // y lo dejo en cero
    }                                  // fin del VBO
    if (ebo_ != 0U) {                  // ¿tengo un EBO?
        glDeleteBuffers(1, &ebo_);     // libero el EBO
        ebo_ = 0U;                     // y lo dejo en cero
    }                                  // fin del EBO
    count_ = 0;                        // con cero índices no se dibuja nada
}                                      // fin de clear (todo en cero = vacío)


// Movimiento: el nuevo dueño se lleva los nombres y el viejo queda en cero.

Mesh::Mesh(Mesh&& other) noexcept                 // constructor de movimiento
    : vao_(other.vao_),               // copio el nombre del VAO (el nuevo dueño)
      vbo_(other.vbo_),               // copio el nombre del VBO
      ebo_(other.ebo_),               // copio el nombre del EBO
      count_(other.count_) {          // copio la cantidad de índices
    // El objeto de ORIGEN deja de poseer: se pone en cero para que su
    // destructor no libere algo que ahora es del nuevo dueño (decisión [2]).
    other.vao_  = 0U;                 // el origen pierde su VAO
    other.vbo_  = 0U;                 // pierde su VBO
    other.ebo_  = 0U;                 // pierde su EBO
    other.count_ = 0;                 // y no conoce índices
}                                      // fin del constructor de movimiento


// Asignación por movimiento: libero lo que YA poseía, luego me quedo con lo
// del origen y el origen queda en cero.

Mesh& Mesh::operator=(Mesh&& other) noexcept {    // asignación de movimiento
    if (this != &other) {             // no me muevo a mí mismo (undefined si lo hago)
        clear();                      // suelto lo que yo poseía (si tenía algo)
        vao_  = other.vao_;           // me quedo con su VAO
        vbo_  = other.vbo_;           // con su VBO
        ebo_  = other.ebo_;           // con su EBO
        count_ = other.count_;        // y con su cantidad de índices
        other.vao_ = other.vbo_ = other.ebo_ = 0U;   // el origen queda en cero
        other.count_ = 0;             // también sin índices
    }                                 // fin del caso "no soy yo mismo"
    return *this;                     // devuelvo la referencia a mí (encadenar)
}                                      // fin de la asignación por movimiento