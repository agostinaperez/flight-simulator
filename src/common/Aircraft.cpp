// Aircraft.cpp — implementación del modelo de la aeronave.
// Acá se decide el DESPIECE: qué primitiva aproxima cada parte del avión, con
// qué medidas y con qué transformaciones. Todas las medidas son RELATIVAS al
// largo del fuselaje (L = 1), como pide la actividad de aula.
//
// SISTEMA DE REFERENCIA DEL MODELO (fijado acá, coincide con la presentación):
//   * Origen en la NARIZ.
//   * +X hacia el ala DERECHA (lateral).
//   * +Y hacia ARRIBA.
//   * +Z de la nariz hacia la COLA.
//   De ahí: CABECEO = rotar alrededor de X (el eje de las alas, paralelo a la
//   dirección del ala); GUIÑADA = rotar alrededor de Y; ROLIDO = rotar
//   alrededor de Z (el eje nariz→cola). La presentación usa exactamente estos
//   ejes en su ejemplo de código.
//
// PUNTO DE REFERENCIA DE LA POSE (kPuntoReferencia):
//   * Los datos de pose (la posición del avión) están referidos al CENTRO DE
//     GRAVEDAD, que aquí NO coincide con el origen del modelo (la nariz).
//   * Para rotar alrededor del CDG, update() multiplica por T(-ref) del lado
//     derecho de la composición (primero se corre el modelo para llevar el CDG
//     al origen, después se rota ahí, después se traslada): M_pose = T(pos)·R·T(-ref).
//   * Si la traslación T(-ref) se pusiera del otro lado (T(pos)·T(-ref)·R) el
//     avión rotaría alrededor de la nariz y además se "escaparía" de su lugar:
//     la corrección del punto de referencia y la rotación no se aplicarían en
//     el mismo sistema. Ver README.
//
// NOTA SOBRE LAS NORMALES Y EL ESCALADO:
//   * Las placas (alas, empenajes) se generan con cube(escala): el escalado NO
//     uniforme queda DENTRO del generador, por lo que las normales que produce
//     ya son perpendiculares a la forma final. En la matriz local NO se escala
//     ninguna pieza.
//   * Nunca se usa un escalado NEGATIVO (glm::scale con un -1): invierte el
//     ordenamiento de los vértices (winding) y voltea las caras. Las piezas
//     simétricas se duplican por TRASLACIÓN a la posición de enfrente.

#include "Aircraft.h"

#include <glm/gtc/matrix_transform.hpp>   // glm::translate / rotate / radians
#include <cmath>                          // std::tan (para deducir la altura de la nariz)
#include <iostream>                       // std::cout (reporte de armado)
#include "Primitives.h"                   // primitives::cube / cylinder / cone

