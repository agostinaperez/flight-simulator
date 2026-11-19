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

constexpr int kAncho = 1280;
constexpr int kAlto = 720;
constexpr double kPasoFijo = 1.0 / 120.0;
constexpr double kMaxTiempoPorCuadro = 0.25;
constexpr double kIntervaloTelemetria = 0.25;
constexpr float kEscalaAvionMetros = 8.0f;
constexpr float kAlturaTerrenoBajoAvion = 80.0f;
constexpr float kMargenContactoTerreno = 0.25f;

struct Aplicacion {
    Aplicacion() : camara(kAncho, kAlto) {}

    InputHandler entradas;
    CameraSystem camara;
    bool wireframe = false;
    bool cursor_inicializado = false;
    double mouse_x = 0.0;
    double mouse_y = 0.0;
};

struct Iluminacion {
    glm::vec3 direccion_sol;
    glm::vec3 color_sol;
    glm::vec3 ambiente;
    glm::vec3 color_niebla;
    float visibilidad;
    bool correccion_gamma;
};

struct UbicacionesIluminacion {
    int direccion_sol;
    int color_sol;
    int ambiente;
    int ojo;
    int color_niebla;
    int visibilidad;
    int correccion_gamma;
    int pintar_normales;
};

void informar_error_glfw(int codigo, const char* descripcion) {
    std::cerr << "[GLFW] Error " << codigo << ": " << descripcion << '\n';
}

void ajustar_viewport(GLFWwindow* ventana, int ancho, int alto) {
    glViewport(0, 0, ancho, alto);
    auto* aplicacion = static_cast<Aplicacion*>(
        glfwGetWindowUserPointer(ventana));
    if (aplicacion != nullptr) {
        aplicacion->camara.set_viewport(ancho, alto);
    }
}

void procesar_tecla(GLFWwindow* ventana, int tecla, int, int accion, int) {
    auto* aplicacion = static_cast<Aplicacion*>(
        glfwGetWindowUserPointer(ventana));
    if (aplicacion == nullptr) {
        return;
    }

    if (tecla == GLFW_KEY_ESCAPE && accion == GLFW_PRESS) {
        glfwSetWindowShouldClose(ventana, GLFW_TRUE);
    }
    if (tecla == GLFW_KEY_F && accion == GLFW_PRESS) {
        aplicacion->wireframe = !aplicacion->wireframe;
    }
    aplicacion->entradas.on_key(tecla, accion);
}

