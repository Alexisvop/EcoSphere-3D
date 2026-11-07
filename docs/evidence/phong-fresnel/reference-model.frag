#version 330 core
in vec2 textureUV;
in vec3 worldPosition;
in vec3 worldNormal;
uniform vec4 baseColor;
uniform sampler2D baseTexture;
uniform bool textured;
uniform bool legacyUnlit, unlit, doubleSided;
uniform int alphaMode;
uniform float alphaCutoff, metallic, roughness, specular, reflection;
uniform vec3 cameraPosition;
uniform bool directEnabled, directionalEnabled, pointEnabled, reflectionEnabled;
uniform float ambient, directionalIntensity, pointIntensity, environmentIntensity, fresnelPower;
uniform vec3 directionalDirection, directionalColor, pointPosition, pointColor, attenuation;
uniform samplerCube environmentMap;
out vec4 fragmentColor;
vec3 linearToSrgb(vec3 value) {
    return mix(12.92 * value, 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), value));
}
vec3 safeNormalize(vec3 v) { return v / max(length(v), 0.00001); }
vec3 directLight(vec3 n, vec3 v, vec3 l, vec3 base, vec3 radiance) {
    float diffuse = max(dot(n, l), 0.0);
    vec3 h = safeNormalize(l + v);
    // Correspondencia explícita de rugosidad a exponente; no es BRDF PBR glTF.
    float exponent = 2.0 + 254.0 * pow(1.0 - roughness, 4.0);
    float highlight = diffuse > 0.0 ? pow(max(dot(n, h), 0.0), exponent) : 0.0;
    vec3 specularColor = mix(vec3(specular), base, metallic);
    return radiance * (base * (1.0 - metallic) * diffuse + specularColor * highlight);
}
void main() {
    vec4 color = baseColor;
    if (textured) color *= texture(baseTexture, textureUV);
    if (alphaMode == 1 && color.a < alphaCutoff) discard;
    float alpha = alphaMode == 2 ? color.a : 1.0;
    // Derivadas también fuera de ramas: fallback geométrico si el GLB no trae NORMAL.
    vec3 geometricNormal = safeNormalize(cross(dFdx(worldPosition), dFdy(worldPosition)));
    vec3 n = length(worldNormal) > 0.00001 ? safeNormalize(worldNormal) :
             (gl_FrontFacing ? geometricNormal : -geometricNormal);
    if (doubleSided && !gl_FrontFacing) n = -n;
    vec3 result = color.rgb;
    if (!unlit && !legacyUnlit) {
        vec3 v = safeNormalize(cameraPosition - worldPosition);
        result = vec3(0.0);
        if (directEnabled) {
            result = ambient * color.rgb * (1.0 - metallic);
            if (directionalEnabled)
                result += directLight(n, v, -directionalDirection, color.rgb, directionalColor * directionalIntensity);
            if (pointEnabled) {
                vec3 delta = pointPosition - worldPosition;
                float d = length(delta);
                float falloff = 1.0 / dot(attenuation, vec3(1.0, d, d*d));
                result += directLight(n, v, safeNormalize(delta), color.rgb, pointColor * pointIntensity * falloff);
            }
        }
        if (reflectionEnabled) {
            vec3 f0 = mix(vec3(0.04), color.rgb, metallic);
            vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - clamp(dot(n, v), 0.0, 1.0), fresnelPower);
            vec3 reflected = texture(environmentMap, reflect(-v, n)).rgb;
            vec3 weight = clamp(fresnel * reflection * environmentIntensity * (1.0 - 0.6 * roughness), 0.0, 1.0);
            result = mix(result, reflected, weight);
        }
    }
    // Regresión: codificación explícita previa. Demo: salida lineal y framebuffer sRGB para blending lineal.
    fragmentColor = vec4(legacyUnlit ? linearToSrgb(result) : result, alpha);
}