// -----------------------------------------------------------------------------
// Configuración del despiece (todo relativo al largo del fuselaje L = 1).
// Valores en un namespace ANÓNIMO: solo se ven dentro de este archivo.
// -----------------------------------------------------------------------------
namespace {

// Ejes del sistema de referencia del modelo (ver comentario de arriba).
const glm::vec3 kEjeX(1.0f, 0.0f, 0.0f);  // lateral (alas) -> eje de CABECEO
const glm::vec3 kEjeY(0.0f, 1.0f, 0.0f);  // vertical      -> eje de GUIÑADA
const glm::vec3 kEjeZ(0.0f, 0.0f, 1.0f);  // nariz->cola   -> eje de ROLIDO

// --- Fuselaje (cilindro acostado) -----------------------------------------
constexpr float kRadioFuselaje = 0.08f;   // radio del "tubo" (muy esbelto)
constexpr unsigned int kGajos = 24U;      // discretización polar (redondez)
constexpr float kZFuselaje = 0.5f;        // centro del tubo en Z (nariz en z=0)

// --- Nariz (cono) -----------------------------------------------------------
constexpr float kAnguloNariz = 40.0f;     // ángulo de conicidad α (grados):
                                          // define cuánto mide la nariz hacia
                                          // adelante (H = radio / tan(α/2)).

// --- Alas (una placa reusada en las dos alas) ------------------------------
constexpr float kEnvergaduraAla = 1.0f;   // ancho total de la placa en X
                                          // (media envergadura = 0.5 por lado)
constexpr float kGrosorAla    = 0.02f;    // espesor de la placa en Y
constexpr float kCuerdaAla    = 0.30f;    // largo nariz-cola de la placa en Z
constexpr float kXAla         = 0.60f;    // centro X (la raíz queda en x=0.1,
                                          // apenas fuera del tubo del fuselaje)
constexpr float kZAla         = 0.50f;    // centro Z (sobre el medio del avión)

// --- Empenaje HORIZONTAL (cola, reusado en dos mitades) ---------------------
constexpr float kEnvergaduraHor = 0.44f;  // ancho total de la placa en X
constexpr float kGrosorHor     = 0.015f;  // espesor
constexpr float kCuerdaHor     = 0.12f;   // cuerda en Z
constexpr float kXHor          = 0.27f;   // centro X (la raíz en x=0.05)
constexpr float kZHor          = 0.96f;   // contra la cola del tubo

// --- Empenaje VERTICAL (timón) ----------------------------------------------
constexpr float kGrosorVer = 0.015f;      // espesor de la placa en X
constexpr float kAltoVer   = 0.22f;      // alto de la placa en Y
constexpr float kCuerdaVer = 0.15f;      // cuerda en Z
constexpr float kYVer      = 0.11f;      // centro Y (el borde inferior en y=0)
constexpr float kZVer      = 0.96f;      // contra la cola

// --- Punto de referencia de la pose -----------------------------------------
// El CENTRO DE GRAVEDAD del avión, en coordenadas del modelo. Un valor
// razonable: un poco detrás del encastre de las alas (que están en z=0.5),
// sobre la línea central y a la altura del eje del fuselaje.
const glm::vec3 kPuntoReferencia(0.0f, 0.0f, 0.40f);

// --- Materiales por sección ---------------------------------------------------
// PRÁCTICO 9: el color (vec4) pasó a ser un MATERIAL (kd/ks/ns). kd es el color
// difuso de antes; ks/ns el brillo especular (0 para superficies mate). Las
// piezas metálicas (fuselaje, nariz) tienen un brillo suave; las placas blancas
// (alas, empenajes) quedan mate (ks = 0).
const Material kMaterialFuselaje{ glm::vec3(0.78f, 0.80f, 0.82f), glm::vec3(0.4f), 32.0f };  // aluminio
const Material kMaterialNariz   { glm::vec3(0.82f, 0.20f, 0.16f), glm::vec3(0.3f), 16.0f };  // rojo
const Material kMaterialAla     { glm::vec3(0.88f, 0.89f, 0.90f), glm::vec3(0.0f), 1.0f  };  // blanco
const Material kMaterialHor     { glm::vec3(0.15f, 0.35f, 0.70f), glm::vec3(0.0f), 1.0f  };  // azul
const Material kMaterialVer     { glm::vec3(0.88f, 0.89f, 0.90f), glm::vec3(0.0f), 1.0f  };  // blanco

} // fin del namespace anónimo (nada de esto se exporta)

