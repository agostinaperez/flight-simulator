// iluminacion.glsl — código COMÚN a todos los fragment shaders de la escena
// (Práctico 9: iluminación + niebla + corrección gamma).
//
// SIN #version: esa línea la agrega el código que arma el programa, porque tiene
// que ser la PRIMERA línea del texto. Este archivo se concatena (vía
// glShaderSource con varias cadenas) entre "#version" y el código propio de cada
// fragment shader. Así la iluminación y la niebla existen UNA sola vez en el
// proyecto y todos los shaders (terreno, avión, fondo) usan la MISMA función:
// si alguno usara otra fórmula u otros valores, se notaría como una costura en
// el borde de la ventana de terreno.

// -----------------------------------------------------------------------------
// Parámetros de la escena (uniforms). Cada PROGRAMA tiene sus propios uniforms
// aunque se llamen igual en otros shaders: los valores se envían a CADA programa
// por separado. Si un shader no usa una función que consume un uniform, el
// compilador lo elimina y su ubicación devuelve -1 (glProgramUniform* con -1 no
// hace nada y no informa error).
// -----------------------------------------------------------------------------
uniform vec3  uSolDir;         // dirección HACIA el sol, en mundo, normalizada
uniform vec3  uSolColor;       // color e intensidad del sol (lineal)
uniform vec3  uAmbiente;       // luz ambiente (lineal)
uniform vec3  uOjo;            // posición de la cámara, en mundo
uniform vec3  uNieblaColor;    // color del horizonte del cielo (LINEAL)
uniform float uVisibilidad;    // distancia a la que la niebla llega al 63%
uniform bool  uCorreccion;     // activar/desactivar la corrección gamma (tecla G)
uniform bool  uPintarNormales; // pintar la normal como color, para verificar (tecla N)

// -----------------------------------------------------------------------------
// Material — los coeficientes del modelo de Phong. Coincide con el struct
// Material del lado del CPU (Material.h), así se puede pasar por uniform como
// `uniform Material uMaterial;` y direccionar cada campo con "uMaterial.kd".
// -----------------------------------------------------------------------------
struct Material {
    vec3  kd;                  // coeficiente difuso (y ambiente: ka = kd)
    vec3  ks;                  // coeficiente especular
    float ns;                  // exponente especular (el "brillo")
};

// -----------------------------------------------------------------------------
// iluminar — modelo de Phong con UNA luz direccional (el sol). P y N en
// coordenadas de mundo, N normalizada. Devuelve ambiente + difusa + especular.
// -----------------------------------------------------------------------------
vec3 iluminar(vec3 P, vec3 N, Material m)
{
    vec3 L = uSolDir;                       // direccional: la misma en todo punto
    vec3 V = normalize(uOjo - P);           // hacia el ojo

    float NdotL = max(dot(N, L), 0.0);      // < 0 = la cara mira para otro lado

    vec3 ambiente  = uAmbiente * m.kd;
    vec3 difusa    = uSolColor * m.kd * NdotL;
    vec3 especular = vec3(0.0);
    if (NdotL > 0.0) {
        vec3 R = reflect(-L, N);            // -L: la luz INCIDENTE, hacia la superficie
        especular = uSolColor * m.ks * pow(max(dot(R, V), 0.0), m.ns);
    }
    return ambiente + difusa + especular;
}

// -----------------------------------------------------------------------------
// aplicar_niebla — niebla exponencial. Mezcla el color ya iluminado con el color
// de la niebla según la distancia al ojo. Se aplica DESPUÉS de la iluminación
// (la niebla es un color que no se ilumina). d = uVisibilidad => 63% de niebla.
// -----------------------------------------------------------------------------
vec3 aplicar_niebla(vec3 color, vec3 P)
{
    float d = length(P - uOjo);
    float f = 1.0 - exp(-d / uVisibilidad);
    return mix(color, uNieblaColor, f);
}

// -----------------------------------------------------------------------------
// Corrección gamma (Parte 5). La pantalla no es lineal: un valor v se muestra con
// intensidad ~v^2.2. Por eso los colores "elegidos a ojo" (texturas, cubemap,
// colores de pieza) vienen codificados en sRGB (curva inversa v^(1/2.2)). Las
// cuentas de iluminación (sumas y multiplicaciones de intensidades) solo valen en
// LINEAL: se convierte a lineal cada color que entra y, al final, el resultado se
// devuelve a sRGB antes de escribir. (La aproximación usa gamma = 2.2.)
// -----------------------------------------------------------------------------
const float GAMMA = 2.2;
vec3 a_lineal(vec3 c) { return pow(c, vec3(GAMMA)); }       // sRGB -> lineal
vec3 a_srgb(vec3 c)   { return pow(c, vec3(1.0 / GAMMA)); } // lineal -> sRGB
