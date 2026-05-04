# Sy Hair Techniques Deep Dive {#Sy-Hair-Techniques}

`Media/SyHair` is a pragmatic hair-card renderer for OGRE materials. It is
designed for avatar hairstyles exported as triangle cards with colour in RGB and
coverage in the diffuse texture alpha channel. Character Creator / Reallusion
assets are the primary target.

This document is the detailed technical reference for maintaining, tuning, or
extending SyHair. For a short usage guide, see @ref Sy-Hair-Materials.

@tableofcontents

# Goals And Non-goals

SyHair tries to make alpha-card hair stable in an ordinary forward render path:

- dense hair should write stable depth like cutout geometry
- thin card edges should recover some softness
- three scene lights should affect hair at roughly the same brightness scale as
  RTSS `metal_roughness` clothing
- zero light and zero scene colour should produce black hair
- tuning should be possible from `.material` parameters

SyHair is not:

- strand rendering
- order-independent transparency
- physically complete hair scattering
- a direct Cook-Torrance material
- a replacement for authored hair normals and masks

The implementation intentionally borrows RTSS energy discipline without copying
the full RTSS Cook-Torrance BRDF. Hair cards need controlled two-sided response,
alpha coverage behavior, and anisotropic-looking highlights.

# Files And Resource Inputs

The runtime files live in `Media/SyHair`:

- `SyHair.program`: program declarations and default parameters
- `SyHair.material`: reusable material templates
- `SyHair.vert`: shared vertex shader
- `SyHairCore.frag`: alpha-clipped core fragment shader
- `SyHairFringe.frag`: transparent fringe fragment shader
- `sy_flat_normal.png`: neutral normal fallback
- `sy_blue_noise_128.png`: optional blue-noise dither texture

`Media/Main` must also be available because the shaders include
`OgreUnifiedShader.h`.

The texture inputs are:

| Material variable | Used by | Meaning |
| --- | --- | --- |
| `$DiffuseAlpha` | Core and Fringe | RGB hair colour, alpha coverage mask |
| `$Normal` | Core | Tangent-space normal map, or flat fallback |
| `$BlueNoise` | Fringe | Optional 128x128 blue-noise texture |

# Two-pass Render Model

The same hair cards are drawn twice.

## Core Pass

The `Core` pass is the stable base:

- alpha-clipped with `discard`
- `depth_write on`
- `depth_check on`
- `scene_blend one zero`
- `transparent_sorting off`
- `alpha_to_coverage on`

The core pass treats dense hair as cutout opaque geometry. This is important
because a single hairstyle mesh often contains many intersecting cards inside
one `SubEntity`. OGRE sorts transparent renderables, not individual triangles
inside that renderable, so making the whole hairstyle alpha-blended causes
unstable self-overlap.

Core alpha behavior:

```glsl
if (alpha < uAlphaClip)
{
    discard;
}

float edge = smoothstep(uEdgeLow, uEdgeHigh, alpha);
float outAlpha = mix(1.0, edge, saturate(uUseA2C));
```

`uUseA2C = 1.0` lets the output alpha carry the smooth edge into
alpha-to-coverage when the active render target has MSAA and the backend
supports it. `uUseA2C = 0.0` forces core output alpha to `1.0` after clipping.

## Fringe Pass

The `Fringe` pass restores soft edges:

- renders only a narrow alpha range
- `depth_write off`
- `depth_check on`
- `scene_blend src_alpha one_minus_src_alpha`
- `transparent_sorting on`

Fringe alpha behavior:

```glsl
if (alpha < uFringeMin || alpha >= uFringeMax)
{
    discard;
}

float fringe = smoothstep(uFringeMin, uFringeMax, alpha);
gl_FragColor.a = fringe * uFringeAlphaScale;
```

The fringe pass should stay subtle. It is for edge recovery, not for making the
whole hairstyle transparent.

# Vertex-space Flow

`SyHair.vert` outputs all lighting data in world space:

- `vUV`: diffuse/normal texture coordinate
- `vWorldPos`: world-space position
- `vWorldNormal`: normalized world-space normal
- `vWorldTangent`: normalized world-space tangent
- `vWorldBitangent`: normalized world-space bitangent