// -----------------------------------------------------------------------------
// init: arma las piezas y las matrices locales, una sola vez.
// -----------------------------------------------------------------------------
void Aircraft::init() {
    // init() se puede llamar dos veces: limpio lo previo y rearmo. Los Mesh se
    // destruyen al romperse el vector, y sus destructores (RAII) liberan la GPU.
    piezas_.clear();
    locales_.clear();
    materiales_.clear();

    // --- FUSELAJE (cilindro acostado) ----------------------------------------
    // El cilindro del generador nace con el eje en Y. Para acostarlo hay que
    // girarlo -90° alrededor de X; R(-90°, X) manda el eje +Y del cilindro al
    // eje -Z del modelo (la presentación usa exactamente este giro). Como es un
    // cilindro SIN tapas, el sentido no se nota. El tubo queda centrado en
    // z=0.5: la nariz del tubo en z=0 (coincide con el origen del modelo) y la
    // cola en z=1.
    glm::mat4 local = glm::mat4(1.0f);                       // arranco en identidad
    local = glm::translate(local, glm::vec3(0.0f, 0.0f, kZFuselaje));
    local = glm::rotate(local, glm::radians(-90.0f), kEjeX);
    agregar_pieza(primitives::cylinder(kRadioFuselaje, 1.0f, kGajos),
                  local, kMaterialFuselaje);

    // --- NARIZ (cono apuntando hacia delante) --------------------------------
    // La altura de la nariz la deduce el generador del semiángulo:
    // H = radio / tan(α/2). El cono nace con el ápice en +Y; R(-90°, X) lo
    // apunta hacia -Z (adelante). Traslado el cono -H/2 en Z para que su base
    // (el anillo) quede en z=0, encastrado en la nariz del fuselaje; el ápice
    // queda en z = -H, delante de todo.
    const float hNariz = kRadioFuselaje /
                         std::tan(glm::radians(kAnguloNariz) * 0.5f);
    local = glm::mat4(1.0f);
    local = glm::translate(local, glm::vec3(0.0f, 0.0f, -hNariz * 0.5f));
    local = glm::rotate(local, glm::radians(-90.0f), kEjeX);
    agregar_pieza(primitives::cone(kRadioFuselaje, kAnguloNariz, kGajos),
                  local, kMaterialNariz);

    // --- ALAS (una malla, reusada en las dos alas) ---------------------------
    // La malla se GENERA UNA VEZ; las dos alas son dos PIEZAS con la misma
    // geometría y matrices locales DISTINTAS (espejadas). El espejado se hace
    // por TRASLACIÓN al lado opuesto (kXAla con signo ±) y NO con escala
    // negativa, que invertiría el winding de los vértices.
    MeshData datos_ala = primitives::cube(kEnvergaduraAla, kGrosorAla, kCuerdaAla);

    local = glm::mat4(1.0f);
    local = glm::translate(local, glm::vec3(kXAla, 0.0f, kZAla));   // ala derecha
    agregar_pieza(MeshData(datos_ala), local, kMaterialAla);           // copia (1 de 2)

    local = glm::mat4(1.0f);
    local = glm::translate(local, glm::vec3(-kXAla, 0.0f, kZAla));  // ala izquierda
    agregar_pieza(MeshData(datos_ala), local, kMaterialAla);           // copia (2 de 2)

    // --- EMPENAJE HORIZONTAL (otra placa, reusada en dos mitades) -------------
    // Misma idea que las alas: un solo generador → dos piezas espejadas.
    MeshData datos_hor = primitives::cube(kEnvergaduraHor, kGrosorHor, kCuerdaHor);

    local = glm::mat4(1.0f);
    local = glm::translate(local, glm::vec3(kXHor, 0.0f, kZHor));   // mitad derecha
    agregar_pieza(MeshData(datos_hor), local, kMaterialHor);           // copia (1 de 2)

    local = glm::mat4(1.0f);
    local = glm::translate(local, glm::vec3(-kXHor, 0.0f, kZHor));  // mitad izquierda
    agregar_pieza(MeshData(datos_hor), local, kMaterialHor);           // copia (2 de 2)

    // --- EMPENAJE VERTICAL (timón, una sola pieza) ----------------------------
    // Placa parada sobre la cola: el alto va en Y. Se traslada a la mitad del
    // alto (kYVer) para que su borde inferior quede apoyado en y=0, sobre el tubo.
    local = glm::mat4(1.0f);
    local = glm::translate(local, glm::vec3(0.0f, kYVer, kZVer));
    agregar_pieza(primitives::cube(kGrosorVer, kAltoVer, kCuerdaVer),
                  local, kMaterialVer);

    // Reporte de verificación: mallas DISTINTAS generadas (5) vs piezas (7).
    // La diferencia son las mallas REUSADAS (alas y empenaje horizontal).
    std::cout << "[Aircraft] modelo armado: " << piezas_.size()
              << " piezas a partir de 5 mallas distintas\n";
}