CameraCommand leer_comando_camara(GLFWwindow* ventana,
                                  Aplicacion& aplicacion) {
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(ventana, &x, &y);

    if (!aplicacion.cursor_inicializado) {
        aplicacion.mouse_x = x;
        aplicacion.mouse_y = y;
        aplicacion.cursor_inicializado = true;
        return {};
    }

    const double dx = x - aplicacion.mouse_x;
    const double dy = y - aplicacion.mouse_y;
    aplicacion.mouse_x = x;
    aplicacion.mouse_y = y;

    CameraCommand comando;
    if (glfwGetMouseButton(ventana, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        comando.yaw_delta = static_cast<float>(-dx) * 0.005f;
        comando.pitch_delta = static_cast<float>(-dy) * 0.005f;
    }
    if (glfwGetMouseButton(ventana, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        comando.dist_delta = static_cast<float>(dy) * 0.05f;
    }
    return comando;
}

float despeje_avion(const glm::mat4& orientacion) {
    // Caja envolvente del modelo procedural medida desde el centro de gravedad.
    // Proyectar sus semiejes al vertical evita que alas o empenajes atraviesen
    // el relieve cuando el avión está inclinado.
    const float radio_vertical =
        std::abs(orientacion[0].y) * 1.10f +
        std::abs(orientacion[1].y) * 0.22f +
        std::abs(orientacion[2].y) * 0.65f;
    return kEscalaAvionMetros * radio_vertical + kMargenContactoTerreno;
}

UbicacionesIluminacion buscar_ubicaciones_iluminacion(const Shader& shader) {
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
    constexpr std::array<int, 4> kCarasLaterales{0, 1, 4, 5};
    double rojo = 0.0;
    double verde = 0.0;
    double azul = 0.0;
    std::size_t cantidad = 0;

    for (const int indice : kCarasLaterales) {
        const ImageData& cara = *caras[static_cast<std::size_t>(indice)];
        const int fila = cara.height / 2;
        for (int columna = 0; columna < cara.width; ++columna) {
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

    const glm::vec3 srgb(
        static_cast<float>(rojo / static_cast<double>(cantidad)) / 255.0f,
        static_cast<float>(verde / static_cast<double>(cantidad)) / 255.0f,
        static_cast<float>(azul / static_cast<double>(cantidad)) / 255.0f);
    return glm::pow(srgb, glm::vec3(2.2f));
}

void actualizar_telemetria(GLFWwindow* ventana,
                           const dlfdm::AircraftState& estado,
                           const dlfdm::ControlInputs& controles,
                           bool pausado, bool contacto_terreno) {
    const float altitud = -estado.inertial_position.z;
    const float velocidad = glm::length(estado.body_velocity);
    std::ostringstream titulo;
    titulo << "Flight Simulator | ALT " << static_cast<int>(altitud)
           << " m | TAS " << static_cast<int>(velocidad)
           << " m/s | pitch " << static_cast<int>(glm::degrees(estado.theta))
           << " deg | roll " << static_cast<int>(glm::degrees(estado.phi))
           << " deg | throttle "
           << static_cast<int>(controles.throttle * 100.0f) << "%";
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
    glfwSetErrorCallback(informar_error_glfw);
    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "No se pudo inicializar GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* ventana = glfwCreateWindow(
        kAncho, kAlto, "Flight Simulator", nullptr, nullptr);
    if (ventana == nullptr) {
        std::cerr << "No se pudo crear la ventana OpenGL.\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(ventana);
    if (gladLoadGL(glfwGetProcAddress) == 0) {
        std::cerr << "No se pudieron cargar las funciones de OpenGL con GLAD.\n";
        glfwDestroyWindow(ventana);
        glfwTerminate();
        return 1;
    }

    int resultado = 0;
    try {
        {
            Aplicacion aplicacion;
            glfwSetWindowUserPointer(ventana, &aplicacion);
            glfwSetFramebufferSizeCallback(ventana, ajustar_viewport);
            glfwSetKeyCallback(ventana, procesar_tecla);
            glfwSwapInterval(1);
            glViewport(0, 0, kAncho, kAlto);

            ResourceManager recursos("assets");
            const ShaderSource& fuente_terreno =
                recursos.load_shader_source(
                    "terreno", "shaders/terreno_lit.vs",
                    "shaders/terreno_lit.fs");
            const ShaderSource& fuente_avion =
                recursos.load_shader_source(
                    "avion", "shaders/avion_lit.vs",
                    "shaders/avion_lit.fs");
            const ShaderSource& fuente_fondo =
                recursos.load_shader_source(
                    "fondo", "shaders/background.vs",
                    "shaders/background_lit.fs");
            const std::string& iluminacion_comun =
                recursos.load_text("iluminacion", "shaders/iluminacion.glsl");
            const std::string version_glsl = "#version 460 core\n";

            const ImageData& pasto = recursos.load_image(
                "pasto", "textures/suelo/grass.png", true);
            const std::array<const ImageData*, 6> caras_cielo{
                &recursos.load_image(
                    "cielo_derecha", "textures/skybox/right.png", false),
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

            const dlfdm::AircraftParameters parametros =
                dlfdm::jettrainer::load_model();
            dlfdm::FDMSolver fdm(parametros, static_cast<float>(kPasoFijo));
            const dlfdm::TrimPoint trim =
                dlfdm::jettrainer::get_trim_condition(
                    dlfdm::jettrainer::TrimCondition::kISA5000TAS150);
            fdm.setState(trim.state);
            aplicacion.entradas.set_limits(parametros);
            aplicacion.entradas.set_controls(trim.controls);

            Aircraft avion;
            avion.init();

            constexpr unsigned int kCeldasTerreno = 512U;
            constexpr float kPasoTerreno = 16.0f;
            constexpr float kTeselasTextura = 512.0f;
            const float ancho_terreno =
                static_cast<float>(kCeldasTerreno) * kPasoTerreno;
            const terreno::Alturas alturas(12345, 0.04f, 75.0f);
            Mesh mesh_terreno;
            mesh_terreno.load(terreno::grid(
                kCeldasTerreno, kPasoTerreno, alturas));

            Mesh mesh_cielo;
            mesh_cielo.load(primitives::cube());

            Texture textura_pasto;
            textura_pasto.load_2d(pasto);
            Texture textura_cielo;
            textura_cielo.load_cubemap(caras_cielo);

            Shader shader_terreno;
            Shader shader_avion;
            Shader shader_fondo;
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

            const FlightData posicion_inicial = to_world(fdm.getState());
            const float altura_suelo =
                posicion_inicial.position.y - kAlturaTerrenoBajoAvion;
            const glm::vec2 origen_terreno(
                posicion_inicial.position.x - ancho_terreno * 0.5f,
                posicion_inicial.position.z - ancho_terreno * 0.5f);
            const glm::mat4 modelo_terreno = glm::translate(
                glm::mat4(1.0f),
                glm::vec3(origen_terreno.x, altura_suelo, origen_terreno.y));
            const GroundContact contacto_terreno(
                alturas, kCeldasTerreno, kPasoTerreno,
                origen_terreno, altura_suelo);
            const float metros_por_tesela = ancho_terreno / kTeselasTextura;

            Iluminacion iluminacion{
                glm::normalize(glm::vec3(0.5f, 0.65f, 0.45f)),
                glm::vec3(0.70f, 0.68f, 0.64f),
                glm::vec3(0.30f, 0.32f, 0.36f),
                calcular_color_horizonte(caras_cielo),
                1800.0f,
                true,
            };
            const UbicacionesIluminacion luz_terreno =
                buscar_ubicaciones_iluminacion(shader_terreno);
            const UbicacionesIluminacion luz_avion =
                buscar_ubicaciones_iluminacion(shader_avion);
            const UbicacionesIluminacion luz_fondo =
                buscar_ubicaciones_iluminacion(shader_fondo);

            const int terreno_modelo = shader_terreno.loc("uModel");
            const int terreno_vista = shader_terreno.loc("uView");
            const int terreno_proyeccion = shader_terreno.loc("uProjection");
            const int terreno_teselas = shader_terreno.loc("uTeselas");
            const int avion_modelo = shader_avion.loc("uModel");
            const int avion_vista = shader_avion.loc("uView");
            const int avion_proyeccion = shader_avion.loc("uProjection");
            const int avion_matriz_normal = shader_avion.loc("uMatNormal");
            const int avion_kd = shader_avion.loc("uMaterial.kd");
            const int avion_ks = shader_avion.loc("uMaterial.ks");
            const int avion_ns = shader_avion.loc("uMaterial.ns");
            const int fondo_vista = shader_fondo.loc("uView");
            const int fondo_proyeccion = shader_fondo.loc("uProjection");
            const int fondo_altura_suelo = shader_fondo.loc("uAlturaSuelo");
            const int fondo_metros_tesela =
                shader_fondo.loc("uMetrosTesela");

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);

            std::cout
                << "Escena 3D iniciada con el modelo visual de entrenamiento.\n"
                << "Flechas: elevador y alerones | A/D: timon | W/S: potencia\n"
                << "Espacio: pausa | F: alambre | Escape: salir\n"
                << "Mouse izquierdo: orbitar | mouse derecho: acercar/alejar\n";

            double anterior = glfwGetTime();
            double acumulador = 0.0;
            double tiempo_telemetria = 0.0;
            bool en_contacto_terreno = false;
            std::vector<RenderItem> piezas_avion;
            piezas_avion.reserve(7U);

            while (glfwWindowShouldClose(ventana) == GLFW_FALSE) {
                glfwPollEvents();

                const double ahora = glfwGetTime();
                const double frame_dt = std::clamp(
                    ahora - anterior, 0.0, kMaxTiempoPorCuadro);
                anterior = ahora;

                aplicacion.entradas.update(
                    ventana, static_cast<float>(frame_dt));
                if (aplicacion.entradas.paused()) {
                    acumulador = 0.0;
                } else {
                    acumulador += frame_dt;
                    while (acumulador >= kPasoFijo) {
                        fdm.update(aplicacion.entradas.controls());
                        dlfdm::AircraftState estado_corregido = fdm.getState();
                        const FlightData posicion_corregida =
                            to_world(estado_corregido);
                        const glm::mat4 orientacion_corregida =
                            orientacion_vuelo(posicion_corregida);
                        en_contacto_terreno = contacto_terreno.resolve(
                            estado_corregido,
                            despeje_avion(orientacion_corregida));
                        if (en_contacto_terreno) {
                            fdm.setState(estado_corregido);
                        }
                        acumulador -= kPasoFijo;
                    }
                }

                const dlfdm::AircraftState& estado = fdm.getState();
                const FlightData mundo = to_world(estado);
                avion.set_pose(
                    mundo.position, orientacion_vuelo(mundo),
                    kEscalaAvionMetros);
                aplicacion.camara.update(
                    mundo.position,
                    glm::vec3(mundo.phi, mundo.theta, mundo.psi),
                    leer_comando_camara(ventana, aplicacion));

                tiempo_telemetria += frame_dt;
                if (tiempo_telemetria >= kIntervaloTelemetria) {
                    tiempo_telemetria = 0.0;
                    actualizar_telemetria(
                        ventana, estado, aplicacion.entradas.controls(),
                        aplicacion.entradas.paused(),
                        en_contacto_terreno);
                }

                glPolygonMode(
                    GL_FRONT_AND_BACK,
                    aplicacion.wireframe ? GL_LINE : GL_FILL);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                const CameraData& camara = aplicacion.camara.data();
                const glm::mat4 vista_sin_traslacion =
                    glm::mat4(glm::mat3(camara.view));
                const glm::vec3 ojo =
                    glm::vec3(glm::inverse(camara.view)[3]);

                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                shader_fondo.use();
                shader_fondo.set_uniform(fondo_vista, vista_sin_traslacion);
                shader_fondo.set_uniform(fondo_proyeccion, camara.projection);
                shader_fondo.set_uniform(fondo_altura_suelo, altura_suelo);
                shader_fondo.set_uniform(
                    fondo_metros_tesela, metros_por_tesela);
                enviar_iluminacion(
                    shader_fondo, luz_fondo, iluminacion, ojo);
                textura_cielo.bind(0U);
                textura_pasto.bind(1U);
                glBindVertexArray(mesh_cielo.vao());
                glDrawElements(
                    GL_TRIANGLES, mesh_cielo.count(),
                    GL_UNSIGNED_INT, nullptr);
                glDepthMask(GL_TRUE);
                glEnable(GL_CULL_FACE);

                shader_terreno.use();
                shader_terreno.set_uniform(terreno_modelo, modelo_terreno);
                shader_terreno.set_uniform(terreno_vista, camara.view);
                shader_terreno.set_uniform(
                    terreno_proyeccion, camara.projection);
                shader_terreno.set_uniform(
                    terreno_teselas, kTeselasTextura);
                enviar_iluminacion(
                    shader_terreno, luz_terreno, iluminacion, ojo);
                textura_pasto.bind(0U);
                glBindVertexArray(mesh_terreno.vao());
                glDrawElements(
                    GL_TRIANGLES, mesh_terreno.count(),
                    GL_UNSIGNED_INT, nullptr);

                avion.collect(piezas_avion);
                shader_avion.use();
                shader_avion.set_uniform(avion_vista, camara.view);
                shader_avion.set_uniform(
                    avion_proyeccion, camara.projection);
                enviar_iluminacion(
                    shader_avion, luz_avion, iluminacion, ojo);
                for (const RenderItem& pieza : piezas_avion) {
                    shader_avion.set_uniform(avion_modelo, pieza.model);
                    const glm::mat3 matriz_normal = glm::transpose(
                        glm::inverse(glm::mat3(pieza.model)));
                    shader_avion.set_uniform(
                        avion_matriz_normal, matriz_normal);
                    shader_avion.set_uniform(avion_kd, pieza.material.kd);
                    shader_avion.set_uniform(avion_ks, pieza.material.ks);
                    shader_avion.set_uniform(avion_ns, pieza.material.ns);
                    glBindVertexArray(pieza.mesh->vao());
                    glDrawElements(
                        GL_TRIANGLES, pieza.mesh->count(),
                        GL_UNSIGNED_INT, nullptr);
                }

                glfwSwapBuffers(ventana);
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Error al iniciar o ejecutar la escena: "
                  << error.what() << '\n';
        resultado = 1;
    }

    glfwDestroyWindow(ventana);
    glfwTerminate();
    return resultado;
}