The shader transforms position with `uWorldViewProj` and transforms normal and
tangent using `uWorld` with a zero `w`:

```glsl
vec3 n = normalize((uWorld * vec4(normal.xyz, 0.0)).xyz);
vec3 t = normalize((uWorld * vec4(tangent.xyz, 0.0)).xyz);
```

The tangent is re-orthogonalized against the normal:

```glsl
t = normalize(t - n * dot(n, t));
```

The bitangent uses the mesh tangent sign:

```glsl
float tangentSign = tangent.w < 0.0 ? -1.0 : 1.0;
vec3 b = normalize(cross(n, t) * tangentSign);
```

If the mesh tangent is invalid, SyHair builds a fallback tangent by crossing the
normal with a stable axis. This prevents NaNs and keeps the shader rendering,
but it cannot recover authored strand direction.

Important limitation: normals and tangents are transformed with `uWorld`, not an
inverse-transpose normal matrix. This is acceptable for normal avatar transforms
and uniform scale. Nonuniform scale can skew lighting.

# Normal Map Decode

The core pass samples `uNormalMap` and decodes it as tangent-space normal data:

```glsl
vec3 localN = nTex * 2.0 - 1.0;
localN.xy *= uNormalStrength;
localN = normalize(localN);
vec3 worldN = normalize(mat3(t, b, n) * localN);
```

`uNormalStrength` scales only the XY perturbation. This lets a material bind a
placeholder normal map while disabling its visible effect:

- `0.0`: ignore tangent-space perturbation, use card/mesh normal
- `0.3` to `0.6`: mild authored hair normal
- `1.0`: full authored normal map

If the only available normal is a borrowed body normal, use
`uNormalStrength 0.0`. Body normals usually encode skin or clothing detail, not
strand/card detail, and can make hair lighting look noisy or wrong.

The core pass flips the decoded normal toward the viewer:

```glsl
if (dot(n, viewDir) < 0.0)
{
    n = -n;
}
```

This keeps card normals usable from both sides. Directional light response is
still controlled separately by the front/back lighting terms.

# Scene And Light Bindings

SyHair uses OGRE auto constants from `SyHair.program`:

| Uniform | OGRE auto constant | Count | Reason |
| --- | --- | --- | --- |
| `uWorldViewProj` | `worldviewproj_matrix` | 1 | clip-space transform |
| `uWorld` | `world_matrix` | 1 | world-space attributes |
| `uSceneColour` | `derived_scene_colour` | 1 | ambient/emissive scene term |
| `uCameraPos` | `camera_position` | 1 | view vector for specular |
| `uLightPos` | `light_position_array` | 3 | first three light vectors/positions |
| `uLightDiffuse` | `light_diffuse_colour_power_scaled_array` | 3 | light colour multiplied by power |

`derived_scene_colour` is:

```cpp
ambient_light_colour * surface_ambient_colour + surface_emissive_colour
```

SyHair multiplies that scene term by the sampled hair colour:

```glsl
tex.rgb * uSceneColour.rgb
```

This keeps the scene term texture-tinted. If a material or pass has nonzero
self-illumination, `derived_scene_colour` can include it. That is intentional
RTSS-style behavior, but it should be considered when debugging self-lit hair.

The light colour binding uses the power-scaled array so light intensity sliders
can drive contribution to zero:

```material
param_named_auto uLightDiffuse light_diffuse_colour_power_scaled_array 3
```

SyHair is tuned for three directional lights such as key/fill/rim. The light
vector helper also handles `light_position_array` entries with `w = 1.0`, but
SyHair does not apply point-light distance attenuation or spotlight cone
attenuation.

# Diffuse Lighting Math

SyHair uses a Lambert-scaled direct term:

```glsl
#define SY_HAIR_INV_PI 0.31830988618

float front = saturate(dot(n, l));
float back = saturate(dot(-n, l)) * uBackLightStrength;
float diffuse = (front + back) * SY_HAIR_INV_PI;
```

This replaced the old bring-up model:

```glsl
0.25 + 0.75 * abs(dot(n, l))
```

