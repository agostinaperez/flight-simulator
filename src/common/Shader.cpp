#include "Shader.h" 

#include <glad/gl.h> 
#include <iostream>

// Funciones auxiliares INTERNAS (sin "static" visible fuera de este archivo).
namespace {                              // "anonymous namespace": solo visible acá

// log_shader: el log de compilación de UN shader (vertex o fragment).
std::string log_shader(GLuint shader) { 
    GLint largo = 0;                     // variable que recibe el largo
    // Pido la longitud del log. GL_INFO_LOG_LENGTH INCLUYE el carácter '\0'
    // final de la string C, por eso después resto 1 para el std::string.
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &largo);
    if (largo <= 1) {                    // 0 o 1 => no hay texto de log
        return std::string();            // devuelvo string vacío (sin log)
    }  
    // Reservo un string del tamaño justo (largo - 1, menos el '\0' final).
    std::string texto(static_cast<std::size_t>(largo) - 1, '\0');
    // GL llena el buffer con el log. text.data() = puntero al interior del string.
    glGetShaderInfoLog(shader, largo, nullptr, texto.data());
    return texto;                      
}                                       

// log_program: igual pero para el LOG DEL LINKEADO de un programa completo.
std::string log_program(GLuint program) { 
    GLint largo = 0; 
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &largo);
    if (largo <= 1) {                   
        return std::string(); 
    }                                
    std::string texto(static_cast<std::size_t>(largo) - 1, '\0'); 
    glGetProgramInfoLog(program, largo, nullptr, texto.data());  
    return texto;                      
}                                    

// compilar_etapa — compila UNA etapa (vertex o fragment) a partir de VARIAS
// fuentes. glShaderSource recibe un arreglo de cadenas y las concatena como un
// único texto (Práctico 9: no existe #include en GLSL). Devuelve true y deja el
// shader en `shader` si compiló; si falla imprime el log, borra el objeto y
// devuelve false.
bool compilar_etapa(GLenum tipo, const std::vector<std::string>& fuentes,
                    GLuint& shader, const char* nombre) {
    shader = glCreateShader(tipo);              // pido 1 objeto shader a la GPU

    // Convierto los std::string a un arreglo de const char* (lo que espera GL).
    std::vector<const char*> punteros;
    punteros.reserve(fuentes.size());
    for (const std::string& f : fuentes) {
        punteros.push_back(f.c_str());
    }
    glShaderSource(shader, static_cast<GLsizei>(punteros.size()),
                   punteros.data(), nullptr);   // entrego las fuentes (concatenadas)
    glCompileShader(shader);                    // la GPU compila el texto

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {                       // no compiló
        std::cout << "[Shader] ERROR compilando " << nombre << " shader:\n"
                  << log_shader(shader) << "\n";        // imprimo el log del driver
        glDeleteShader(shader);                 // libero el objeto roto
        shader = 0;
        return false;
    }
    return true;
}

} // fin de namespace anónimo (nada de esto es visible fuera del archivo)

// Destructor: libera el programa si este objeto lo posee.
Shader::~Shader() {
    clear();                           
}                       

// compile_from_source: todo el pipeline de compilación y linkeo.
// Devuelve true/false; si falla imprime el log y deja vacío.

//LOS STRINGS Q RECIBO SON EL CÓDIGO FUENTE DE LOS SHADERS, CORTE EL R(Version blablabla; vec4 blabkabka)
//q los tengo en assets/shaders
bool Shader::compile_from_source(const std::string& vs, const std::string& fs) {
    // Caso simple (una fuente por etapa): delega en compile_from_sources con una
    // sola parte para el fragment shader.
    return compile_from_sources(vs, {fs});
}

// compile_from_sources: el fragment shader se arma concatenando varias fuentes.
bool Shader::compile_from_sources(const std::string& vs, const std::vector<std::string>& fs_parts) {
    clear();

    // -- 1) Compilar el VERTEX shader (una sola fuente) ------------------------
    GLuint v = 0;
    if (!compilar_etapa(GL_VERTEX_SHADER, {vs}, v, "VERTEX")) {
        return false;                           // el log ya se imprimió
    }

    // -- 2) Compilar el FRAGMENT shader (varias fuentes concatenadas) ----------
    GLuint f = 0;
    if (!compilar_etapa(GL_FRAGMENT_SHADER, fs_parts, f, "FRAGMENT")) {
        glDeleteShader(v);                      // libero el vertex ya compilado
        return false;
    }

    // -- 3) Crear el PROGRAMA y adjuntar ambos shaders -------------------------
    GLuint prog = glCreateProgram();            // pido un programa nuevo
    glAttachShader(prog, v);                    // agarro el vertex compilado
    glAttachShader(prog, f);                    // agarro el fragment compilado
    glLinkProgram(prog);                        // los uno en un binario final

    GLint ok_l = 0;                             // resultado del linkeo
    glGetProgramiv(prog, GL_LINK_STATUS, &ok_l);
    if (ok_l == GL_FALSE) {                     // no linkeó
        std::cout << "[Shader] ERROR linkeando programa:\n"
                  << log_program(prog) << "\n";
        glDeleteProgram(prog);
        glDeleteShader(v);
        glDeleteShader(f);
        return false;
    }

    // Los shaders sueltos ya están "fusionados" en el programa; se borran y el
    // programa sigue válido (la GPU conserva el binario linkeado internamente).
    glDeleteShader(v);
    glDeleteShader(f);
    id_ = prog;                                 // ahora poseo el programa
    return true;
}
// activa el programa para los dibujos siguientes.
void Shader::use(void) const {
    glUseProgram(id_);                              // el driver marca "prog actual"
}                                                   // (id_ == 0 sería no usar programa)

