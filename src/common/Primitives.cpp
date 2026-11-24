// Este archivo NO toca OpenGL: solo fabrica números (Vertex + índices) en la RAM.

#include "Primitives.h"                
#include "MeshData.h"                 
#include <glm/glm.hpp>                // glm::vec2/vec3 para rellenar los campos
#include <cmath>                      // cosf/sinf/tanf (seno, coseno, tangente)
#include <vector>                     // std::vector (vía MeshData.h, igual)

namespace primitives {
namespace {
constexpr double kPi = 3.14159265358979323846;   // π con 18 decimales, cte local del archivo q uso para pasar grados a radianes
}

MeshData cube(float scale_x, float scale_y, float scale_z) { 
    MeshData m;                        // la malla que voy a ir llenando

    // Media longitud en cada eje: el cubo va de -L/2 a +L/2 (centrado).
    // Usar hx/hy/hz hace que la misma fórmula sirva para escalas distintas.
    const float hx = scale_x * 0.5f;   // media longitud en X (ej. 0.5 para lado 1)
    const float hy = scale_y * 0.5f;   // media longitud en Y
    const float hz = scale_z * 0.5f;   // media longitud en Z

    
    // Tabla de las 6 caras. Cada fila define UNA cara con:
    //   normal : hacia dónde "mira" la cara (afuera del cubo).
    //   u, v   : dos direcciones TANGENTES al plano de la cara, con las que se arman las esquinas. Se eligen de forma que u × v = normal
    //   así, recorriendo las esquinas en el  orden que uso abajo, el sentido queda ANTIHORARIO visto desde afuera.    
    struct Cara {                       // un registro local por cara
        float nx, ny, nz;               //   la normal (afuera)
        float ux, uy, uz;               //   tangente "horizontal" de la cara
        float vx, vy, vz;               //   tangente "vertical" de la cara
    };                                  // fin del registro

    // 6 filas: 6 caras. Cada una con su normal y sus dos tangentes que cumplen
    // u×v = normal (verificado a mano).
    const Cara caras[6] = {
        // normal +X  (derecha),  u = -Z, v = +Y
        {  1, 0, 0,   0, 0,-1,   0, 1, 0 },   // derecho
        // normal -X  (izquierda), u = +Z, v = +Y
        { -1, 0, 0,   0, 0, 1,   0, 1, 0 },   // izquierdo
        // normal +Y  (arriba),   u = +X, v = -Z
        {  0, 1, 0,   1, 0, 0,   0, 0,-1 },   // arriba
        // normal -Y  (abajo),    u = +X, v = +Z
        {  0,-1, 0,   1, 0, 0,   0, 0, 1 },   // abajo
        // normal +Z  (frente),   u = +X, v = +Y
        {  0, 0, 1,   1, 0, 0,   0, 1, 0 },   // frente
        // normal -Z  (atrás),    u = -X, v = +Y
        {  0, 0,-1,  -1, 0, 0,   0, 1, 0 }    // atrás
    };                             

    
    // Por cada cara: agrego sus 4 vértices y sus 2 triángulos (6 índices).
    
    for (const Cara& cara : caras) {    
        // base = el primer número de vértice de ESTA cara.
        // Como cada cara agrega 4 vértices, el índice base va saltando
        // (0, 4, 8, 12, ...). Sirve para escribir los índices de esta cara.
        const unsigned int base = static_cast<unsigned int>(m.vertices.size());

        // El CENTRO de la cara: el cubo está centrado en el origen, entonces el
        // centro de una cara está sobre la normal, a media longitud del eje.
        const float cx = cara.nx * hx;  // x del centro (ej. +hx para la cara +X)
        const float cy = cara.ny * hy;  // y del centro (ej. 0 para las caras laterales)
        const float cz = cara.nz * hz;  // z del centro

        // Esquina = centro + (una mitad de u) + (una mitad de v), cada signo.
        // La multiplicación por hx/hy/hz según el eje hace que cada tangente
        // se estire solo a lo largo de su eje (escala independiente por eje).
        // Para NO escribir los 24 vértices a mano, lo armo con una lambda.
        auto esquina = [=](float su, float sv) -> Vertex {   // su,sv ∈ {-1,+1}
            Vertex v;                  // la esquina que voy a devolver
            // posición = centro + su*(tangente u) + sv*(tangente v), por eje.
            v.position.x = cx + su * (cara.ux * hx) + sv * (cara.vx * hx);
            v.position.y = cy + su * (cara.uy * hy) + sv * (cara.vy * hy);
            v.position.z = cz + su * (cara.uz * hz) + sv * (cara.vz * hz);
            // normal: la de la cara, const para los 4 vértices (la cara es plana).
            v.normal = glm::vec3(cara.nx, cara.ny, cara.nz);
            // textura: mapeo su/sv ∈ {-1,+1} a uv ∈ {0,1}: (-1, -1)->(0,0),
            // (+1,-1)->(1,0), (+1,+1)->(1,1), (-1,+1)->(0,1). Hoy no se usa.
            v.tex_coords = glm::vec2((su + 1.0f) * 0.5f,
                                     (sv + 1.0f) * 0.5f);
            return v;                  // la esquina lista
        };                             // fin de la lambda esquina

        // Las 4 esquinas de la cara, en orden ANTIHORARIO visto desde afuera:
        // p0 = centro -u -v, p1 = centro +u -v, p2 = centro +u +v, p3 = centro -u +v
        // (el cruce p0→p1→p2 con la regla de la mano derecha apunta a la normal).
        m.vertices.push_back(esquina(-1.0f, -1.0f));   // vértice base+0: "abajo-izquierda"
        m.vertices.push_back(esquina( 1.0f, -1.0f));   // vértice base+1: "abajo-derecha"
        m.vertices.push_back(esquina( 1.0f,  1.0f));   // vértice base+2: "arriba-derecha"
        m.vertices.push_back(esquina(-1.0f,  1.0f));   // vértice base+3: "arriba-izquierda"

        // Los 2 triángulos de la cara (cuadrilátero = 2 triángulos):
        //   triángulo 1: vértices (base+0, base+1, base+2)
        //   triángulo 2: vértices (base+0, base+2, base+3)
        // Ambos mantienen el sentido antihorario (la diagonal del cuadrilátero
        // va de base+0 a base+2; compartir la diagonal no cambia la orientación).
        m.indices.push_back(base + 0); // del triángulo 1
        m.indices.push_back(base + 1); //   primera esquina
        m.indices.push_back(base + 2); //   segunda esquina
        m.indices.push_back(base + 0); // del triángulo 2
        m.indices.push_back(base + 2); //   primera esquina
        m.indices.push_back(base + 3); //   segunda esquina

        // 6 índices agregados + 4 vértices agregados. La próxima cara arranca
        // en base = 4 de esta (porque `base` se recalcula con vertices.size()).
    }                                   // fin del loop sobre las 6 caras

    
    return m;                         
}


MeshData cylinder(float radio, float largo, unsigned int gajos) {
    // Guard "no romper en silencio": con gajos < 3 no se puede "cerrar la
    // vuelta" (un único gajo no forma un cilindro creíble). Aviso con
    // excepción en vez de devolver una malla deforme.
    if (gajos < 3U) {
        throw std::invalid_argument("cylinder: gajos debe ser >= 3");
    }
    // Un radio (o largo) no positivo tampoco define un cilindro.
    if (radio <= 0.0f || largo <= 0.0f) {
        throw std::invalid_argument("cylinder: radio y largo deben ser > 0");
    }

    MeshData m;                        // la malla que voy a ir llenando

    // Pasos del PDF:
    //   1) Definir el sistema de referencia: elección FIJADA:
    //        origen en el CENTRO del cilindro; eje Y = eje del cilindro;
    //        anillo inferior en y = -largo/2, superior en y = +largo/2;
    //        theta (el ángulo de la vuelta) se mide en el plano XZ desde +X.
    //   2) Discretizar los anillos: el anillo inferior y el superior, con
    //      (gajos+1) vértices cada uno.
    //   3) Definir los triángulos con índices: entre dos anillos, cada par de
    //      vértices vecinos forma un cuadrilátero, y cada cuadrilátero son dos
    //      triángulos.

    // Las alturas de los dos anillos (mitad para cada lado: centrado en 0).
    const float y0 = -largo * 0.5f;    // anillo inferior ("abajo")
    const float y1 =  largo * 0.5f;    // anillo superior ("arriba")

    // Los vértices del anillo de ABAJO ocupan las posiciones 0..gajos.
    // Los del anillo de ARRIBA ocupan gajos+1..2*(gajos+1)-1.
    // Por eso el "desfase" para indexar el anillo de arriba es (gajos+1).
    const unsigned int arriba = gajos + 1U;   // índice del primer vértice "arriba"

    // PASO 2: generar los vértices de los dos anillos.
    //   Uso i <= gajos y NO i < gajos (lección del PDF): en la COSTURA la
    //   posición y la normal del último vértice (theta = 2π) coinciden con las
    //   del primero (theta = 0), pero la coordenada de textura u vale 1 en vez
    //   de 0. Como un atributo difiere, el vértice se duplica: son gajos+1.
    for (unsigned int i = 0; i <= gajos; ++i) {
        // theta recorre los (gajos+1) puntos de la vuelta; con i = gajos queda
        // theta = 2π (vuelve a +X, misma posición y normal que con theta = 0).
        const float theta = static_cast<float>(2.0 * kPi * i / gajos);

        // u de textura: fracción de la vuelta que ya recorrí (0 a 1 inclusive).
        const float u = static_cast<float>(i) / static_cast<float>(gajos);

        // Los dos vértices de ESTE punto de la vuelta: uno abajo y uno arriba.
        //  * posición lateral del cilindro: (r·cos θ, r·sin θ) en el plano XZ.
        //  * normal RADIAL: (cos θ, 0, sin θ). Es la perpendicular a la
        //    superficie de UN cilindro de eje Y (siempre apunta "para afuera").
        //  * textura: u es la posición horizontal, v baja (0) / sube (1).
        for (unsigned int anillo = 0; anillo <= 1U; ++anillo) {
            Vertex v;                  // el vértice que voy a completar
            v.position = glm::vec3(radio * std::cos(theta),
                                   (anillo == 0U) ? y0 : y1,
                                   radio * std::sin(theta));
            v.normal = glm::vec3(std::cos(theta), 0.0f, std::sin(theta));
            v.tex_coords = glm::vec2(u, (anillo == 0U) ? 0.0f : 1.0f);
            m.vertices.push_back(v);   // lo agrego a la lista
        }                              // fin de "los dos vértices de este ángulo"

        // OJO: con `anillo` enumero abajo=0 y arriba=1, pero en `vertices` van
        // TODOS los de abajo primero y después todos los de arriba. Guardo en
        // qué posición quedarán con el índice calculado en el paso 3.
    }                                  // fin del loop de la vuelta

    // PASO 3: los triángulos (los índices).
    for (unsigned int i = 0; i < gajos; ++i) {
        // Los 4 vértices del "cuadrilátero" i (entre dos cortes del anillo):
        //   b0, b1 : del anillo de abajo (i e i+1).
        //   t0, t1 : del anillo de arriba (arriba+i y arriba+i+1).
        const unsigned int b0 = i;                 // abajo, corte i
        const unsigned int b1 = i + 1U;            // abajo, corte i+1
        const unsigned int t0 = arriba + i;        // arriba, corte i
        const unsigned int t1 = arriba + i + 1U;   // arriba, corte i+1

        // Cada cuadrilátero se parte en 2 triángulos. El ORDEN de cada terceto
        // define la orientación (normal) de la cara. El "natural" {b0,b1,t1}
        // (el que sugiere recorrer el bucle) deja la normal al REVÉS (lo dice
        // el PDF); este orden {b0, t1, b1} + {b0, t0, t1} es el que deja la
        // normal apuntando hacia afuera. Hoy no se usa (sin culling), pero
        // queda fijado para cuando se active.
        m.indices.insert(m.indices.end(), {b0, t1, b1});       // triángulo 1
        m.indices.insert(m.indices.end(), {b0, t0, t1});       // triángulo 2
    }                                  // fin de la tira de cuadriláteros

    return m;                          // vértices (2·(gajos+1)) + índices (6·gajos)
}


MeshData cone(float radio, float angulo_conicidad, unsigned int gajos) {
    // Guard "no romper en silencio": mismos criterios que el cilindro.
    if (gajos < 3U) {
        throw std::invalid_argument("cone: gajos debe ser >= 3");
    }
    // El ángulo de conicidad tiene que estar bien definido: 0 < α < 180°.
    // Con α = 0 el cono no tendría altura finita (H = radio/tan(0) = infinito);
    // con α >= 180 la tangente se vuelve negativa o infinita (absurdo).
    if (radio <= 0.0f || angulo_conicidad <= 0.0f || angulo_conicidad >= 180.0f) {
        throw std::invalid_argument("cone: radio > 0 y 0 < angulo_conicidad < 180");
    }

    MeshData m;                        // la malla que voy a ir llenando

    // Sistema de referencia (paso 1 del PDF, igual que el cilindro):
    // origen en el CENTRO del cono, eje Y hacia el ápice (arriba), el anillo
    // base abajo en -H/2 y el ápice arriba en +H/2, theta en el plano XZ.
    //
    // Altura del cono a partir del radio y del semiángulo:
    //   En el plano axial, tan(α/2) = radio / altura (triángulo rectángulo que
    //   forma el perfil con el eje). Entonces altura = radio / tan(α/2).
    const double alpha = angulo_conicidad * kPi / 180.0;   // α en radianes
    const float altura = static_cast<float>(radio / std::tan(alpha * 0.5));
    const float y_base = -altura * 0.5f;   // y del anillo de la base
    const float y_apex =  altura * 0.5f;   // y del ápice

    // Los cos/sin del semiángulo: aparecen en la normal del lateral.
    const float c_med = static_cast<float>(std::cos(alpha * 0.5));  // cos(α/2)
    const float s_med = static_cast<float>(std::sin(alpha * 0.5));  // sin(α/2)

    // PASO 2: el anillo de la base (como en el cilindro, gajos+1 vértices por
    // la costura de textura) y el ápice.
    for (unsigned int i = 0; i <= gajos; ++i) {
        const float theta = static_cast<float>(2.0 * kPi * i / gajos);
        const float u = static_cast<float>(i) / static_cast<float>(gajos);

        Vertex v;                      // vértice del anillo base
        v.position = glm::vec3(radio * std::cos(theta), y_base, radio * std::sin(theta));
        // Normal del LATERAL: NO es radial pura (como el cilindro). Está
        // inclinada hacia arriba por el semiángulo α/2, porque la superficie
        // del cono "sube". Fórmula del PDF:
        //   n = (cosθ·cos(α/2), sin(α/2), sinθ·cos(α/2))
        v.normal = glm::vec3(std::cos(theta) * c_med, s_med, std::sin(theta) * c_med);
        v.tex_coords = glm::vec2(u, 0.0f);   // en la base, v = 0
        m.vertices.push_back(v);       // agrego el vértice del lateral
    }

    // El ÁPICE: un vértice propio (duplicado, NO comparte con el lateral).
    // Criterio fijado en el PDF: la normal del ápice es AXIAL (0,1,0), mientras
    // que la del lateral es la inclinada. Un atributo (la normal) difiere, así
    // que el vértice se duplica — la regla de siempre.
    const unsigned int apex = static_cast<unsigned int>(m.vertices.size());
    {
        Vertex v;                      // el vértice del ápice
        v.position = glm::vec3(0.0f, y_apex, 0.0f);
        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v.tex_coords = glm::vec2(0.5f, 1.0f);   // arriba del todo, en el centro
        m.vertices.push_back(v);
    }

    // PASO 3: el abanico de triángulos {base del corte i, ápice, base i+1}.
    //   * El par (apex - b0) × (b1 - b0) da la dirección (altura, radio) que,
    //     normalizada, es exactamente (cos(α/2), sin(α/2)): coincide con la
    //     normal lateral del PDF. Deja la cara apuntando hacia AFUERA.
    //   * Queda fijado para cuando se active el culling (mismo criterio que el
    //     cilindro).
    for (unsigned int i = 0; i < gajos; ++i) {
        m.indices.insert(m.indices.end(),
                         {i, apex, i + 1U});     // triángulo {b0, apex, b1}
    }                                  // fin del abanico

    return m;                          // vértices ((gajos+1) + 1) + índices (3·gajos)
}

} // fin del namespace primitives