The old model was intentionally robust during early tests, but it over-lit real
assets because:

- every light had a nonzero floor
- back-facing card normals were lit as strongly as front-facing normals
- multiple lights could stack to white
- the response was not scaled like Lambert diffuse in RTSS Cook-Torrance

The current model is stricter:

- front-facing light uses `saturate(dot(n, l))`
- back-facing light is explicit and tunable
- both front and back terms are scaled by `1 / pi`
- zero light colour produces zero direct diffuse

`uBackLightStrength` is a hair-card softness control, not a general brightness
boost:

- `0.0`: strict front-only debug mode
- `0.10` to `0.15`: subtle production default
- `0.25`: stronger rim/back card fill

# Specular Lighting Math

The core pass adds a cheap anisotropic-looking highlight. It is not a physically
complete Marschner or Kajiya-Kay hair model, but it uses the tangent direction
to make highlights run along cards:

```glsl
vec3 h = normalize(l + v);
float tDotH = dot(t, h);
float sinTH = sqrt(max(1.0 - tDotH * tDotH, 0.0));
float exponent = mix(24.0, 96.0, saturate(1.0 - roughness));
float nDotL = saturate(dot(n, l));
float spec = pow(sinTH, exponent) * nDotL;
```

Two lobes are accumulated:

- primary lobe: tangent `t`, roughness `0.45`
- secondary lobe: tangent biased by `0.25 * n`, roughness `0.70`, weighted by
  `0.35`

The final specular contribution is:

```glsl
specular += lightColor * ((spec1 + spec2) * uSpecStrength);
```

`uSpecStrength` defaults to `0.15`. Set it to `0.0` when matching diffuse
brightness against clothing, then raise it only after the hair no longer looks
over-lit.

The fringe pass does not add specular. This avoids bright transparent halos.

# Dither And Noise

Fringe can optionally dither the transparent band:

```glsl
if (uUseDither > 0.5)
{
    float noise = sySampleNoise(gl_FragCoord.xy);
    if (fringe < noise)
    {
        discard;
    }
}
```

`sySampleNoise()` uses Bayer by default:

```glsl
float noise = syBayer4(fragCoord);
```

If `uUseBlueNoise > 0.5`, it samples `$BlueNoise` instead:

```glsl
vec2 uv = fract((fragCoord + vec2(uFrameIndex * 17.0,
                                  uFrameIndex * 29.0)) * (1.0 / 128.0));
noise = texture2D(uBlueNoise, uv).r;
```

Recommended defaults:

- `uUseDither = 0.0`
- `uUseBlueNoise = 1.0`
- `uFrameIndex = 0.0`

Only enable dithering after core/fringe brightness and coverage are correct.
Animating `uFrameIndex` without temporal accumulation can make edges sparkle, so
keep the fringe subtle when using animated blue noise.

# Parameter Reference

## Core Parameters

| Parameter | Default | Debug range | Meaning |
| --- | ---: | ---: | --- |
| `uAlphaClip` | `0.55` | `0.35` to `0.75` | core discard threshold |
| `uEdgeLow` | `0.30` | `0.15` to `0.45` | lower alpha-to-coverage edge |
| `uEdgeHigh` | `0.60` | `0.45` to `0.85` | upper alpha-to-coverage edge |
| `uUseA2C` | `1.0` | `0.0` or `1.0` | use smooth alpha for A2C |
| `uNormalStrength` | `1.0` | `0.0` to `1.0` | normal map perturbation strength |
| `uBackLightStrength` | `0.15` | `0.0` to `0.25` | controlled two-sided diffuse |
| `uSpecStrength` | `0.15` | `0.0` to `0.25` | tangent highlight strength |

## Fringe Parameters

| Parameter | Default | Debug range | Meaning |
| --- | ---: | ---: | --- |
| `uFringeMin` | `0.20` | `0.05` to `0.45` | first alpha value drawn by fringe |
| `uFringeMax` | `0.55` | `0.35` to `0.95` | alpha value where fringe stops |
| `uFringeAlphaScale` | `0.45` | `0.10` to `1.00` | final fringe opacity multiplier |
| `uUseDither` | `0.0` | `0.0` or `1.0` | enable fringe dither discard |
| `uUseBlueNoise` | `1.0` | `0.0` or `1.0` | use blue noise instead of Bayer |
| `uFrameIndex` | `0.0` | frame counter | animated blue-noise offset |
| `uBackLightStrength` | `0.15` | `0.0` to `0.25` | same diffuse softness as core |

