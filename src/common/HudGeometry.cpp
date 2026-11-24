// HudGeometry.cpp — la GEOMETRIA del HUD, CPU puro (sin OpenGL).
//
// Aca vive todo lo que NO toca la GPU:
//   * la fuente bitmap 5x7 (tabla de glifos) y sus metricas,
//   * el formateo de numeros a texto,
//   * build_text (texto -> lista de glifos) y
//   * build_hud (estado de vuelo -> lineas + glifos).
//
// Separar esto de la clase Hud (que SI toca OpenGL) permite probar el HUD en
// tests/ sin crear contexto ni ventana. Ver Hud.h, seccion "datos / GPU".

#include "Hud.h"

#include <algorithm>   // std::min / std::max
#include <cmath>       // std::cos / std::sin / std::round / std::abs
#include <cstdio>      // std::snprintf (formato de numeros)

// -----------------------------------------------------------------------------
// Fuente bitmap 5x7. Cada glifo son 7 filas x 5 columnas; '#' = pixel encendido,
// '.' = apagado. La fila 0 es la SUPERIOR. El conjunto cubre lo que usa el HUD:
// espacio, algunos simbolos, digitos y mayusculas.
// -----------------------------------------------------------------------------
namespace {

struct Glifo {
    char        caracter;
    const char* filas[7];
};

constexpr int kGlifoW = 5;   // ancho de un glifo (columnas)
constexpr int kGlifoH = 7;   // alto de un glifo (filas)
constexpr int kCeldaW = 6;   // ancho de celda en el atlas: 5 + 1 columna de separacion
constexpr int kAvance  = 6;  // avance monoespaciado (celdas) tras cada caracter

// Color del HUD (rojo). Se elige rojo en vez de verde para que contraste con el
// cielo (celeste) y con el terreno (verde/marron): lo usan build_hud (glifos) y
// Hud.cpp (uniform de lineas).
const glm::vec3 kColorHud(1.0f, 0.22f, 0.16f);

// La tabla define el ORDEN de los glifos dentro del atlas (una fila horizontal).
const Glifo kFuente[] = {
    // --- simbolos ---
    {' ', {".....",".....",".....",".....",".....",".....","....."}},
    {'-', {".....",".....",".....","#####",".....",".....","....."}},
    {'+', {".....","..#..","..#..","#####","..#..","..#..","....."}},
    {'.', {".....",".....",".....",".....",".....",".##..",".##.."}},
    {'/', {"....#","....#","...#.","..#..",".#...","#....","#...."}},
    // --- digitos ---
    {'0', {".###.","#...#","#...#","#...#","#...#","#...#",".###."}},
    {'1', {"..#..",".##..","..#..","..#..","..#..","..#..",".###."}},
    {'2', {".###.","#...#","....#","...#.","..#..",".#...","#####"}},
    {'3', {".###.","#...#","....#","..##.","....#","#...#",".###."}},
    {'4', {"...#.","..##.",".#.#.","#..#.","#####","...#.","...#."}},
    {'5', {"#####","#....","####.","....#","....#","#...#",".###."}},
    {'6', {"..##.",".#...","#....","####.","#...#","#...#",".###."}},
    {'7', {"#####","....#","...#.","..#..",".#...",".#...",".#..."}},
    {'8', {".###.","#...#","#...#",".###.","#...#","#...#",".###."}},
    {'9', {".###.","#...#","#...#",".####","....#","...#.",".##.."}},
    // --- mayusculas ---
    {'A', {".###.","#...#","#...#","#####","#...#","#...#","#...#"}},
    {'B', {"####.","#...#","#...#","####.","#...#","#...#","####."}},
    {'C', {".###.","#...#","#....","#....","#....","#...#",".###."}},
    {'D', {"####.","#...#","#...#","#...#","#...#","#...#","####."}},
    {'E', {"#####","#....","#....","####.","#....","#....","#####"}},
    {'F', {"#####","#....","#....","####.","#....","#....","#...."}},
    {'G', {".###.","#...#","#....","#.###","#...#","#...#",".####"}},
    {'H', {"#...#","#...#","#...#","#####","#...#","#...#","#...#"}},
    {'I', {".###.","..#..","..#..","..#..","..#..","..#..",".###."}},
    {'J', {"..###","...#.","...#.","...#.","...#.","#..#.",".##.."}},
    {'K', {"#...#","#..#.","#.#..","##...","#.#..","#..#.","#...#"}},
    {'L', {"#....","#....","#....","#....","#....","#....","#####"}},
    {'M', {"#...#","##.##","#.#.#","#.#.#","#...#","#...#","#...#"}},
    {'N', {"#...#","##..#","#.#.#","#..##","#...#","#...#","#...#"}},
    {'O', {".###.","#...#","#...#","#...#","#...#","#...#",".###."}},
    {'P', {"####.","#...#","#...#","####.","#....","#....","#...."}},
    {'Q', {".###.","#...#","#...#","#...#","#.#.#","#..#.",".##.#"}},
    {'R', {"####.","#...#","#...#","####.","#.#..","#..#.","#...#"}},
    {'S', {".###.","#...#","#....",".###.","....#","#...#",".###."}},
    {'T', {"#####","..#..","..#..","..#..","..#..","..#..","..#.."}},
    {'U', {"#...#","#...#","#...#","#...#","#...#","#...#",".###."}},
    {'V', {"#...#","#...#","#...#","#...#","#...#",".#.#.","..#.."}},
    {'W', {"#...#","#...#","#...#","#.#.#","#.#.#","##.##","#...#"}},
    {'X', {"#...#","#...#",".#.#.","..#..",".#.#.","#...#","#...#"}},
    {'Y', {"#...#","#...#",".#.#.","..#..","..#..","..#..","..#.."}},
    {'Z', {"#####","....#","...#.","..#..",".#...","#....","#####"}},
};

constexpr int kCantidad = static_cast<int>(sizeof(kFuente) / sizeof(kFuente[0]));

// indice: posicion del caracter en la tabla, o -1 si no esta en la fuente.
int indice(char c) {
    for (int i = 0; i < kCantidad; ++i) {
        if (kFuente[i].caracter == c) {
            return i;
        }
    }
    return -1;
}

} // namespace

