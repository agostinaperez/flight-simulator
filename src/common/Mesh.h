
//mesh vive en la GPU y, a partir de un MeshData, la función load() crea el VBO, EBO y VAO y los sube a la placa
//el meshdata llena los vectores, el mesh los sube, los dibuja, y se encarga de q se liberen bien

// DECISIONES DE DISEÑO
//     → count_ guarda INDICES. La función que dibuja este práctico es glDrawElements, y su
//       parámetro central es la CANTIDAD DE INDICES a procesar (36). Si
//       mañana se quisiera una malla NO indexada (sin EBO), se dibujaría con
//       glDrawArrays y lo que se necesita es la cantidad de VÉRTICES; 

//      ¿Por qué el objeto movido debe quedar en cero?
//     → Porque el destructor (y clear()) liberan lo que posee el objeto.
//       Si el "nuevo dueño" se lleva los números (vao_, vbo_, ebo_) y el
//       "viejo" los conserva, si se borra se haría un borrado doble
//
//      Los desplazamientos de atributos CALCULADOS con offsetof(Vertex, campo). Si se escribieran a mano y
//       mañana la tupla cambiara (ej. se agregara un atributo nuevo en el medio), los offsets se romperían en
//       silencio y cada atributo se leería desde el lugar equivocado. 
//
//       Si el vértice tuviera un atributo más, ¿cuántos lugares se tocan?
//     → Con este diseño: 3 lugares. (a) struct Vertex: agregar el campo.
//       (b) el shader: agregar el layout(location=N). (c) el array de
//       descriptores dentro de Mesh::load(): agregar UNA entrada. El loop que
//       configura los atributos no cambia (los guía la lista de descriptores).
//       (En el Práctico 3 esto ya pasó de verdad: los atributos pasaron de ser
//        [posición, color] a [posición, normal, textura]).
//
//     → Copiar se BORRA, Mover SÍ se permite
//
//      si llamo a clear() 2 veces no pasa na, pq clear borra y pone en 0, si llamo de nuevo ve 0 y no borra nada
#pragma once   
#include "MeshData.h" 


// el constructor no hace nada; load() crea los objetos en la GPU;
// el destructor (o clear()) los libera; mover transfiere la propiedad.
class Mesh {
public:
    // Constructor por defecto: crea un objeto VACÍO (sin nombres de GPU).
    Mesh() = default;

    // Destructor: llama a clear() y libera lo que este objeto posee.
    // Todo glDelete* vive acá; nada afuera de esta clase debe borrar GPU.
    ~Mesh();

    // Copiar NO existe: si dos Mesh tuvieran los mismos enteros, el primero
    // que muera rompería al otro (borrado doble). "= delete" lo prohíbe.
    Mesh(const Mesh&) = delete;                                  // constructor copia, borrado
    Mesh& operator=(const Mesh&) = delete;                       // asignación copia, borrada

    // Mover SÍ existe: transfiere la propiedad del objeto de origen al nuevo.
    // "noexcept": mover promete no lanzar, así los contenedores la usan.
    Mesh(Mesh&& other) noexcept;                                 // constructor por movimiento
    Mesh& operator=(Mesh&& other) noexcept;                      // asignación por movimiento

    // load: sube la malla a la GPU y arma el VAO.
    //   * Crea VAO, VBO y EBO.
    //   * Sube la lista de vértices (un buffer entrelazado) y la de índices.
    //   * Conecta el EBO a la ranura dedicada del VAO.
    //   * Configura los tres atributos: 0=posición, 1=normal, 2=textura
    //     (offsets calculados).
    //   * Guarda en count_ la cantidad de índices.
    // Si la malla ya tenía datos cargados, los libera primero (clear() interno).
    void load(const MeshData& data);

    void clear(void);

    // vao: devuelve el nombre (entero) del VAO para usarlo al dibujar
    unsigned int vao(void) const { return vao_; }                // el VAO de esta malla

    // count: cuántos INDICES hay que dibujar (glDrawElements)
    int count(void) const { return count_; }                     // cantidad de índices

private:
    // Los tres nombres (nombres = enteros) de los objetos en la GPU.
    // "= 0U" en la declaración = si no se carga nada, quedan en cero. Cero en
    // OpenGL significa "no hay objeto", y glDelete* con cero no hace nada.
    unsigned int vao_ {0U};        // Vertex Array Object: el "manual" de lectura
    unsigned int vbo_ {0U};        // Vertex Buffer Object: bytes de los vértices
    unsigned int ebo_ {0U};        // Element Buffer Object: bytes de los índices
    int count_ {0};                // cantidad de índices a dibujar (decisión [1])
};                                 // fin de la clase Mesh