// -----------------------------------------------------------------------------
// update: rearma la pose, cada cuadro. No toca piezas ni locales.
// -----------------------------------------------------------------------------
void Aircraft::update(glm::vec3 pos, glm::vec3 angulos) {
    // M_pose = T(pos) · R(cabeceo)·R(guiñada)·R(rolido) · T(-ref)
    // Con glm se escribe al REVÉS (la última llamada se aplica primero):
    //   1) T(-ref): llevo el punto de referencia (CDG) al origen,
    //   2) roto alrededor del origen (los ángulos, en ese orden),
    //   3) T(pos): dejo el avión en su lugar del mundo.
    // Nota: los ángulos son absolutos por cuadro (no se acumulan).
    pose_ = glm::mat4(1.0f);
    pose_ = glm::translate(pose_, pos);                 // T(pos)
    pose_ = glm::rotate(pose_, angulos.x, kEjeX);       // CABECEO: nariz arriba/abajo
    pose_ = glm::rotate(pose_, angulos.y, kEjeY);       // GUIÑADA: morro a izquierda/derecha
    pose_ = glm::rotate(pose_, angulos.z, kEjeZ);       // ROLIDO: inclinar las alas
    pose_ = glm::translate(pose_, -kPuntoReferencia);   // T(-ref): CDG -> origen
}

// -----------------------------------------------------------------------------
// set_pose: rearma la pose desde una ORIENTACIÓN ya compuesta (Práctico 6).
// M_pose = T(pos) · orientacion · T(-ref): igual que update(), pero la rotación
// llega como matriz (calculada afuera con el orden correcto del FDM).
// -----------------------------------------------------------------------------
void Aircraft::set_pose(glm::vec3 pos, const glm::mat4& orientacion,
                        float escala_modelo) {
    // T(pos): el avión a su lugar del mundo.
    pose_ = glm::translate(glm::mat4(1.0f), pos);
    // La orientación del FDM (matriz modelo -> mundo ya compuesta).
    pose_ = pose_ * orientacion;
    // El modelo está dibujado en unidades normalizadas; esta escala lo lleva a metros.
    pose_ = glm::scale(pose_, glm::vec3(escala_modelo));
    // T(-ref): el CDG al origen, para que la rotación haya sido alrededor del
    // punto de referencia y no del origen del modelo (la nariz).
    pose_ = pose_ * glm::translate(glm::mat4(1.0f), -kPuntoReferencia);
}

// -----------------------------------------------------------------------------
// collect: arma la lista de RenderItem con la transformación ya compuesta.
// -----------------------------------------------------------------------------
void Aircraft::collect(std::vector<RenderItem>& items) const {
    // Arranco con una lista limpia para poder llamar a collect() varias veces.
    items.clear();

    // Un item por pieza; las tres listas tienen el mismo largo y mismo orden.
    const std::size_t n = piezas_.size();
    for (std::size_t i = 0; i < n; ++i) {
        RenderItem item;                  // el item de la pieza i
        item.mesh  = &(piezas_[i]);       // presto la malla (la posee Aircraft)
        item.model = pose_ * locales_[i]; // compongo: pose * local (en ese orden)
        item.material = materiales_[i];    // su material (kd/ks/ns)
        items.push_back(item);            // lo agrego a la lista
    }
}

// -----------------------------------------------------------------------------
// pieza_count: cuántas piezas hay cargadas.
// -----------------------------------------------------------------------------
std::size_t Aircraft::pieza_count() const {
    return piezas_.size();                // el largo de cualquiera de las 3 listas
}

// -----------------------------------------------------------------------------
// agregar_pieza: helper privado que agrega una pieza a las tres listas a la vez.
// Mantiene la INVARIANTE de que las listas quedan paralelas (mismo largo).
// -----------------------------------------------------------------------------
void Aircraft::agregar_pieza(MeshData&& datos, const glm::mat4& local, const Material& material) {
    piezas_.emplace_back();               // crea un Mesh VACÍO al final de la lista
    piezas_.back().load(datos);           // lo lleno: crea VAO/VBO/EBO y sube a GPU
    locales_.push_back(local);            // registro la matriz local de esta pieza
    materiales_.push_back(material);      // registro el material (mismo índice)
}