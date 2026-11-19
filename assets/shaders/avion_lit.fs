// avion_lit.fs — Fragment shader de la aeronave ILUMINADA
// El material de cada pieza llega por uniform (struct Material
// uMaterial, cuyos campos coinciden con el struct Material del código común). El
// color que antes llevaba el RenderItem ahora es el coeficiente difuso (kd).
// Como kd se eligió "a ojo" en el Práctico 4 (en sRGB), se lo pasa a lineal antes
// de las cuentas; ks y ns son fracciones/brillo, no colores, y no se convierten.

in vec3 vPosMundo;
in vec3 vNormalMundo;

uniform Material uMaterial;                // el material de ESTA pieza (kd/ks/ns)

out vec4 FragColor;

void main()
{
    vec3 N = normalize(vNormalMundo);

    Material m;
    m.kd = a_lineal(uMaterial.kd);         // el color difuso está en sRGB -> lineal
    m.ks = uMaterial.ks;                   // fracción de luz: ya es lineal
    m.ns = uMaterial.ns;

    vec3 color = iluminar(vPosMundo, N, m);
    color = aplicar_niebla(color, vPosMundo);

    if (uPintarNormales) {
        // La normal como color (verificación): dato, no color -> sin gamma.
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
    } else {
        FragColor = vec4(uCorreccion ? a_srgb(color) : color, 1.0);
    }
}