# Tuning Recipes

## Zero-light Validation

Set all directional light powers to `0` and scene ambient RGB to `0`.

Expected result:

- core hair should be black
- fringe hair should be black
- alpha silhouettes may still be visible only as black coverage

If hair remains bright:

- confirm the running media contains the latest SyHair shaders
- check pass self-illumination because `derived_scene_colour` includes emissive
- set `uSpecStrength 0.0`
- set `uBackLightStrength 0.0`

## Core-only Bring-up

Use `SyHair/Base_CoreOnly` first.

Recommended first debug settings:

```material
param_named uNormalStrength float 0.0
param_named uBackLightStrength float 0.0
param_named uSpecStrength float 0.0
```

Tune `uAlphaClip` until dense hair coverage is acceptable, then restore
`uBackLightStrength 0.15` and `uSpecStrength 0.15`.

## Fringe Visibility A/B

Use `SyHair/Base_MaskedFringe` and temporarily exaggerate fringe:

```material
param_named uFringeMin float 0.05
param_named uFringeMax float 0.95
param_named uFringeAlphaScale float 1.0
param_named uUseDither float 0.0
```

Expected result: edges should become softer and more transparent than core-only.
If the result looks identical to core-only, the fringe pass is not contributing
or the texture alpha has little usable intermediate range.

## Matching RTSS Metal-roughness Clothing

Use clothing with `lighting_stage metal_roughness` as the brightness reference.

Procedure:

1. Set `uSpecStrength 0.0`.
2. Set `uBackLightStrength 0.0`.
3. Match diffuse brightness under normal key/fill/rim lights.
4. Raise `uBackLightStrength` to `0.10` or `0.15`.
5. Raise `uSpecStrength` only if hair needs a highlight.

If hair goes white before clothing, do not raise alpha or fringe first. Check
lighting terms first.

# Troubleshooting

## Hair Looks Self-lit

- Check stale shader media first; old SyHair builds had floor lighting.
- Check whether `derived_scene_colour` includes pass emissive.
- Confirm light powers are actually zero.
- Set `uBackLightStrength 0.0` and `uSpecStrength 0.0`.

## Hair Is Too White

- Reduce `uSpecStrength`.
- Reduce `uBackLightStrength`.
- Reduce `uFringeAlphaScale`.
- Inspect whether all three light slots are strong.

## Hair Is Chunky Or Shaved

- Lower `uAlphaClip`.
- Lower `uFringeMin`.
- Raise `uFringeAlphaScale` slightly.
- Inspect whether the alpha texture is mostly binary.

## Hair Is Soupy Or Transparent

- Raise `uAlphaClip`.
- Raise `uFringeMin`.
- Lower `uFringeAlphaScale`.
- Narrow `uFringeMax - uFringeMin` if the whole card reads transparent.

## Hair Has A Halo

- Lower `uFringeAlphaScale`.
- Raise `uFringeMin`.
- Keep the fringe pass specular-free.
- Confirm the diffuse alpha has useful partial coverage instead of a broad grey
  outline.

## Edges Sparkle

- Set `uUseDither 0.0`.
- Tune alpha/fringe without dither.
- Avoid animated `uFrameIndex` without temporal accumulation.
- Re-enable dither only if needed.

## Normal Detail Looks Wrong

- Set `uNormalStrength 0.0`.
- Check lighting again.
- Re-enable only with an authored hair normal map.
- Inspect tangent basis and nonuniform object scale if artifacts remain.

## Deploy Edits Disappear

SyHair shader files live in OGRE media, but asset `.material` edits in generated
`deploy/` folders may be overwritten by asset builds. Make bring-up edits there
only as scratch, then move stable settings into source assets or the conversion
pipeline used by the project.
