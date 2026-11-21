#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <dlfdm/fdmsolver.h>
#include <dlfdm/models/aircraft/jettrainer.h>

#include "common/Aircraft.h"
#include "common/CameraCommand.h"
#include "common/CameraSystem.h"
#include "common/FlightData.h"
#include "common/GroundContact.h"
#include "common/InputHandler.h"
#include "common/Material.h"
#include "common/Primitives.h"
#include "common/RenderItem.h"
#include "common/ResourceManager.h"
#include "common/Shader.h"
#include "common/Terrain.h"
#include "common/Texture.h"

namespace {

// Ancho inicial de la ventana, en pixeles. No limita los redimensionamientos.
constexpr int kAncho = 1280;
// Alto inicial de la ventana, en pixeles.
constexpr int kAlto = 720;
// Duracion de cada paso del modelo de vuelo: 1/120 s equivale a 120 Hz.
constexpr double kPasoFijo = 1.0 / 120.0;
// Maximo tiempo real que se acepta de un solo cuadro para recuperar atrasos.
// Evita ejecutar cientos de pasos si el programa se frena o pierde foco.
constexpr double kMaxTiempoPorCuadro = 0.25;
// Tiempo entre actualizaciones del titulo con altitud, velocidad y otros datos.
constexpr double kIntervaloTelemetria = 0.25;
// Factor que convierte el tamano normalizado del modelo visual a metros.
// Solo escala la representacion dibujada; no cambia el tamano del avion del FDM.
constexpr float kEscalaAvionMetros = 8.0f;
// La base del terreno se coloca esta distancia debajo de la altura inicial.
// Es una referencia visual fija, no una elevacion geografica real.
constexpr float kAlturaTerrenoBajoAvion = 80.0f;
// Separacion adicional para que la caja aproximada del avion no penetre el suelo.
constexpr float kMargenContactoTerreno = 0.25f;

// Estado que comparten los callbacks de GLFW y el bucle principal.
struct Aplicacion {
    // La camara necesita las dimensiones iniciales para armar su proyeccion.
    Aplicacion() : camara(kAncho, kAlto) {}

