#include "ResourceManager.h"        // declaración de la clase que implementamos

#include <fstream>                 
#include <sstream>                
#include <stdexcept>               
#include <string>       
#include <filesystem>  
#include <utility>                 // std::move (al cachear el texto leído)

// STB_IMAGE_IMPLEMENTATION: compila la IMPLEMENTACIÓN de stb_image AQUÍ (un solo
// .cpp de todo el proyecto). Si se definiera en más de un .cpp, el linkeador
// daría error de símbolos duplicados. La decodificación de imágenes es tarea del
// CPU: OpenGL no interviene (solo procesa los bytes ya decodificados).
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
// assets_root : carpeta base de los recursos. Ahí se resuelven las rutas que se piden relativas (ej. "shaders/basic.vs").
ResourceManager::ResourceManager(const std::filesystem::path& assets_root)
    : assets_root_(assets_root)     // inicializa el miembro con la raíz recibida
{                                   // el cache (shaders_sources_) empieza vacío
}

// Carga (o reutiliza) un shader. Es el corazón del módulo.
const ShaderSource& ResourceManager::load_shader_source(const std::string& key, const std::string& vs_file, const std::string& fs_file, const std::string& gs_file) {
    if (key.empty()) {                      
        throw std::invalid_argument(     
            "[ResourceManager] load_shader_source: la clave no puede ser vacía.");
    }                                        

    // -- ¿La clave YA está en el cache?
    const auto it = shaders_sources_.find(key); // it = iterador a la entrada si existe, o end() si la clave NO está cargada
    if (it != shaders_sources_.end()) {         
        const LoadedShader& cached = it->second; // referencia a la entrada guardada
        // Corroboro que los archivos pedidos sean los mismos que ya guardé.
        // Si coinciden => cache hit: devuelvo lo existente, NO vuelvo a leer disco.
        const bool mismo_archivo = cached.vs_file == vs_file && cached.fs_file == fs_file && cached.gs_file == gs_file;      
        if (mismo_archivo) {                  
            return cached.source;               // devuelvo la copia en memoria
        }    
        // Si los archivos difieren ES UN ERROR.
        throw std::runtime_error("[ResourceManager] La clave '" + key + "' ya está cargada con OTROS archivos " +  "(guardado: vs='"+  cached.vs_file + "' fs='" +    // lo que había...
            cached.fs_file + "'; pedido: vs='" + vs_file + "' fs='" + fs_file + "'). " +"Usá otra clave o llamá a clear().");
    }     

    // -- Caso PRIMERA carga: leer todo y guardarlo en el cache -
    ShaderSource src;                           // struct temporal con el texto nuevo
    src.vs = read_shader_file(vs_file, "VS");   // lee el vertex desde el disco
    src.fs = read_shader_file(fs_file, "FS");   // lee el fragment desde el disco
    if (!gs_file.empty()) {                     // el geometry es OPCIONAL
        src.gs = read_shader_file(gs_file, "GS");
    }                                          

    LoadedShader nueva_entrada = {              
        src,                                    //   .source = los textos leídos
        vs_file,                                //   .vs_file = ruta original
        fs_file,                                //   .fs_file = ruta original
        gs_file                                 //   .gs_file = ruta original ("" ok)
    };                                          // cierra el inicializador

    // Inserto la entrada en el mapa. `insert` NO pisa claves existentes y me devuelve un par {iterador, bool}: bool=true si la inserción fue exitosa.
    const auto [out, insertado] = shaders_sources_.insert({key, nueva_entrada});  // clave -> entrada

    if (!insertado) {                          
        throw std::logic_error("[ResourceManager] No se pudo insertar '" + key + "' en el cache.");
    }                                          

    return out->second.source; // devuelvo la referencia ya cacheadada (apunta DENTRO del mapa)
}  


// get_shader_source — solo consulta: nunca toca el disco.
const ShaderSource& ResourceManager::get_shader_source(const std::string& key) const {
    const auto it = shaders_sources_.find(key); // busco la clave en el cache
    if (it == shaders_sources_.end()) {         // no está cargada
        throw std::runtime_error("[ResourceManager] get_shader_source: la clave '" + key + "' no está cargada. " +"¿Llamaste a load_shader_source() antes?");
    }                                        
    return it->second.source;                   // ref. const a la entrada del mapa
}                                              


// clear — vacía el cache. Cuidado: invalida todas las referencias pendientes.
void ResourceManager::clear() {
    shaders_sources_.clear();                   // elimina todas las entradas del mapa
    images_.clear();                            // lo mismo para las imágenes
    texts_.clear();                             // y para el texto
}                                               

// shader_count — cuántos shaders distintos hay cacheados.
std::size_t ResourceManager::shader_count() const {
    return shaders_sources_.size();             // cantidad de claves en el mapa
}                                              

