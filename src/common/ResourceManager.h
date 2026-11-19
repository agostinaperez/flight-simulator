// Módulo "administrador de recursos" del proyecto de computación gráfica.
//
// Este módulo lee los shaders del disco como archivos externos (.vs / .fs / .gs), mantiene una copia en memoria (cache) y la deja
// disponible para el resto del proyecto.
//
// A futuro este módulo administrará OTROS recursos (texturas, mallas, modelos).
// Por eso se usa el patrón "un struct por tipo de recurso" + un load_*/get_*
// por tipo + un mapa. Creando un nuevo tipo de recurso solo hay que repetir este mismo molde.

// DECISIONES DE DISEÑO
//
// 
//     → Por el caché se indexa x CLAVE (string) que elige quien usa el módulo.
//       * Las prácticas referencian el shader por un nombre semántico
//         ("triangulo") sin repetir rutas de archivos.
//       * Mover o renombrar el archivo en disco NO obliga a tocar el código.
//       * El mismo archivo puede cargarse dos veces bajo claves distintas.
//
//     → Si se pide una clave ya cargada pero con OTROS archivos Es un ERROR (excepción std::runtime_error).
//     
//     → si algo falla se usan EXCEPCIONES
//
//     → devuelve una REFERENCIA const: no se copian los textos de los shaders (eficiente).
//       ADVERTENCIA: la referencia apunta a una entrada interna del mapa.
//       clear() (o un load que recargue la misma clave) la INVALIDA. Regla de
//       uso: obtener la referencia y usarla de inmediato.
//
//     → El struct shadersource agrupa vs/fs/gs como una unidad lógica (un
//       programa de shaders) y fija el patrón "un struct por tipo de recurso"
//       que se repetirá cuando se agreguen texturas/mallas/modelos.
//
#pragma once  
#include <string>                 
#include <filesystem>     
#include <unordered_map>        
#include <cstddef>
#include <vector>               // std::vector (los píxeles de una imagen)

// ShaderSource — un tipo de recurso: el código fuente de un programa de shader.
struct ShaderSource {            
    std::string vs;                // código fuente del vertex shader
    std::string fs;                // código fuente del fragment shader
    std::string gs;               //codigo fuente del geometry shader (todavia no se usa)
};                              

// ImageData — un tipo de recurso: una imagen YA DECODIFICADA del lado del CPU.
// (Práctico 8.) La decodificación es tarea del CPU (stb_image); la textura es
// un recurso de GPU que se administra en otro módulo (Texture). Misma
// separación que hay entre MeshData (datos) y Mesh (recurso de GPU).
struct ImageData {
    int width    = 0;             // ancho en píxeles
    int height   = 0;             // alto en píxeles
    int channels = 0;             // canales DEL ARCHIVO (informativo)
    // Los píxeles se guardan SIEMPRE como RGBA (4 bytes por texel), sin importar
    // los canales del archivo. Decisión de diseño: el código que sube la imagen
    // a la GPU no depende de la cantidad de canales del archivo.
    std::vector<unsigned char> pixels;
};

// ResourceManager — administra los recursos del proyecto en memoria (cache).
// Instanciar uno con la carpeta raíz de los assets; todas las rutas de los archivos de shader se resuelven relativas a esa raíz.
class ResourceManager {
public:
    // Constructor: guarda la raíz de assets y deja el cache vacío.
    // Se marca "explicit" para que no se pueda hacer una conversión implícita
    explicit ResourceManager(const std::filesystem::path& assets_root);

    
    // load_shader_source: lee del disco (solo la primera vez) y guarda en cache.
    //   key     : clave (nombre lógico) con la que el resto del código pide este shader. Ej. "triangulo".
    //   vs_file : archivo del vertex shader, relativo a assets_root_.
    //   fs_file : archivo del fragment shader, relativo a assets_root_.
    //   gs_file : archivo del geometry shader (opcional; "" = no se usa).
    // Devuelve una REFERENCIA const a la entrada del cache
    const ShaderSource& load_shader_source(const std::string& key, const std::string& vs_file, const std::string& fs_file, const std::string& gs_file = "");

    
    //devuelve el shader ya cargado bajo `key`, sin tocar el disco. Lanza excepción si `key` nunca fue cargada
    const ShaderSource& get_shader_source(const std::string& key) const;

