// maneja el programa de shaders. Los shaders (vertex y fragment) le dicen a la GPU cómo pintar y dond

// DECISIONES DE DISEÑO 
// [1] El largo del log SE CONSULTA. El driver informa la longitud exacta del log con
//       glGetShaderiv(..., GL_INFO_LOG_LENGTH, ...) y el string se reserva de ese tamaño. 
//
// [2] ¿Cómo se avisa el error? ¿Y es coherente con ResourceManager?
//     → Esta clase devuelve bool (falso si algo falló), a diferencia de
//       ResourceManager, que usa excepciones. Un fallo de compilación/link es un diagnóstico que
//       el driver te da y el llamador elige cómo manejar (acá el main sale);
//       una "clave o archivo inexistente" en ResourceManager es un uso
//       incorrecto del módulo, y la excepción evita seguir con datos rotos.
//
// [3] No abre archivos.
//     → Quien lee el disco es ResourceManager. Esta clase recibe el texto ya leído (std::string) y solo compila/linkea. 
//       Así el módulo no depende de rutas ni del filesystem
//
// [4] Mismo patrón de propiedad que Mesh: copiar prohibido, mover permitido, clear() idempotente, y los glDelete* viven solo acá dentro.
//
// [5] set_uniform: hay DOS versiones de cada setter (las pide el PDF).
//     * Por NOMBRE   : busca el uniform dentro del programa (glGetUniformLocation)
//                      cada vez que se llama. CÓMODO para leer el código, pero
//                      en el loop de dibujo es una búsqueda de string por
//                      cuadro para devolver SIEMPRE el mismo número.
//     * Por UBICACIÓN: `loc(nombre)` consulta el identificador UNA vez (al
//                      armar la escena) y después el loop usa ese int directo.
//                      Evita la búsqueda por texto en cada frame.
//     Por eso cada setter viene en dos sabores. El main decide cuál usar.
//     Si la ubicación no existe (GL devuelve -1), glProgramUniform* la ignora
//     en silencio (se especifica así); no hay error que manejar.

#pragma once                          // se incluye una sola vez por archivo

#include <glm/glm.hpp>                // glm::mat4 / glm::vec3: valores a pasar
#include <string>                     // std::string: recibe el fuente de los shaders
#include <vector>                     // std::vector (varias fuentes por etapa, P9)

class Shader {
public:
    // Constructor por defecto: objeto VACÍO (no tiene programa todavía).
    // "= default": el constructor que genera el compilador solo.
    Shader() = default;

    // Destructor: libera el programa si este objeto lo posee (clear()).
    ~Shader();

    // const Shader& referencia a un shader existente, const Shader&& referencia a un shader que se va a mover
    
    // Copiar prohibido: DOS dueños del mismo programa = doble borrado.
    Shader(const Shader&) = delete;                                  // copia, borrada
    Shader& operator=(const Shader&) = delete;                       // copia, borrada

    // Mover permitido: transfiere el nombre del programa y deja al origen en cero.
    Shader(Shader&& other) noexcept;                                 // move ctor
    Shader& operator=(Shader&& other) noexcept;                      // move asign

    // compila vertex + fragment, linkea el programa y libera los objetos intermedios. NO abre archivos
    //   vs, fs : texto de los shaders (viene de ResourceManager).
    // Devuelve true si quedó listo. Si algo falla: imprime el log del driver, el objeto queda con id 0 (vacío)
    // y devuelve false
    bool compile_from_source(const std::string& vs, const std::string& fs);

    // compile_from_sources: igual que compile_from_source, pero el FRAGMENT shader
    // se arma concatenando VARIAS fuentes (Práctico 9). GLSL no tiene #include:
    // glShaderSource recibe un arreglo de cadenas y las compila como un único
    // texto, una a continuación de la otra. Así el fragment shader queda armado
    // como: "#version" + código común (iluminacion.glsl) + código propio.
    //   vs        : texto del vertex shader (una sola fuente, con su #version).
    //   fs_parts  : las fuentes del fragment shader, EN ORDEN. La primera DEBE
    //               ser la línea "#version ..." (tiene que ser la primera del
    //               texto; por eso no vive dentro de ninguno de los archivos).
    bool compile_from_sources(const std::string& vs, const std::vector<std::string>& fs_parts);

    // use: activa el programa para dibujar (glUseProgram) antes de dibujar.
    void use(void) const;

    // loc: consulta UNA VEZ la ubicación (número) de un uniform por su nombre.
    // Guardar el int a mano y pasarlo a los set_uniform( int, ... ) evita
    // buscar por texto dentro del loop de dibujo. Devuelve -1 si el uniform no
    // existe en el programa (glProgramUniform* con -1 no hace nada).
    int loc(const std::string& nombre) const;

    // set_uniform (versión mat4): fija una matriz 4x4 en el programa.
    // Se usa para las matrices de modelo (uModel) y de vista+proyección
    // (uAjuste). GLSL la espera por COLUMNAS (como glm la guarda).
    void set_uniform(const std::string& nombre, const glm::mat4& m) const;
    void set_uniform(int ubicacion, const glm::mat4& m) const;

    // set_uniform (versión vec3): fija un vector de 3 floats.
    // Se usa para los colores (uColor) y, más adelante, direcciones/luz.
    void set_uniform(const std::string& nombre, const glm::vec3& v) const;
    void set_uniform(int ubicacion, const glm::vec3& v) const;

    // set_uniform (versión mat3): fija una matriz 3x3. Se usa para la MATRIZ
    // NORMAL (Práctico 9): transforma las normales al espacio de mundo cuando el
    // modelo tiene escalado no uniforme.
    void set_uniform(int ubicacion, const glm::mat3& m) const;

    // set_uniform (versión float): fija un único valor (ej. tiempo, intensidad).
    void set_uniform(const std::string& nombre, float valor) const;
    void set_uniform(int ubicacion, float valor) const;

    // set_uniform (versión bool, int): para flags como "corrección gamma on/off".
    void set_uniform(int ubicacion, bool valor) const;

    // clear: libera el programa y deja el objeto en cero. Idempotente.
    void clear(void);

    // id: el nombre (entero) del programa, para usarlo si hiciera falta.
    // Se presta para usar, no para borrar (el borrado es de esta clase).
    unsigned int id(void) const { return id_; } 

private:
    unsigned int id_ {0U};             // nombre del programa en la GPU (0 = vacío)
};          