// loc: pide al driver el número de ubicación del uniform por su nombre.
// Se llama UNA vez (cuando se arma la escena) y el loop reutiliza ese int.
int Shader::loc(const std::string& nombre) const {
    // glGetUniformLocation busca el uniform dentro del PROGRAMA linkeado y
    // devuelve su índice (la "dirección" donde está). -1 = no existe/no se usa.
    return glGetUniformLocation(id_, nombre.c_str());
}

// ----------------------------------------------------------------------------
// setters de uniforms (las dos versiones de cada uno, ver Shader.h, decisión [5]).
// La versión por nombre es un "atajo" que delega en la versión por ubicación.
// La técnica es DSA: glProgramUniform* escribe en el programa SIN bindearlo
// (no hace falta glUseProgram antes). La versión por ubicación hace la misma
// llamada pero sin volver a pedir el índice por texto.
// ----------------------------------------------------------------------------

void Shader::set_uniform(const std::string& nombre, const glm::mat4& m) const {
    set_uniform(loc(nombre), m);                    // busco la ubicación y delego
}

void Shader::set_uniform(int ubicacion, const glm::mat4& m) const {
    // matrix4fv: fija una mat4. GL_FALSE = NO trasponga (glm ya está en el
    // orden de columnas que espera GLSL). &m[0][0] = inicio de los 16 floats.
    glProgramUniformMatrix4fv(id_, ubicacion, 1, GL_FALSE, &m[0][0]);
}

void Shader::set_uniform(const std::string& nombre, const glm::vec3& v) const {
    set_uniform(loc(nombre), v);
}

void Shader::set_uniform(int ubicacion, const glm::vec3& v) const {
    // 3fv: fija un vec3 (tres floats). &v.x = primer float del vector.
    glProgramUniform3fv(id_, ubicacion, 1, &v.x);
}

void Shader::set_uniform(int ubicacion, const glm::mat3& m) const {
    // Matrix3fv: fija una mat3 (la matriz normal). GL_FALSE = no trasponga (glm
    // ya está en el orden de columnas que espera GLSL).
    glProgramUniformMatrix3fv(id_, ubicacion, 1, GL_FALSE, &m[0][0]);
}

void Shader::set_uniform(const std::string& nombre, float valor) const {
    set_uniform(loc(nombre), valor);
}

void Shader::set_uniform(int ubicacion, float valor) const {
    // 1f: fija un solo float. Las otras son ...2fv/...4fv para vec2/vec4.
    glProgramUniform1f(id_, ubicacion, valor);
}

void Shader::set_uniform(int ubicacion, bool valor) const {
    // 1i: un bool se pasa como int (0/1) al uniform bool del shader.
    glProgramUniform1i(id_, ubicacion, valor ? 1 : 0);
}

// clear: libera el programa y deja el objeto en cero. Idempotente.
void Shader::clear(void) {
    if (id_ != 0U) {                                // ¿poseo algo? (evita doble borrado)
        glDeleteProgram(id_);                       // libero el programa en la GPU
        id_ = 0U;                                   // el nombre vuelve a cero
    }                                               // (0 = "no poseo", glDelete no-op)
}                                                   // fin clear

// Movimiento: el nuevo dueño se lleva el programa y el origen queda en cero.
Shader::Shader(Shader&& other) noexcept             // constructor de movimiento
    : id_(other.id_) {                              // me quedo con el nombre del programa
    other.id_ = 0U;                                 // el origen deja de poseerlo
}                                                   // (su destructor no borra lo mío)

// Asignación por movimiento: libero lo que tenía, me quedo con lo del origen.
Shader& Shader::operator=(Shader&& other) noexcept {   // asignación de movimiento
    if (this != &other) {                           // evito moverme sobre mí mismo
        clear();                                    // suelto mi programa previo
        id_ = other.id_;                            // me quedo con el del origen
        other.id_ = 0U;                             // y el origen deja de poseerlo
    }                                               // fin del caso "no soy yo"
    return *this;                                   // devuelvo la referencia a mí
}                                                   // fin operator=