namespace hud {

// -----------------------------------------------------------------------------
// color_hud: el color del HUD (lo comparten geometria y shader de lineas).
// -----------------------------------------------------------------------------
glm::vec3 color_hud() { return kColorHud; }

// -----------------------------------------------------------------------------
// Metrica de la fuente (ver Hud.h).
// -----------------------------------------------------------------------------
bool glyph_uv(char c, float& u0, float& v0, float& du, float& dv) {
    const int i = indice(c);
    if (i < 0) {
        return false;                     // caracter fuera de la fuente
    }
    const float ancho = static_cast<float>(atlas_width());
    const float alto  = static_cast<float>(atlas_height());
    // El glifo ocupa las columnas [i*kCeldaW, i*kCeldaW+kGlifoW) y TODO el alto.
    u0 = static_cast<float>(i * kCeldaW) / ancho;
    du = static_cast<float>(kGlifoW) / ancho;
    v0 = 0.0f;
    dv = static_cast<float>(kGlifoH) / alto;
    return true;
}

float glyph_advance(char c) {
    return (indice(c) >= 0) ? static_cast<float>(kAvance) : 0.0f;
}

int atlas_width()  { return kCantidad * kCeldaW; }
int atlas_height() { return kGlifoH; }

// -----------------------------------------------------------------------------
// build_atlas: rasteriza los glifos en una imagen RGBA (ver Hud.h).
// La fila 0 del buffer es la fila INFERIOR de la textura (v=0 en OpenGL); por
// eso se escribe el glifo de ABAJO hacia ARRIBA (fila de glifo = kGlifoH-1-r).
// -----------------------------------------------------------------------------
ImageData build_atlas() {
    const int w = atlas_width();
    const int h = atlas_height();

    ImageData img;
    img.width    = w;
    img.height   = h;
    img.channels = 4;
    img.pixels.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4U, 0);

    for (int i = 0; i < kCantidad; ++i) {
        for (int r = 0; r < kGlifoH; ++r) {
            const int fila_glifo = kGlifoH - 1 - r;   // abajo -> arriba
            const char* fila = kFuente[i].filas[fila_glifo];
            for (int c = 0; c < kGlifoW; ++c) {
                if (fila[c] != '#') {
                    continue;
                }
                const int px = i * kCeldaW + c;
                const int py = r;
                const std::size_t idx =
                    (static_cast<std::size_t>(py) * static_cast<std::size_t>(w) +
                     static_cast<std::size_t>(px)) * 4U;
                // La "tinta" es blanca; la forma queda en el canal alfa.
                img.pixels[idx + 0U] = 255;
                img.pixels[idx + 1U] = 255;
                img.pixels[idx + 2U] = 255;
                img.pixels[idx + 3U] = 255;
            }
        }
    }
    return img;
}