    InputHandler entradas;      // Lee teclas y produce mandos para el FDM.
    CameraSystem camara;        // Calcula las matrices de la vista activa.
    bool wireframe = false;     // Si es true, se dibujan aristas en vez de relleno.
    bool cursor_inicializado = false; // Evita un salto al tomar la primera posicion.
    double mouse_x = 0.0;       // Ultima coordenada X para calcular el delta del mouse.
    double mouse_y = 0.0;       // Ultima coordenada Y para calcular el delta del mouse.
};

// Parametros de iluminacion que se envian a los shaders de la escena.
struct Iluminacion {
    glm::vec3 direccion_sol;    // Direccion desde la que llega la luz direccional.
    glm::vec3 color_sol;        // Color/intensidad de la luz directa (RGB).
    glm::vec3 ambiente;         // Luz de base que ilumina aun las zonas en sombra.
    glm::vec3 color_niebla;     // Color con el que se mezclan objetos lejanos.
    float visibilidad;          // Distancia caracteristica a la que actua la niebla.
    bool correccion_gamma;      // Indica al shader si debe corregir gamma.
};

// Ubicaciones de uniforms consultadas una vez despues de compilar cada shader.
struct UbicacionesIluminacion {
    int direccion_sol;      // Uniform uSolDir.
    int color_sol;          // Uniform uSolColor.
    int ambiente;           // Uniform uAmbiente.
    int ojo;                // Uniform uOjo: posicion de la camara en el mundo.
    int color_niebla;       // Uniform uNieblaColor.
    int visibilidad;        // Uniform uVisibilidad.
    int correccion_gamma;   // Uniform uCorreccion.
    int pintar_normales;    // Uniform de depuracion uPintarNormales.
};

void informar_error_glfw(int codigo, const char* descripcion) {
    // GLFW deriva los errores a esta funcion para que aparezcan en la consola.
    std::cerr << "[GLFW] Error " << codigo << ": " << descripcion << '\n';
}

// GLFW llama esto al cambiar el framebuffer; se actualizan tanto OpenGL como
// la proyeccion de la camara para conservar la relacion de aspecto.
void ajustar_viewport(GLFWwindow* ventana, int ancho, int alto) {
    // Ajusta el rectangulo donde OpenGL dibuja al nuevo tamano del framebuffer.
    glViewport(0, 0, ancho, alto);
    // Recupera el estado de la aplicacion asociado a esta ventana.
    auto* aplicacion = static_cast<Aplicacion*>(
        glfwGetWindowUserPointer(ventana));
    // La camara tambien necesita el nuevo tamano para actualizar la perspectiva.
    if (aplicacion != nullptr) {
        aplicacion->camara.set_viewport(ancho, alto);
    }
}

// Atajos globales de escena. InputHandler recibe tambien cada tecla para
// manejar mandos y pausa; CameraSystem no depende directamente de GLFW.
void procesar_tecla(GLFWwindow* ventana, int tecla, int, int accion, int) {
    // El puntero lo registramos al iniciar la escena; puede no estar listo aun.
    auto* aplicacion = static_cast<Aplicacion*>(
        glfwGetWindowUserPointer(ventana));
    if (aplicacion == nullptr) {
        return;
    }

    // Escape cierra la ventana unicamente al presionar (no al mantener/repetir).
    if (tecla == GLFW_KEY_ESCAPE && accion == GLFW_PRESS) {
        glfwSetWindowShouldClose(ventana, GLFW_TRUE);
    }
    // F alterna entre poligonos rellenos y contornos (wireframe).
    if (tecla == GLFW_KEY_F && accion == GLFW_PRESS) {
        aplicacion->wireframe = !aplicacion->wireframe;
    }
    // C pasa a la siguiente vista: orbital, persecucion o cabina.
    if (tecla == GLFW_KEY_C && accion == GLFW_PRESS) {
        aplicacion->camara.cycle_mode();
    }
    // El manejador independiente traduce las demas teclas a mandos y pausa.
    aplicacion->entradas.on_key(tecla, accion);
}

// Convierte el movimiento del mouse en deltas de camara. El modo orbital es el
// unico que los consume; las vistas de cabina y persecucion siguen al avion.
CameraCommand leer_comando_camara(GLFWwindow* ventana,
                                  Aplicacion& aplicacion) {
    double x = 0.0;
    double y = 0.0;
    // GLFW entrega la posicion actual del cursor en coordenadas de ventana.
    glfwGetCursorPos(ventana, &x, &y);

    if (!aplicacion.cursor_inicializado) {
        // La primera lectura solo establece referencia; no debe mover la camara.
        aplicacion.mouse_x = x;
        aplicacion.mouse_y = y;
        aplicacion.cursor_inicializado = true;
        return {};
    }

    // Diferencia entre esta lectura y la anterior: cuanto se movio el cursor.
    const double dx = x - aplicacion.mouse_x;
    const double dy = y - aplicacion.mouse_y;
    // Guardar la lectura actual para que sea la referencia del proximo cuadro.
    aplicacion.mouse_x = x;
    aplicacion.mouse_y = y;

    CameraCommand comando;
    // Con boton izquierdo, el movimiento horizontal/vertical rota la orbital.
    if (glfwGetMouseButton(ventana, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        comando.yaw_delta = static_cast<float>(-dx) * 0.005f;
        comando.pitch_delta = static_cast<float>(-dy) * 0.005f;
    }
    // Con boton derecho, el movimiento vertical acerca o aleja la orbital.
    if (glfwGetMouseButton(ventana, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        comando.dist_delta = static_cast<float>(dy) * 0.05f;
    }
    return comando;
}

// Estima cuanto sobresale el avion por debajo del centro de gravedad segun su
// orientacion, para que alas y empenajes no atraviesen el terreno.
float despeje_avion(const glm::mat4& orientacion) {
    // Caja envolvente del modelo procedural medida desde el centro de gravedad.
    // Proyectar sus semiejes al vertical evita que alas o empenajes atraviesen
    // el relieve cuando el avión está inclinado.
    // Contribucion vertical de cada semieje local del modelo (x, y y z).
    const float radio_vertical =
        std::abs(orientacion[0].y) * 1.10f +
        std::abs(orientacion[1].y) * 0.22f +
        std::abs(orientacion[2].y) * 0.65f;
    // Se escala la caja a metros y se agrega un pequeno margen anti-penetracion.
    return kEscalaAvionMetros * radio_vertical + kMargenContactoTerreno;
}

// Se buscan una vez los nombres de uniforms que se usan en todos los cuadros.
UbicacionesIluminacion buscar_ubicaciones_iluminacion(const Shader& shader) {
    // shader.loc obtiene la ubicacion de cada variable uniforme por su nombre.
    return {
        shader.loc("uSolDir"),
        shader.loc("uSolColor"),
        shader.loc("uAmbiente"),
        shader.loc("uOjo"),
        shader.loc("uNieblaColor"),
        shader.loc("uVisibilidad"),
        shader.loc("uCorreccion"),
        shader.loc("uPintarNormales"),
    };
}

void enviar_iluminacion(const Shader& shader,
                        const UbicacionesIluminacion& ubicaciones,
                        const Iluminacion& iluminacion,
                        const glm::vec3& ojo) {
    // Los valores comunes de luz se comparten entre terreno, avion y fondo.
    // set_uniform actualiza el valor en el programa de shader activo.
    shader.set_uniform(ubicaciones.direccion_sol, iluminacion.direccion_sol);
    shader.set_uniform(ubicaciones.color_sol, iluminacion.color_sol);
    shader.set_uniform(ubicaciones.ambiente, iluminacion.ambiente);
    shader.set_uniform(ubicaciones.ojo, ojo);
    shader.set_uniform(ubicaciones.color_niebla, iluminacion.color_niebla);
    shader.set_uniform(ubicaciones.visibilidad, iluminacion.visibilidad);
    shader.set_uniform(ubicaciones.correccion_gamma,
                       iluminacion.correccion_gamma);
    shader.set_uniform(ubicaciones.pintar_normales, false);
}

glm::vec3 calcular_color_horizonte(
    const std::array<const ImageData*, 6>& caras) {
    // El promedio del centro de las caras laterales da un color de niebla que
    // combina visualmente con el horizonte del cubemap.
    constexpr std::array<int, 4> kCarasLaterales{0, 1, 4, 5};
    // Acumuladores double reducen el error al promediar muchos pixeles.
    double rojo = 0.0;
    double verde = 0.0;
    double azul = 0.0;
    std::size_t cantidad = 0;

    // Recorre solo las caras horizontales; arriba y abajo no representan horizonte.
    for (const int indice : kCarasLaterales) {
        const ImageData& cara = *caras[static_cast<std::size_t>(indice)];
        // Se toma una fila central y se promedian todos sus pixeles.
        const int fila = cara.height / 2;
        for (int columna = 0; columna < cara.width; ++columna) {
            // Cada pixel tiene cuatro bytes RGBA; se acumulan solo RGB.
            const std::size_t pixel =
                static_cast<std::size_t>(fila * cara.width + columna) * 4U;
            rojo += cara.pixels[pixel];
            verde += cara.pixels[pixel + 1U];
            azul += cara.pixels[pixel + 2U];
            ++cantidad;
        }
    }

    if (cantidad == 0U) {
        throw std::runtime_error("No se pudo calcular el color del horizonte.");
    }

    // Normaliza los bytes de 0..255 al rango 0..1, luego pasa de sRGB a lineal.
    const glm::vec3 srgb(
        static_cast<float>(rojo / static_cast<double>(cantidad)) / 255.0f,
        static_cast<float>(verde / static_cast<double>(cantidad)) / 255.0f,
        static_cast<float>(azul / static_cast<double>(cantidad)) / 255.0f);
    return glm::pow(srgb, glm::vec3(2.2f));
}

void actualizar_telemetria(GLFWwindow* ventana,
                           const dlfdm::AircraftState& estado,
                           const dlfdm::ControlInputs& controles,
                           bool pausado, bool contacto_terreno,
                           const char* vista) {
    // La telemetria se muestra en el titulo de la ventana, no dentro de la
    // escena. La conversion de posicion/actitud esta centralizada en FlightData.
    const float altitud = -estado.inertial_position.z; // NED: Z positivo apunta abajo.
    const float velocidad = glm::length(estado.body_velocity); // Modulo de la velocidad.
    std::ostringstream titulo;
    // Los angulos internos estan en radianes; degrees los convierte para mostrar.
    titulo << "Flight Simulator | ALT " << static_cast<int>(altitud)
           << " m | TAS " << static_cast<int>(velocidad)
           << " m/s | pitch " << static_cast<int>(glm::degrees(estado.theta))
           << " deg | roll " << static_cast<int>(glm::degrees(estado.phi))
           << " deg | throttle "
           << static_cast<int>(controles.throttle * 100.0f) << "%"
           << " | vista " << vista;
    if (pausado) {
        titulo << " | PAUSADO";
    }
    if (contacto_terreno) {
        titulo << " | CONTACTO CON TERRENO";
    }
    glfwSetWindowTitle(ventana, titulo.str().c_str());
}

} // namespace

int main() {
    // GLFW y OpenGL se inicializan antes de crear los recursos de la escena.
    glfwSetErrorCallback(informar_error_glfw);
    if (glfwInit() != GLFW_TRUE) {
        // Sin GLFW no hay ventana ni contexto donde dibujar.
        std::cerr << "No se pudo inicializar GLFW.\n";
        return 1;
    }

    // Se solicita OpenGL 4.6 y el perfil moderno Core (sin funciones antiguas).
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Crea la ventana inicial usando el ancho y alto definidos arriba.
    GLFWwindow* ventana = glfwCreateWindow(
        kAncho, kAlto, "Flight Simulator", nullptr, nullptr);
    if (ventana == nullptr) {
        // Si falla la ventana, liberar GLFW antes de terminar.
        std::cerr << "No se pudo crear la ventana OpenGL.\n";
        glfwTerminate();
        return 1;
    }

    // Las llamadas OpenGL siguientes operan sobre el contexto de esta ventana.
    glfwMakeContextCurrent(ventana);
    if (gladLoadGL(glfwGetProcAddress) == 0) {
        // GLAD carga las direcciones de las funciones OpenGL disponibles.
        std::cerr << "No se pudieron cargar las funciones de OpenGL con GLAD.\n";
        glfwDestroyWindow(ventana);
        glfwTerminate();
        return 1;
    }

    // El codigo de salida pasa a 1 si ocurre una excepcion al preparar o dibujar.
    int resultado = 0;
    try {
        {
            // Este estado vive mientras se ejecuta la escena y lo usan callbacks.
            Aplicacion aplicacion;
            // Los callbacks recuperan este estado con glfwGetWindowUserPointer.
            glfwSetWindowUserPointer(ventana, &aplicacion);
            // Registra las funciones llamadas por eventos de ventana/teclado.
            glfwSetFramebufferSizeCallback(ventana, ajustar_viewport);
            glfwSetKeyCallback(ventana, procesar_tecla);
            // Espera al refresco de pantalla para evitar dibujar cuadros de mas.
            glfwSwapInterval(1);
            // Establece el viewport inicial; el callback lo actualiza al cambiar.
            glViewport(0, 0, kAncho, kAlto);

            // El administrador busca los recursos relativos a esta carpeta.
            ResourceManager recursos("assets");
            // Los shaders y las imagenes se cargan desde assets/. El shader de
            // iluminacion comun se concatena con cada fragment shader.
            // Programa para la malla del terreno.
            const ShaderSource& fuente_terreno =
                recursos.load_shader_source(
                    "terreno", "shaders/terreno_lit.vs",
                    "shaders/terreno_lit.fs");
            // Programa para las piezas del modelo del avion.
            const ShaderSource& fuente_avion =
                recursos.load_shader_source(
                    "avion", "shaders/avion_lit.vs",
                    "shaders/avion_lit.fs");
            // Programa para el cubemap/fondo y el suelo que continua a lo lejos.
            const ShaderSource& fuente_fondo =
                recursos.load_shader_source(
                    "fondo", "shaders/background.vs",
                    "shaders/background_lit.fs");
            // Codigo GLSL compartido por los fragment shaders de la escena.
            const std::string& iluminacion_comun =
                recursos.load_text("iluminacion", "shaders/iluminacion.glsl");
            // Version GLSL antepuesta al compilar cada programa.
            const std::string version_glsl = "#version 460 core\n";

            // Imagen del suelo; se comparte entre terreno y fondo.
            const ImageData& pasto = recursos.load_image(
                "pasto", "textures/suelo/grass.png", true); // true: invierte filas para OpenGL.
            // Caras del cubemap en el orden esperado por Texture::load_cubemap.
            const std::array<const ImageData*, 6> caras_cielo{
                &recursos.load_image(
                    "cielo_derecha", "textures/skybox/right.png", false), // No invertir cubemap.
                &recursos.load_image(
                    "cielo_izquierda", "textures/skybox/left.png", false),
                &recursos.load_image(
                    "cielo_arriba", "textures/skybox/top.png", false),
                &recursos.load_image(
                    "cielo_abajo", "textures/skybox/bottom.png", false),
                &recursos.load_image(
                    "cielo_frente", "textures/skybox/front.png", false),
                &recursos.load_image(
                    "cielo_atras", "textures/skybox/back.png", false),
            };

            // Parametros fisicos del JetTrainer usado por el solver.
            const dlfdm::AircraftParameters parametros =
                dlfdm::jettrainer::load_model();
            // El integrador usa el mismo paso fijo que el bucle principal.
            dlfdm::FDMSolver fdm(parametros, static_cast<float>(kPasoFijo));
            // El trim establece el estado inicial de vuelo y los mandos
            // iniciales. InputHandler toma esos limites y ese neutro.
            // Condicion de equilibrio: ISA, 5000 m y 150 m/s.
            const dlfdm::TrimPoint trim =
                dlfdm::jettrainer::get_trim_condition(
                    dlfdm::jettrainer::TrimCondition::kISA5000TAS150);
            fdm.setState(trim.state); // Posicion, velocidad y actitud iniciales.
            aplicacion.entradas.set_limits(parametros); // Limites fisicos de los mandos.
            aplicacion.entradas.set_controls(trim.controls); // Mandos iniciales de trim.
            aplicacion.entradas.set_neutral(trim.controls); // Neutro al soltar superficies.

            // Construye el avion visual procedural; no es la geometria del FDM.
            Aircraft avion;
            avion.init();

            // Configuracion geometrica del terreno: celdas, tamano, repeticion
            // de textura y parametros del generador procedural de alturas.
            constexpr unsigned int kCeldasTerreno = 512U; // Celdas por lado (513 vertices).
            constexpr float kPasoTerreno = 16.0f; // Metros entre nodos vecinos.
            constexpr float kTeselasTextura = 512.0f; // Repeticiones de textura por lado.
            // Ancho total de la grilla en metros.
            const float ancho_terreno =
                static_cast<float>(kCeldasTerreno) * kPasoTerreno;
            // Semilla, frecuencia y amplitud del ruido que define las alturas.
            const terreno::Alturas alturas(12345, 0.04f, 75.0f);
            Mesh mesh_terreno;
            // Convierte las alturas en triangulos y los carga a buffers de GPU.
            mesh_terreno.load(terreno::grid(
                kCeldasTerreno, kPasoTerreno, alturas));

            // El cubo se usa como geometria envolvente para dibujar el cielo.
            Mesh mesh_cielo;
            mesh_cielo.load(primitives::cube());

            // Sube la imagen del pasto a una textura 2D.
            Texture textura_pasto;
            textura_pasto.load_2d(pasto);
            // Combina las seis imagenes en una textura cubemap.
            Texture textura_cielo;
            textura_cielo.load_cubemap(caras_cielo);

            // Compilacion de shaders: si cualquiera falla, se detiene el inicio
            // para informar el error en vez de dibujar una escena incompleta.
            Shader shader_terreno;
            Shader shader_avion;
            Shader shader_fondo;
            // Cada programa combina su vertex shader, version, iluminacion comun
            // y su fragment shader especifico.
            const bool shaders_correctos =
                shader_terreno.compile_from_sources(
                    fuente_terreno.vs,
                    {version_glsl, iluminacion_comun, fuente_terreno.fs}) &&
                shader_avion.compile_from_sources(
                    fuente_avion.vs,
                    {version_glsl, iluminacion_comun, fuente_avion.fs}) &&
                shader_fondo.compile_from_sources(
                    fuente_fondo.vs,
                    {version_glsl, iluminacion_comun, fuente_fondo.fs});
            if (!shaders_correctos) {
                throw std::runtime_error(
                    "No se pudieron compilar todos los shaders de la escena.");
            }

            // Convierte el estado inicial NED a coordenadas de la escena OpenGL.
            const FlightData posicion_inicial = to_world(fdm.getState());
            // Fija la altura base del terreno respecto del avion al iniciar.
            const float altura_suelo =
                posicion_inicial.position.y - kAlturaTerrenoBajoAvion;
            // Centra la grilla bajo la posicion inicial del avion en X y Z.
            const glm::vec2 origen_terreno(
                posicion_inicial.position.x - ancho_terreno * 0.5f,
                posicion_inicial.position.z - ancho_terreno * 0.5f);
            // Matriz que traslada la grilla desde su origen local al mundo.
            const glm::mat4 modelo_terreno = glm::translate(
                glm::mat4(1.0f),
                glm::vec3(origen_terreno.x, altura_suelo, origen_terreno.y));
            // Usa las mismas alturas y dimensiones para que el contacto coincida
            // con la superficie que se ve dibujada.
            const GroundContact contacto_terreno(
                alturas, kCeldasTerreno, kPasoTerreno,
                origen_terreno, altura_suelo);
            // Metros de mundo que representa cada repeticion de textura del fondo.
            const float metros_por_tesela = ancho_terreno / kTeselasTextura;

            // Cambiar estos valores modifica color/direccion de la luz,
            // ambiente, niebla y correccion gamma para todos los objetos.
            Iluminacion iluminacion{
                glm::normalize(glm::vec3(0.5f, 0.65f, 0.45f)), // Direccion normalizada del sol.
                glm::vec3(0.70f, 0.68f, 0.64f), // Color de la luz directa.
                glm::vec3(0.30f, 0.32f, 0.36f), // Color e intensidad ambiente.
                calcular_color_horizonte(caras_cielo), // Niebla acorde al cielo.
                1800.0f, // Distancia caracteristica de visibilidad de la niebla.
                true, // Aplicar correccion gamma en el shader.
            };
            // Cada programa tiene sus propias ubicaciones de uniformes.
            const UbicacionesIluminacion luz_terreno =
                buscar_ubicaciones_iluminacion(shader_terreno);
            const UbicacionesIluminacion luz_avion =
                buscar_ubicaciones_iluminacion(shader_avion);
            const UbicacionesIluminacion luz_fondo =
                buscar_ubicaciones_iluminacion(shader_fondo);

            // Cachea ubicaciones de matrices y parametros para enviarlos despues.
            const int terreno_modelo = shader_terreno.loc("uModel"); // Modelo local -> mundo.
            const int terreno_vista = shader_terreno.loc("uView"); // Mundo -> camara.
            const int terreno_proyeccion = shader_terreno.loc("uProjection"); // Camara -> pantalla.
            const int terreno_teselas = shader_terreno.loc("uTeselas"); // Repeticion del pasto.
            const int avion_modelo = shader_avion.loc("uModel"); // Transformacion de cada pieza.
            const int avion_vista = shader_avion.loc("uView"); // Mundo -> camara.
            const int avion_proyeccion = shader_avion.loc("uProjection"); // Camara -> pantalla.
            const int avion_matriz_normal = shader_avion.loc("uMatNormal"); // Transforma normales.
            const int avion_kd = shader_avion.loc("uMaterial.kd"); // Componente difusa.
            const int avion_ks = shader_avion.loc("uMaterial.ks"); // Componente especular.
            const int avion_ns = shader_avion.loc("uMaterial.ns"); // Tamano del brillo especular.
            const int fondo_vista = shader_fondo.loc("uView"); // Orientacion de camara, sin traslacion.
            const int fondo_proyeccion = shader_fondo.loc("uProjection"); // Perspectiva.
            const int fondo_altura_suelo = shader_fondo.loc("uAlturaSuelo"); // Base del suelo lejano.
            const int fondo_metros_tesela =
                shader_fondo.loc("uMetrosTesela"); // Escala de textura del fondo.

            // Estado global de OpenGL para la escena opaca.
            glEnable(GL_DEPTH_TEST); // Oculta fragmentos que quedan detras de otros.
            glEnable(GL_CULL_FACE); // No dibuja caras orientadas hacia atras.

            std::cout
                << "Escena 3D iniciada con el modelo visual de entrenamiento.\n"
                << "WASD: baston (cabeceo y alabeo) | Flechas: potencia y timon\n"
                << "Espacio: pausa | F: alambre | C: cambiar vista | Escape: salir\n"
                << "Vistas: orbital, persecucion y cabina (tecla C)\n"
                << "Mouse izquierdo: orbitar | mouse derecho: acercar/alejar\n";

            double anterior = glfwGetTime(); // Marca de tiempo del cuadro previo.
            double acumulador = 0.0; // Tiempo pendiente de consumir en pasos del FDM.
            double tiempo_telemetria = 0.0; // Cuenta cuando toca refrescar el titulo.
            bool en_contacto_terreno = false; // Estado que se muestra en telemetria.
            std::vector<RenderItem> piezas_avion;
            piezas_avion.reserve(7U); // El modelo visual se compone de siete piezas.

            while (glfwWindowShouldClose(ventana) == GLFW_FALSE) {
                // Atiende teclado, mouse y eventos de ventana antes de actualizar.
                glfwPollEvents();

                // El tiempo entre cuadros se limita para evitar que una pausa
                // del sistema cause una recuperacion enorme de la simulacion.
                const double ahora = glfwGetTime();
                // Delta real no negativo y limitado al maximo configurado arriba.
                const double frame_dt = std::clamp(
                    ahora - anterior, 0.0, kMaxTiempoPorCuadro);
                anterior = ahora; // Esta lectura sera la referencia del proximo cuadro.

                // Lee las teclas mantenidas; dt determina cuanto cambian los mandos.
                aplicacion.entradas.update(
                    ventana, static_cast<float>(frame_dt));
                if (aplicacion.entradas.paused()) {
                    // Al pausar se descarta tiempo pendiente para que no haya salto al reanudar.
                    acumulador = 0.0;
                } else {
                    // El FDM siempre avanza con paso fijo; pueden ejecutarse
                    // cero, uno o varios pasos durante un cuadro renderizado.
                    acumulador += frame_dt;
                    while (acumulador >= kPasoFijo) {
                        // Integra las ecuaciones del avion un paso usando los mandos actuales.
                        fdm.update(aplicacion.entradas.controls());
                        // Se copia el estado porque el contacto con suelo puede corregirlo.
                        dlfdm::AircraftState estado_corregido = fdm.getState();
                        // El contacto necesita posicion y orientacion expresadas en el mundo.
                        const FlightData posicion_corregida =
                            to_world(estado_corregido);
                        const glm::mat4 orientacion_corregida =
                            orientacion_vuelo(posicion_corregida);
                        // Corrige penetracion y velocidad hacia abajo si toca la superficie.
                        en_contacto_terreno = contacto_terreno.resolve(
                            estado_corregido,
                            despeje_avion(orientacion_corregida));
                        if (en_contacto_terreno) {
                            // Guarda en el solver el estado que ya fue corregido.
                            fdm.setState(estado_corregido);
                        }
                        // Consume exactamente un paso de la deuda de simulacion.
                        acumulador -= kPasoFijo;
                    }
                }

                // Toma el ultimo estado integrado para calcular pose visual y camara.
                const dlfdm::AircraftState& estado = fdm.getState();
                const FlightData mundo = to_world(estado);
                const glm::mat4 orientacion = orientacion_vuelo(mundo);
                // Actualiza el modelo visual con posicion, actitud y escala en metros.
                avion.set_pose(
                    mundo.position, orientacion,
                    kEscalaAvionMetros);
                // La camara sigue la pose y recibe los deltas del mouse de este cuadro.
                aplicacion.camara.update(
                    mundo.position, orientacion,
                    leer_comando_camara(ventana, aplicacion));

                // La telemetria se refresca cada intervalo, no necesariamente en cada frame.
                tiempo_telemetria += frame_dt;
                if (tiempo_telemetria >= kIntervaloTelemetria) {
                    tiempo_telemetria = 0.0; // Reinicia el contador tras actualizar.
                    actualizar_telemetria(
                        ventana, estado, aplicacion.entradas.controls(),
                        aplicacion.entradas.paused(),
                        en_contacto_terreno,
                        aplicacion.camara.mode_name());
                }

                // El modo se elige con F; afecta a terreno, avion y cubo del fondo.
                glPolygonMode(
                    GL_FRONT_AND_BACK,
                    aplicacion.wireframe ? GL_LINE : GL_FILL);
                // Limpia imagen y buffer de profundidad antes de empezar el frame.
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                // La camara entrega las matrices usadas por todos los shaders.
                const CameraData& camara = aplicacion.camara.data();
                // El cielo se queda centrado en el ojo: se elimina la traslacion de la vista.
                const glm::mat4 vista_sin_traslacion =
                    glm::mat4(glm::mat3(camara.view));
                // La inversa de view contiene la posicion del ojo en coordenadas mundo.
                const glm::vec3 ojo =
                    glm::vec3(glm::inverse(camara.view)[3]);

                // Orden de dibujo: cielo/fondo, terreno y luego piezas del
                // avion. El cielo no escribe profundidad para no tapar objetos.
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                // Activa el shader que pinta cubemap y suelo lejano.
                shader_fondo.use();
                // Al fondo se le pasa una vista sin posicion, pero la perspectiva normal.
                shader_fondo.set_uniform(fondo_vista, vista_sin_traslacion);
                shader_fondo.set_uniform(fondo_proyeccion, camara.projection);
                // Estos valores hacen coincidir el suelo lejano con el terreno cercano.
                shader_fondo.set_uniform(fondo_altura_suelo, altura_suelo);
                shader_fondo.set_uniform(
                    fondo_metros_tesela, metros_por_tesela);
                enviar_iluminacion(
                    shader_fondo, luz_fondo, iluminacion, ojo);
                textura_cielo.bind(0U); // Unidad 0: cubemap del cielo.
                textura_pasto.bind(1U); // Unidad 1: pasto para el suelo lejano.
                glBindVertexArray(mesh_cielo.vao()); // Selecciona la malla del cubo.
                // Dibuja los triangulos indexados del cubo que envuelve la escena.
                glDrawElements(
                    GL_TRIANGLES, mesh_cielo.count(),
                    GL_UNSIGNED_INT, nullptr);
                glDepthMask(GL_TRUE); // Restaura escritura de profundidad para objetos.
                glEnable(GL_CULL_FACE); // Restaura el descarte de caras traseras.

                // Pinta la malla de terreno con las matrices y luz de la escena.
                shader_terreno.use();
                shader_terreno.set_uniform(terreno_modelo, modelo_terreno);
                shader_terreno.set_uniform(terreno_vista, camara.view);
                shader_terreno.set_uniform(
                    terreno_proyeccion, camara.projection);
                shader_terreno.set_uniform(
                    terreno_teselas, kTeselasTextura);
                enviar_iluminacion(
                    shader_terreno, luz_terreno, iluminacion, ojo);
                textura_pasto.bind(0U); // Unidad 0: textura 2D del terreno.
                glBindVertexArray(mesh_terreno.vao()); // Selecciona la malla del terreno.
                glDrawElements(
                    GL_TRIANGLES, mesh_terreno.count(),
                    GL_UNSIGNED_INT, nullptr);

                // Recolecta las mallas/materiales actuales de las piezas del avion.
                avion.collect(piezas_avion);
                shader_avion.use();
                shader_avion.set_uniform(avion_vista, camara.view);
                shader_avion.set_uniform(
                    avion_proyeccion, camara.projection);
                enviar_iluminacion(
                    shader_avion, luz_avion, iluminacion, ojo);
                for (const RenderItem& pieza : piezas_avion) {
                    // Cada pieza puede tener modelo y material propios, aunque
                    // todas comparten shader, vista, proyeccion e iluminacion.
                    shader_avion.set_uniform(avion_modelo, pieza.model);
                    const glm::mat3 matriz_normal = glm::transpose(
                        glm::inverse(glm::mat3(pieza.model)));
                    // La matriz normal corrige la orientacion al transformar normales.
                    shader_avion.set_uniform(
                        avion_matriz_normal, matriz_normal);
                    shader_avion.set_uniform(avion_kd, pieza.material.kd);
                    shader_avion.set_uniform(avion_ks, pieza.material.ks);
                    shader_avion.set_uniform(avion_ns, pieza.material.ns);
                    glBindVertexArray(pieza.mesh->vao()); // Activa la geometria de esta pieza.
                    glDrawElements(
                        GL_TRIANGLES, pieza.mesh->count(),
                        GL_UNSIGNED_INT, nullptr);
                }

                // Presenta el frame terminado y alterna el buffer visible.
                glfwSwapBuffers(ventana);
            }
        }
    } catch (const std::exception& error) {
        // Los errores de carga/compilacion/ejecucion se reportan y cambian el exit code.
        std::cerr << "Error al iniciar o ejecutar la escena: "
                  << error.what() << '\n';
        resultado = 1;
    }

    // Libera primero la ventana/contexto y despues el sistema GLFW.
    glfwDestroyWindow(ventana);
    glfwTerminate();
    // 0 indica salida normal; 1 indica fallo al iniciar o ejecutar la escena.
    return resultado;
}