    // load_image: decodifica una imagen del disco (stb_image) y la guarda en el
    // cache como RGBA. (Práctico 8.)
    //   key             : clave con la que se pide la imagen después.
    //   file            : archivo, relativo a assets_root_.
    //   flip_vertically : si true, se invierte verticalmente al cargar (lo que
    //                     necesita una textura 2D por la convención de OpenGL:
    //                     origen abajo-izquierda). Las caras de un cubemap NO se
    //                     invierten (convención RenderMan, origen arriba-izquierda).
    // Lanza excepción si el archivo no existe o no se puede decodificar.
    const ImageData& load_image(const std::string& key, const std::string& file, bool flip_vertically);

    // get_image: devuelve la imagen ya decodificada, sin tocar el disco.
    const ImageData& get_image(const std::string& key) const;

    // unload_image: libera la copia en RAM de una imagen ya cargada (la GPU ya
    // tiene su copia). Si se la vuelve a pedir, se relee del disco. Sirve para
    // no tener imágenes de 4 MiB en RAM durante todo el programa.
    void unload_image(const std::string& key);

    // load_text: lee un archivo de TEXTO cualquiera (ej. un .glsl de código
    // común) y lo cachea. (Práctico 9: el código de iluminación compartido.)
    const std::string& load_text(const std::string& key, const std::string& file);

    // get_text: devuelve el texto ya cargado, sin tocar el disco.
    const std::string& get_text(const std::string& key) const;

    // clear: vacía TODO el cache. Cualquier referencia devuelta antes por load_/get_ queda inválida
    void clear();

    
    // shader_count (extra fuera del molde): cuántos shaders hay cacheados.
    std::size_t shader_count() const;

private:
    // LoadedShader — ENTRADA interna del cache. Además del texto leído guarda
    // las rutas originales. Así, cuando se repite una clave, el módulo puede
    // comparar los archivos pedidos contra los ya guardados y detectar el conflicto de la decisión 
    
    struct LoadedShader {          // una entrada del mapa del cache
        ShaderSource source;       // el texto ya leído (vs/fs/gs)
        std::string vs_file;       // archivo original del vertex (para comparar)
        std::string fs_file;       // archivo original del fragment (para comparar)
        std::string gs_file;       // archivo original del geometry ("" si no se usa)
    };                             // fin de la entrada interna

    
    // Rutina privada: lee UN archivo de shader del disco y devuelve su texto.
    //   shader_file : ruta del archivo (absoluta o relativa a assets_root_).
    //   shader_type : etiqueta de diagnóstico ("VS", "FS", "GS") para que el
    //                 mensaje de error diga qué etapa falló exactamente.
    // Lanza excepción si el nombre es vacío, el archivo no existe, no es un
    // archivo regular o no se puede abrir.
    std::string read_shader_file(const std::string& shader_file,
                                 const std::string& shader_type) const;

    // Entry imagen del cache: además de los datos decodificados guarda el
    // archivo y el flag de inversión para detectar conflictos de clave.
    struct ImageEntry {
        ImageData   data;
        std::string file;
        bool        flip_vertically = false;
    };

    std::filesystem::path assets_root_;          // raíz donde viven los assets
    std::unordered_map<std::string, LoadedShader> shaders_sources_;  // el cache de shaders
    std::unordered_map<std::string, ImageEntry>  images_;            // el cache de imágenes
    std::unordered_map<std::string, std::string> texts_;             // el cache de texto
};