// -----------------------------------------------------------------------------
// format_number: numero -> texto con decimales fijos y '.' como separador.
// -----------------------------------------------------------------------------
std::string format_number(float valor, int decimales) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer),
                  decimales == 0 ? "%.0f" : "%.1f", static_cast<double>(valor));
    return std::string(buffer);
}

// -----------------------------------------------------------------------------
// text_width: ancho en pixeles (monoespaciado: todos los caracteres avanzan igual).
// -----------------------------------------------------------------------------
float text_width(const std::string& texto, float escala) {
    float ancho = 0.0f;
    for (char c : texto) {
        ancho += glyph_advance(c) * escala;
    }
    return ancho;
}

// -----------------------------------------------------------------------------
// build_text: texto -> lista de glifos (ver Hud.h).
// -----------------------------------------------------------------------------
void build_text(const std::string& texto, glm::vec2 origen, float escala,
                glm::vec3 color, std::vector<Glyph>& out) {
    float cursor = origen.x;
    for (char c : texto) {
        float u0, v0, du, dv;
        if (glyph_uv(c, u0, v0, du, dv)) {
            Glyph g;
            g.pos     = glm::vec2(cursor, origen.y);
            g.size    = glm::vec2(static_cast<float>(kGlifoW) * escala,
                                  static_cast<float>(kGlifoH) * escala);
            g.uv      = glm::vec2(u0, v0);
            g.uv_size = glm::vec2(du, dv);
            g.color   = color;
            g.alpha   = 1.0f;
            out.push_back(g);
        }
        cursor += glyph_advance(c) * escala;
    }
}

// -----------------------------------------------------------------------------
// Utilidades de geometria (anonimas).
// -----------------------------------------------------------------------------
namespace {

constexpr float kPi = 3.14159265358979323846f;

// rot: rota el punto `p` un angulo `ang` (rad) alrededor del centro `c`.
glm::vec2 rot(glm::vec2 p, glm::vec2 c, float ang) {
    const float cs = std::cos(ang);
    const float sn = std::sin(ang);
    const float dx = p.x - c.x;
    const float dy = p.y - c.y;
    return glm::vec2(c.x + dx * cs - dy * sn, c.y + dx * sn + dy * cs);
}

// fade_borde: 1 en el centro de una cinta y baja a 0 hacia sus bordes (para
// desvanecer las marcas lejanas, como en la presentacion).
float fade_borde(float y, float centro, float mitad) {
    const float d = std::abs(y - centro) / std::max(mitad, 1.0f);  // 0..1
    const float suave = 0.75f;                                     // inicio del desvanecido
    if (d < suave) {
        return 1.0f;
    }
    return std::max(0.0f, (1.0f - d) / (1.0f - suave));
}

// agrega un segmento (con sus extremos ya rotados si hace falta).
void linea(glm::vec2 a, glm::vec2 b, float alpha, std::vector<hud::Line>& out) {
    out.push_back({a, b, alpha});
}

} // namespace

