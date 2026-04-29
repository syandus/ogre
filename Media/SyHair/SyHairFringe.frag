#include <OgreUnifiedShader.h>

SAMPLER2D(uDiffuseAlpha, 0);
SAMPLER2D(uBlueNoise, 1);

OGRE_UNIFORMS(
uniform vec4 uLightPos;
uniform vec4 uLightDiffuse;
uniform vec4 uAmbient;

uniform float uFringeMin;
uniform float uFringeMax;
uniform float uFringeAlphaScale;
uniform float uFrameIndex;
uniform float uUseDither;
uniform float uUseBlueNoise;
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
    if (uUseBlueNoise > 0.5)
    {
        vec2 uv = fract((fragCoord + vec2(uFrameIndex * 17.0, uFrameIndex * 29.0)) * (1.0 / 128.0));
        return texture2D(uBlueNoise, uv).r;
    }

    return syBayer4(fragCoord);
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

    if (uUseDither > 0.5)
    {
        float noise = sySampleNoise(gl_FragCoord.xy);
        if (fringe < noise)
        {
            discard;
        }
    }

    vec3 n = sySafeNormalize(vWorldNormal, vec3(0.0, 0.0, 1.0));
    vec3 l = syLightVector(uLightPos, vWorldPos);
    float lit = 0.30 + 0.70 * saturate(abs(dot(n, l)));

    vec3 ambient = max(uAmbient.rgb, vec3_splat(0.08));
    vec3 lightColor = max(uLightDiffuse.rgb, vec3_splat(0.25));
    vec3 color = tex.rgb * (ambient + lightColor * lit);

    gl_FragColor = vec4(color, fringe * uFringeAlphaScale);
}
