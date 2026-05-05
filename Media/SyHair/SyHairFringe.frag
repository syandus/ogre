#include <OgreUnifiedShader.h>

#ifndef SY_HAIR_ENABLE_DITHER
#define SY_HAIR_ENABLE_DITHER 0
#endif

#ifndef SY_HAIR_ENABLE_BLUE_NOISE
#define SY_HAIR_ENABLE_BLUE_NOISE 0
#endif

SAMPLER2D(uDiffuseAlpha, 0);
#if SY_HAIR_ENABLE_DITHER && SY_HAIR_ENABLE_BLUE_NOISE
SAMPLER2D(uBlueNoise, 1);
#endif

#define SY_HAIR_LIGHT_COUNT 3
#define SY_HAIR_INV_PI 0.31830988618

OGRE_UNIFORMS(
uniform vec4 uLightPos[SY_HAIR_LIGHT_COUNT];
uniform vec4 uLightDiffuse[SY_HAIR_LIGHT_COUNT];
uniform vec4 uSceneColour;
uniform vec4 uDesaturation;

uniform float uFringeMin;
uniform float uFringeMax;
uniform float uFringeAlphaScale;
uniform float uFrameIndex;
uniform float uBackLightStrength;
)

MAIN_PARAMETERS
IN(vec2 vUV, TEXCOORD0)
IN(vec3 vWorldPos, TEXCOORD1)
IN(vec3 vWorldNormal, TEXCOORD2)

vec3 sySafeNormalize(vec3 value, vec3 fallback)
{
    float len2 = dot(value, value);
    if (len2 > 0.00000001)
    {
        return value * inversesqrt(len2);
    }

    return fallback;
}

vec3 syLightVector(vec4 lightPos, vec3 worldPos)
{
    vec3 fallback = sySafeNormalize(vec3(0.4, 0.8, 0.2), vec3(0.0, 1.0, 0.0));
    return sySafeNormalize(lightPos.xyz - worldPos * lightPos.w, fallback);
}

#if SY_HAIR_ENABLE_DITHER
float syBayer4(vec2 fragCoord)
{
    vec2 p = mod(floor(fragCoord), vec2_splat(4.0));
    float x = p.x;
    float y = p.y;
    float value = 0.0;

    if (y < 1.0)
    {
        if (x < 1.0) value = 0.0;
        else if (x < 2.0) value = 8.0;
        else if (x < 3.0) value = 2.0;
        else value = 10.0;
    }
    else if (y < 2.0)
    {
        if (x < 1.0) value = 12.0;
        else if (x < 2.0) value = 4.0;
        else if (x < 3.0) value = 14.0;
        else value = 6.0;
    }
    else if (y < 3.0)
    {
        if (x < 1.0) value = 3.0;
        else if (x < 2.0) value = 11.0;
        else if (x < 3.0) value = 1.0;
        else value = 9.0;
    }
    else
    {
        if (x < 1.0) value = 15.0;
        else if (x < 2.0) value = 7.0;
        else if (x < 3.0) value = 13.0;
        else value = 5.0;
    }

    return (value + 0.5) / 16.0;
}

float sySampleNoise(vec2 fragCoord)
{
#if SY_HAIR_ENABLE_BLUE_NOISE
    vec2 uv = fract((fragCoord + vec2(uFrameIndex * 17.0, uFrameIndex * 29.0)) * (1.0 / 128.0));
    return texture2D(uBlueNoise, uv).r;
#else
    float noise = syBayer4(fragCoord);

    return noise;
#endif
}
#endif

vec4 syDesaturate(vec4 colour, float amount)
{
    float luma = dot(colour.rgb, vec3(0.299, 0.587, 0.114));
    colour.rgb = mix(colour.rgb, vec3_splat(luma), clamp(amount, 0.0, 1.0));
    return colour;
}

MAIN_DECLARATION
{
    vec4 tex = texture2D(uDiffuseAlpha, vUV);
    float alpha = tex.a;

    if (alpha < uFringeMin || alpha >= uFringeMax)
    {
        discard;
    }

    float fringe = smoothstep(uFringeMin, uFringeMax, alpha);

#if SY_HAIR_ENABLE_DITHER
    float noise = sySampleNoise(gl_FragCoord.xy);
    if (fringe < noise)
    {
        discard;
    }
#endif

    vec3 n = sySafeNormalize(vWorldNormal, vec3(0.0, 0.0, 1.0));
    vec3 direct = vec3_splat(0.0);

    for (int i = 0; i < SY_HAIR_LIGHT_COUNT; ++i)
    {
        vec3 l = syLightVector(uLightPos[i], vWorldPos);
        float front = saturate(dot(n, l));
        float back = saturate(dot(-n, l)) * uBackLightStrength;
        float lit = (front + back) * SY_HAIR_INV_PI;
        direct += uLightDiffuse[i].rgb * lit;
    }

    vec3 color = tex.rgb * (uSceneColour.rgb + direct);

    gl_FragColor = syDesaturate(vec4(color, fringe * uFringeAlphaScale), uDesaturation.x);
}
