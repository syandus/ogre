#include <OgreUnifiedShader.h>

SAMPLER2D(uDiffuseAlpha, 0);
SAMPLER2D(uNormalMap, 1);

#define SY_HAIR_LIGHT_COUNT 3
#define SY_HAIR_INV_PI 0.31830988618

OGRE_UNIFORMS(
uniform vec4 uLightPos[SY_HAIR_LIGHT_COUNT];
uniform vec4 uLightDiffuse[SY_HAIR_LIGHT_COUNT];
uniform vec4 uSceneColour;
uniform vec3 uCameraPos;

uniform float uAlphaClip;
uniform float uEdgeLow;
uniform float uEdgeHigh;
uniform float uUseA2C;
uniform float uNormalStrength;
uniform float uBackLightStrength;
uniform float uSpecStrength;
)

MAIN_PARAMETERS
IN(vec2 vUV, TEXCOORD0)
IN(vec3 vWorldPos, TEXCOORD1)
IN(vec3 vWorldNormal, TEXCOORD2)
IN(vec3 vWorldTangent, TEXCOORD3)
IN(vec3 vWorldBitangent, TEXCOORD4)

vec3 sySafeNormalize(vec3 value, vec3 fallback)
{
    float len2 = dot(value, value);
    if (len2 > 0.00000001)
    {
        return value * inversesqrt(len2);
    }

    return fallback;
}

vec3 syDecodeNormal(vec3 nTex, vec3 t, vec3 b, vec3 n)
{
    vec3 localN = nTex * 2.0 - 1.0;
    localN.xy *= uNormalStrength;
    localN = sySafeNormalize(localN, vec3(0.0, 0.0, 1.0));

    mat3 tbn = mtxFromCols(t, b, n);
    return sySafeNormalize(mul(tbn, localN), n);
}

vec3 syFaceViewer(vec3 n, vec3 viewDir)
{
    if (dot(n, viewDir) < 0.0)
    {
        return -n;
    }

    return n;
}

vec3 syLightVector(vec4 lightPos, vec3 worldPos)
{
    vec3 fallback = sySafeNormalize(vec3(0.4, 0.8, 0.2), vec3(0.0, 1.0, 0.0));
    return sySafeNormalize(lightPos.xyz - worldPos * lightPos.w, fallback);
}

float syHairSpec(vec3 n, vec3 t, vec3 l, vec3 v, float roughness)
{
    vec3 h = sySafeNormalize(l + v, n);
    float tDotH = dot(t, h);
    float sinTH = sqrt(max(1.0 - tDotH * tDotH, 0.0));
    float exponent = mix(24.0, 96.0, saturate(1.0 - roughness));
    float nDotL = saturate(dot(n, l));

    return pow(sinTH, exponent) * nDotL;
}

MAIN_DECLARATION
{
    vec4 tex = texture2D(uDiffuseAlpha, vUV);
    float alpha = tex.a;

    if (alpha < uAlphaClip)
    {
        discard;
    }

    vec3 viewDir = sySafeNormalize(uCameraPos - vWorldPos, vec3(0.0, 0.0, 1.0));
    vec3 nTex = texture2D(uNormalMap, vUV).xyz;
    vec3 n = syDecodeNormal(nTex, vWorldTangent, vWorldBitangent, vWorldNormal);
    n = syFaceViewer(n, viewDir);

    vec3 t = sySafeNormalize(vWorldTangent, vec3(1.0, 0.0, 0.0));
    vec3 direct = vec3_splat(0.0);
    vec3 specular = vec3_splat(0.0);

    for (int i = 0; i < SY_HAIR_LIGHT_COUNT; ++i)
    {
        vec3 l = syLightVector(uLightPos[i], vWorldPos);
        vec3 lightColor = uLightDiffuse[i].rgb;

        float front = saturate(dot(n, l));
        float back = saturate(dot(-n, l)) * uBackLightStrength;
        float diffuse = (front + back) * SY_HAIR_INV_PI;
        float spec1 = syHairSpec(n, t, l, viewDir, 0.45);
        float spec2 = syHairSpec(n, sySafeNormalize(t + 0.25 * n, t), l, viewDir, 0.70) * 0.35;

        direct += lightColor * diffuse;
        specular += lightColor * ((spec1 + spec2) * uSpecStrength);
    }

    vec3 color = tex.rgb * (uSceneColour.rgb + direct) + specular;

    float edge = smoothstep(uEdgeLow, uEdgeHigh, alpha);
    float outAlpha = mix(1.0, edge, saturate(uUseA2C));

    gl_FragColor = vec4(color, outAlpha);
}
