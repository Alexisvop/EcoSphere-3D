#version 330 core
in vec2 textureUV;
in vec3 worldPosition;
in vec3 worldNormal;

// material, cámara y luces (los manda ModelRenderer)
uniform vec4 baseColor;
uniform sampler2D baseTexture;
uniform bool textured;
uniform bool legacyUnlit;
uniform bool unlit;
uniform bool doubleSided;
uniform int alphaMode;
uniform float alphaCutoff;
uniform float metallic;
uniform float roughness;
uniform float specular;
uniform float reflection;

uniform vec3 cameraPosition;
uniform bool phongShading;
uniform bool fresnelEnabled;

uniform bool directEnabled;
uniform bool directionalEnabled;
uniform bool pointEnabled;
uniform bool reflectionEnabled;
uniform float ambient;
uniform float directionalIntensity;
uniform float pointIntensity;
uniform float environmentIntensity;
uniform float fresnelPower;
uniform vec3 directionalDirection;
uniform vec3 directionalColor;
uniform vec3 pointPosition;
uniform vec3 pointColor;
uniform vec3 attenuation;
uniform samplerCube environmentMap;

out vec4 fragmentColor;

// de color lineal a sRGB
vec3 linearToSrgb(vec3 value) {
    vec3 low = 12.92 * value;
    vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

vec3 safeNormalize(vec3 v) {
    return v / max(length(v), 0.00001);
}

// difusa + especular de una luz
// toLight apunta a la luz, toCamera apunta a la cámara
vec3 directLight(vec3 normal, vec3 toCamera, vec3 toLight, vec3 base, vec3 radiance) {
    float diffuse = max(dot(normal, toLight), 0.0);
    float exponent = 2.0 + 254.0 * pow(1.0 - roughness, 4.0);
    float highlight;
    if (phongShading) {
        // Phong: rayo reflejado contra la vista, con un cuarto del exponente
        vec3 reflected = reflect(-toLight, normal);
        highlight = diffuse > 0.0 ? pow(max(dot(reflected, toCamera), 0.0), exponent * 0.25) : 0.0;
    } else {
        // Blinn-Phong: vector medio contra la normal
        vec3 halfway = safeNormalize(toLight + toCamera);
        highlight = diffuse > 0.0 ? pow(max(dot(normal, halfway), 0.0), exponent) : 0.0;
    }
    vec3 specularColor = mix(vec3(specular), base, metallic);
    return radiance * (base * (1.0 - metallic) * diffuse + specularColor * highlight);
}

void main() {
    vec4 color = baseColor;
    if (textured) color *= texture(baseTexture, textureUV);
    // alphaMode: 0 opaco, 1 recorte, 2 transparente
    if (alphaMode == 1 && color.a < alphaCutoff) discard;
    float alpha = alphaMode == 2 ? color.a : 1.0;

    // normal de la cara con derivadas, para modelos sin normales
    vec3 geometricNormal = safeNormalize(cross(dFdx(worldPosition), dFdy(worldPosition)));
    vec3 faceNormal = gl_FrontFacing ? geometricNormal : -geometricNormal;
    vec3 normal = length(worldNormal) > 0.00001 ? safeNormalize(worldNormal) : faceNormal;
    if (doubleSided && !gl_FrontFacing) normal = -normal;

    vec3 result = color.rgb;
    if (!unlit && !legacyUnlit) {
        vec3 toCamera = safeNormalize(cameraPosition - worldPosition);
        result = vec3(0.0);
        if (directEnabled) {
            result = ambient * color.rgb * (1.0 - metallic);
            // luz direccional
            if (directionalEnabled) {
                vec3 toSun = -directionalDirection;
                vec3 directionalRadiance = directionalColor * directionalIntensity;
                result += directLight(normal, toCamera, toSun, color.rgb, directionalRadiance);
            }
            // luz puntual con atenuación
            if (pointEnabled) {
                vec3 toPoint = pointPosition - worldPosition;
                float lightDistance = length(toPoint);
                vec3 distanceTerms = vec3(1.0, lightDistance, lightDistance * lightDistance);
                float falloff = 1.0 / dot(attenuation, distanceTerms);
                vec3 pointRadiance = pointColor * pointIntensity * falloff;
                result += directLight(normal, toCamera, safeNormalize(toPoint), color.rgb, pointRadiance);
            }
        }
        // reflejo del cielo con Fresnel de Schlick
        if (reflectionEnabled) {
            vec3 f0 = mix(vec3(0.04), color.rgb, metallic);
            float cosine = clamp(dot(normal, toCamera), 0.0, 1.0);
            vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - cosine, fresnelPower);
            // sin Fresnel el reflejo es constante (F0)
            if (!fresnelEnabled) fresnel = f0;
            vec3 reflected = texture(environmentMap, reflect(-toCamera, normal)).rgb;
            vec3 weight = fresnel * reflection * environmentIntensity * (1.0 - 0.6 * roughness);
            weight = clamp(weight, 0.0, 1.0);
            result = mix(result, reflected, weight);
        }
    }
    // modo legacy convierte a sRGB aquí, la demo deja que lo haga el framebuffer
    fragmentColor = vec4(legacyUnlit ? linearToSrgb(result) : result, alpha);
}
