#include <OgreUnifiedShader.h>

#ifndef SY_HAIR_ENABLE_NORMAL_MAPS
#define SY_HAIR_ENABLE_NORMAL_MAPS 0
#endif

OGRE_UNIFORMS(
uniform mat4 uWorldViewProj;
uniform mat4 uWorld;
)

MAIN_PARAMETERS
IN(vec4 position, POSITION)
IN(vec4 normal, NORMAL)
IN(vec4 tangent, TANGENT)
IN(vec2 uv0, TEXCOORD0)

OUT(vec2 vUV, TEXCOORD0)
OUT(vec3 vWorldPos, TEXCOORD1)
OUT(vec3 vWorldNormal, TEXCOORD2)
OUT(vec3 vWorldTangent, TEXCOORD3)
OUT(vec3 vWorldBitangent, TEXCOORD4)

vec3 sySafeNormalize(vec3 value, vec3 fallback)
{
    float len2 = dot(value, value);
    if (len2 > 0.00000001)
    {
        return value * inversesqrt(len2);
    }

    return fallback;
}

vec3 syOrthogonal(vec3 n)
{
    vec3 axis = vec3(0.0, 0.0, 1.0);
    if (abs(n.z) > 0.999)
    {
        axis = vec3(0.0, 1.0, 0.0);
    }

    return sySafeNormalize(cross(axis, n), vec3(1.0, 0.0, 0.0));
}

MAIN_DECLARATION
{
    vec4 worldPos = mul(uWorld, position);

    vec3 n = sySafeNormalize(mul(uWorld, vec4(normal.xyz, 0.0)).xyz, vec3(0.0, 0.0, 1.0));
    vec3 fallbackTangent = syOrthogonal(n);
    vec3 t = sySafeNormalize(mul(uWorld, vec4(tangent.xyz, 0.0)).xyz, fallbackTangent);
    t = sySafeNormalize(t - n * dot(n, t), fallbackTangent);

    float tangentSign = 1.0;
    if (tangent.w < 0.0)
    {
        tangentSign = -1.0;
    }

    vec3 b = sySafeNormalize(cross(n, t) * tangentSign, cross(n, fallbackTangent));

    vUV = uv0;
    vWorldPos = worldPos.xyz;
    vWorldNormal = n;
    vWorldTangent = t;
    vWorldBitangent = b;

    gl_Position = mul(uWorldViewProj, position);
}