// -----------------------------------------------------------------------------
// build_hud: arma todos los instrumentos (ver Hud.h).
// -----------------------------------------------------------------------------
void build_hud(const HudState& s, int width, int height,
               std::vector<Line>& lines, std::vector<Glyph>& glyphs) {
    lines.clear();
    glyphs.clear();

    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    const float cx = w * 0.5f;   // centro horizontal del indicador de actitud
    const float cy = h * 0.5f;   // centro vertical del indicador de actitud

    // --- Indicador de ACTITUD (escalera de cabeceo + arco de alabeo) ----------
    // pixeles por grado: la escalera se mueve con el cabeceo.
    const float ppd = std::max(4.0f, h * 0.009f);
    const float roll = glm::radians(s.roll_deg);

    // Escalera de cabeceo: barras cada 5 grados, de -30 a +30. La barra del
    // angulo `a` queda en y = cy + (a - pitch)*ppd: cuando el morro sube (pitch
    // crece) la escalera BAJA en pantalla. Todo rota con el alabeo (-roll).
    for (int a = -30; a <= 30; a += 5) {
        const float y = cy + (static_cast<float>(a) - s.pitch_deg) * ppd;
        if (y < -h || y > 2.0f * h) {
            continue;                        // fuera de la ventana: no se dibuja
        }
        float mitad = 26.0f;                 // largo de la barra (cada lado)
        if (a % 10 == 0) mitad = 44.0f;      // lineas "redondas" mas largas
        if (a == 0)      mitad = 60.0f;      // el horizonte, la mas larga
        const float hueco = 24.0f;           // espacio central para el simbolo

        glm::vec2 l0(cx - hueco - mitad, y);
        glm::vec2 l1(cx - hueco, y);
        glm::vec2 r0(cx + hueco, y);
        glm::vec2 r1(cx + hueco + mitad, y);
        const glm::vec2 c(cx, cy);
        linea(rot(l0, c, -roll), rot(l1, c, -roll), 1.0f, lines);
        linea(rot(r0, c, -roll), rot(r1, c, -roll), 1.0f, lines);
    }

    // Simbolo de referencia FIJO (no rota): un circulo central + dos alas.
    const int seg = 20;
    const float r = 7.0f;
    glm::vec2 prev(cx + r, cy);
    for (int i = 1; i <= seg; ++i) {
        const float t = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(seg);
        const glm::vec2 p(cx + r * std::cos(t), cy + r * std::sin(t));
        linea(prev, p, 1.0f, lines);
        prev = p;
    }
    linea(glm::vec2(cx - 60.0f, cy), glm::vec2(cx - 18.0f, cy), 1.0f, lines);
    linea(glm::vec2(cx + 18.0f, cy), glm::vec2(cx + 60.0f, cy), 1.0f, lines);

    // Arco de alabeo (escala de banqueo): arco de -60 a +60 grados alrededor
    // del centro; rota con -roll (va "pegado" a la aeronave). Un puntero fijo
    // arriba marca el banqueo actual.
    const float radio = std::min(w, h) * 0.30f;
    const int n = 40;
    for (int i = 0; i < n; ++i) {
        const float a0 = glm::radians(-60.0f + 120.0f * static_cast<float>(i) / n);
        const float a1 = glm::radians(-60.0f + 120.0f * static_cast<float>(i + 1) / n);
        glm::vec2 p0(cx + radio * std::sin(a0), cy + radio * std::cos(a0));
        glm::vec2 p1(cx + radio * std::sin(a1), cy + radio * std::cos(a1));
        const glm::vec2 c(cx, cy);
        linea(rot(p0, c, -roll), rot(p1, c, -roll), 0.85f, lines);
    }
    // Marcas del arco: -60, -45, -30, -15, 0, 15, 30, 45, 60.
    const int marcas[] = {-60, -45, -30, -15, 0, 15, 30, 45, 60};
    for (int a : marcas) {
        const float ang = glm::radians(static_cast<float>(a));
        const float largo = (a == 0 || std::abs(a) == 60) ? 14.0f : 9.0f;
        glm::vec2 p0(cx + radio * std::sin(ang), cy + radio * std::cos(ang));
        glm::vec2 p1(cx + (radio - largo) * std::sin(ang),
                     cy + (radio - largo) * std::cos(ang));
        const glm::vec2 c(cx, cy);
        linea(rot(p0, c, -roll), rot(p1, c, -roll), 1.0f, lines);
    }
    // Puntero fijo arriba (triangulo que apunta al arco).
    const glm::vec2 tip(cx, cy + radio);
    linea(tip, glm::vec2(cx - 8.0f, cy + radio - 12.0f), 1.0f, lines);
    linea(tip, glm::vec2(cx + 8.0f, cy + radio - 12.0f), 1.0f, lines);
    linea(glm::vec2(cx - 8.0f, cy + radio - 12.0f),
          glm::vec2(cx + 8.0f, cy + radio - 12.0f), 1.0f, lines);

    // --- Cinta de VELOCIDAD (izquierda, pegada al borde) -----------------------
    const float tape_x = w * 0.08f;
    const float top    = h * 0.82f;
    const float bottom = h * 0.18f;
    const float px_vel = (top - bottom) / 150.0f;    // 150 m/s visibles
    const float v      = s.speed_ms;
    const int paso     = 5;                          // marca menor cada 5 m/s
    const int mayor    = 25;                         // marca mayor cada 25 m/s

    for (int t = -150; t <= 150; t += paso) {
        const float val = std::round(v / paso) * paso + static_cast<float>(t);
        const float y = cy + (val - v) * px_vel;
        if (y < bottom || y > top) {
            continue;
        }
        const bool esMayor = (static_cast<int>(std::lround(val)) % mayor) == 0;
        const float largo = esMayor ? 26.0f : 14.0f;
        linea(glm::vec2(tape_x, y), glm::vec2(tape_x + largo, y),
              fade_borde(y, cy, (top - bottom) * 0.5f), lines);
    }
    // Puntero fijo (triangulo) hacia la cinta.
    linea(glm::vec2(tape_x, cy), glm::vec2(tape_x + 16.0f, cy + 8.0f), 1.0f, lines);
    linea(glm::vec2(tape_x, cy), glm::vec2(tape_x + 16.0f, cy - 8.0f), 1.0f, lines);
    linea(glm::vec2(tape_x + 16.0f, cy + 8.0f), glm::vec2(tape_x + 16.0f, cy - 8.0f), 1.0f, lines);

    // Lectura numerica de velocidad (a la derecha de la cinta).
    build_text(format_number(v, 0), glm::vec2(tape_x + 24.0f, cy - 10.0f),
               3.0f, kColorHud, glyphs);
    build_text("SPD", glm::vec2(tape_x + 24.0f, cy - 34.0f), 2.0f, kColorHud, glyphs);

    // --- Cinta de ALTITUD (derecha, pegada al borde) ---------------------------
    const float alt_x = w * 0.92f;
    const float px_alt = (top - bottom) / 800.0f;    // 800 m visibles
    const float alt    = s.altitude_m;
    const int paso_alt  = 20;                        // marca menor cada 20 m
    const int mayor_alt = 100;                       // marca mayor cada 100 m

    for (int t = -400; t <= 400; t += paso_alt) {
        const float val = std::round(alt / paso_alt) * paso_alt + static_cast<float>(t);
        const float y = cy + (val - alt) * px_alt;
        if (y < bottom || y > top) {
            continue;
        }
        const bool esMayor = (static_cast<int>(std::lround(val)) % mayor_alt) == 0;
        const float largo = esMayor ? 26.0f : 14.0f;
        linea(glm::vec2(alt_x, y), glm::vec2(alt_x - largo, y),
              fade_borde(y, cy, (top - bottom) * 0.5f), lines);
    }
    // Puntero fijo (triangulo) hacia la cinta.
    linea(glm::vec2(alt_x, cy), glm::vec2(alt_x - 16.0f, cy + 8.0f), 1.0f, lines);
    linea(glm::vec2(alt_x, cy), glm::vec2(alt_x - 16.0f, cy - 8.0f), 1.0f, lines);
    linea(glm::vec2(alt_x - 16.0f, cy + 8.0f), glm::vec2(alt_x - 16.0f, cy - 8.0f), 1.0f, lines);

    // Lectura numerica de altitud (a la izquierda de la cinta, alineada a la
    // derecha). Velocidad vertical debajo.
    const std::string texto_alt = format_number(alt, 0);
    const float ancho_alt = text_width(texto_alt, 3.0f);
    build_text(texto_alt, glm::vec2(alt_x - 24.0f - ancho_alt, cy - 10.0f),
               3.0f, kColorHud, glyphs);
    build_text("ALT", glm::vec2(alt_x - 24.0f - text_width("ALT", 2.0f), cy - 34.0f),
               2.0f, kColorHud, glyphs);

    const std::string texto_vs =
        (s.vspeed_ms >= 0.0f ? "+" : "") + format_number(s.vspeed_ms, 1);
    build_text("VS " + texto_vs,
               glm::vec2(alt_x - 24.0f - text_width("VS " + texto_vs, 2.0f),
                         cy - 54.0f),
               2.0f, kColorHud, glyphs);

    // --- RUMBO (texto centrado arriba) -----------------------------------------
    const std::string texto_hdg = "HDG " + format_number(
        std::fmod(s.heading_deg + 360.0f, 360.0f), 0);
    const float ancho_hdg = text_width(texto_hdg, 2.0f);
    build_text(texto_hdg, glm::vec2(cx - ancho_hdg * 0.5f, h - 34.0f),
               2.0f, kColorHud, glyphs);

    // --- AYUDA DE CONTROLES (esquina superior izquierda) ----------------------
    // Recordatorio fijo de los mandos, en la misma fuente que el resto del HUD.
    // Solo usa mayusculas, digitos y '/', que son los caracteres de la fuente.
    const float help_x       = 14.0f;
    const float help_escala  = 2.0f;
    const float help_espacio = 20.0f;   // separacion vertical entre lineas
    const char* ayudas[] = {
        "W/S CABECEO   A/D ALABEO",
        "FLECHAS TIMON Y POTENCIA",
        "ESPACIO PAUSA   C VISTA",
        "F ALAMBRE   ESC SALIR",
    };
    float help_y = h - 28.0f;
    for (const char* texto : ayudas) {
        build_text(texto, glm::vec2(help_x, help_y), help_escala, kColorHud, glyphs);
        help_y -= help_espacio;
    }
}

} // namespace hud