// read_shader_file — lee UN archivo del disco y devuelve su contenido en texto.
std::string ResourceManager::read_shader_file(const std::string& shader_file, const std::string& shader_type) const {
    // -- 1) Validación: el nombre no puede ser vacío ---
    if (shader_file.empty()) {                  // VS/FS siempre deben traer archivo
        throw std::invalid_argument("[ResourceManager] " + shader_type + ": no se indicó el nombre del archivo.");
    } 

    // -- 2) Resolver la ruta completa --
    std::filesystem::path ruta(shader_file);    // convierto el string a un "path"
    if (!ruta.is_absolute()) {                  // ¿es una ruta relativa?
        ruta = assets_root_ / ruta;             // entonces la cuelgo de la raíz de assets
    }                                           // / = concatenar carpetas (no plataforma-dependente)

    // -- 3) Validación del archivo en disco 
    if (!std::filesystem::exists(ruta)) {       // ¿existe la ruta? (archivo o carpeta)
        throw std::runtime_error("[ResourceManager] " + shader_type + ": no existe el archivo '" + shader_file +"' (busqué en: " + ruta.string() + ").");
    }                                          
    if (!std::filesystem::is_regular_file(ruta)) {  // ¿es un archivo normal? 
        throw std::runtime_error("[ResourceManager] " + shader_type + ": la ruta '" + ruta.string() + "' no es un archivo regular (¿es una carpeta?).");
    }  
    // -- 4) Abrir y leer el contenido --
    // ios::binary: modo binario. No hace traducciones de fin de línea, así el texto que leemos es EXACTAMENTE el que está en el archivo.
    std::ifstream in(ruta, std::ios::binary);   // abre el archivo para lectura
    if (!in.is_open()) {                        // ¿falló la apertura? (ej. permiso)
        throw std::runtime_error("[ResourceManager] " + shader_type + ": no pude abrir '" + ruta.string() + "'.");
    } 

    std::ostringstream buffer;                  // acumulador de texto (eficiente)
    buffer << in.rdbuf();                       // vuelco TODO el archivo al buffer
    if (in.bad()) {                             // ¿error de hardware/media durante la lectura?
        throw std::runtime_error("[ResourceManager] " + shader_type + ": error de lectura al leer '" + ruta.string() + "'.");
    } 

    return buffer.str();                        // el texto del shader, listo para usar
}    

// -----------------------------------------------------------------------------
// load_image — decodifica una imagen (stb_image) y la cachea como RGBA.
// -----------------------------------------------------------------------------
const ImageData& ResourceManager::load_image(const std::string& key, const std::string& file, bool flip_vertically) {
    if (key.empty()) {
        throw std::invalid_argument("[ResourceManager] load_image: la clave no puede ser vacía.");
    }

    // -- ¿La clave ya está en el cache?
    const auto it = images_.find(key);
    if (it != images_.end()) {
        // Cache hit solo si el archivo y la inversión coinciden. Si la MISMA
        // imagen se pide una vez invertida y otra no (o desde otro archivo) es
        // un error, igual que con los shaders: la clave identifica UNA imagen.
        if (it->second.file == file && it->second.flip_vertically == flip_vertically) {
            return it->second.data;
        }
        throw std::runtime_error("[ResourceManager] La clave '" + key +
            "' ya está cargada con otra imagen (o con otro flip). Usá otra clave o clear().");
    }

    // -- Resolver la ruta completa (misma lógica que read_shader_file) --
    std::filesystem::path ruta(file);
    if (!ruta.is_absolute()) {
        ruta = assets_root_ / ruta;
    }

    // -- Decodificar. El último argumento (4) fuerza RGBA sin importar los
    //    canales del archivo; `channels` informa los canales reales del archivo.
    //    El flip es un ESTADO GLOBAL de stb_image: se fija justo antes de cargar
    //    y se restaura a 0 al terminar, para no contaminar la siguiente carga.
    stbi_set_flip_vertically_on_load(flip_vertically ? 1 : 0);
    int w = 0, h = 0, canales = 0;
    unsigned char* datos = stbi_load(ruta.string().c_str(), &w, &h, &canales, 4);
    stbi_set_flip_vertically_on_load(0);

    if (datos == nullptr) {
        throw std::runtime_error("[ResourceManager] load_image: no se pudo decodificar '" +
            file + "': " + stbi_failure_reason());
    }

    // -- Copiar los bytes a un vector propio (RAII) y liberar el buffer de stb --
    ImageData img;
    img.width    = w;
    img.height   = h;
    img.channels = canales;
    img.pixels.assign(datos, datos + static_cast<std::size_t>(w) * h * 4);
    stbi_image_free(datos);

    // -- Guardar en el cache --
    const auto [out, insertado] = images_.insert({key, {img, file, flip_vertically}});
    if (!insertado) {
        throw std::logic_error("[ResourceManager] No se pudo insertar '" + key + "' en el cache de imágenes.");
    }
    return out->second.data;
}

// get_image — solo consulta, nunca toca el disco.
const ImageData& ResourceManager::get_image(const std::string& key) const {
    const auto it = images_.find(key);
    if (it == images_.end()) {
        throw std::runtime_error("[ResourceManager] get_image: la clave '" + key + "' no está cargada.");
    }
    return it->second.data;
}

// unload_image — libera la copia en RAM (la GPU ya tiene su copia). Volver a
// pedirla obliga a releer el disco.
void ResourceManager::unload_image(const std::string& key) {
    images_.erase(key);
}

// -----------------------------------------------------------------------------
// load_text / get_text — archivo de TEXTO genérico (ej. iluminacion.glsl).
// -----------------------------------------------------------------------------
const std::string& ResourceManager::load_text(const std::string& key, const std::string& file) {
    if (key.empty()) {
        throw std::invalid_argument("[ResourceManager] load_text: la clave no puede ser vacía.");
    }
    const auto it = texts_.find(key);
    if (it != texts_.end()) {
        return it->second;
    }
    std::string texto = read_shader_file(file, "TEXT");
    const auto [out, insertado] = texts_.insert({key, std::move(texto)});
    if (!insertado) {
        throw std::logic_error("[ResourceManager] No se pudo insertar '" + key + "' en el cache de texto.");
    }
    return out->second;
}

const std::string& ResourceManager::get_text(const std::string& key) const {
    const auto it = texts_.find(key);
    if (it == texts_.end()) {
        throw std::runtime_error("[ResourceManager] get_text: la clave '" + key + "' no está cargada.");
    }
    return it->second;
}                                        