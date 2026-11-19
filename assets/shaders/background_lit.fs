// background_lit.fs — Fragment shader del fondo ILUMINADO (Práctico 9).
//
// SIN #version (se concatena "#version" + iluminacion.glsl + este archivo). Usa
// el MISMO background.vs del Práctico 8 (solo pasa la dirección). Combina:
//   * Por debajo del horizonte (d.y < 0): el SUELO CONTINUADO, iluminado con la
//     MISMA función (iluminar) y el MISMO material que la malla del terreno, para
//     que no se distinga el borde de la ventana de terreno. La normal de un plano
//     horizontal es (0,1,0) y la posición ya la calcula este shader (P).
//   * Por encima del horizonte: el CIELO del cubemap, sin iluminar (ya es la
//     imagen de un cielo iluminado), pero mezclado con la niebla cerca del
//     horizonte para que a elevación cero coincida con el color del suelo.

in vec3 vDireccion;

layout (binding = 0) uniform samplerCube uCielo;        // el cubemap del cielo
layout (binding = 1) uniform sampler2D   uTexturaSuelo; // la textura del terreno

uniform float uAlturaSuelo;                // altura del plano que continúa el terreno
uniform float uMetrosTesela;               // metros que cubre una tesela de la textura

out vec4 FragColor;

void main()
{
    vec3 d = normalize(vDireccion);

    if (d.y < 0.0) {
        // --- SUELO CONTINUADO ---
        // El rayo P(t) = uOjo + t·d contra el plano y = uAlturaSuelo.
        float t  = (uOjo.y - uAlturaSuelo) / max(-d.y, 1.0e-4);
        vec3  P  = uOjo + t * d;

        Material m;
        m.kd = a_lineal(texture(uTexturaSuelo, P.xz / uMetrosTesela).rgb);
        m.ks = vec3(0.0);                  // mate, como el pasto
        m.ns = 1.0;

        vec3 color = iluminar(P, vec3(0.0, 1.0, 0.0), m); // normal de plano horizontal
        color = aplicar_niebla(color, P);  // hacia el horizonte, t crece y f -> 1

        FragColor = vec4(uCorreccion ? a_srgb(color) : color, 1.0);
    } else {
        // --- CIELO ---
        vec3 cielo = a_lineal(texture(uCielo, vDireccion).rgb);  // el cubemap está en sRGB

        // La niebla del cielo solo en los primeros grados de elevación, para que
        // a elevación cero (d.y = 0) cielo y suelo tengan el mismo color. Con
        // smoothstep(0, 0.10, d.y) la transición cubre ~5.7°.
        float f = 1.0 - smoothstep(0.0, 0.10, d.y);
        vec3 color = mix(cielo, uNieblaColor, f);

        FragColor = vec4(uCorreccion ? a_srgb(color) : color, 1.0);
    }
}
