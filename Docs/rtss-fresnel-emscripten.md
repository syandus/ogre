# RTSS Fresnel Edge Shine On Emscripten (Research + Actions)

## Summary
This document explains why RTSS materials show strong edge brightening on Emscripten/WebGL and proposes low-effort, global mitigation options that do not require per-material BRDF classification. It is based on the current OGRE fork sources in `/home/rko/src/ogre`.

## Context (Emscripten/WebGL)
- Emscripten builds force the GLES2 RenderSystem (`glsles`) and disable desktop GL. See `CMakeLists.txt:245`.
- The Emscripten sample mounts `/RTShaderLib` and `/Main` into the OGRE VFS. Shader includes and `OgreUnifiedShader.h` resolve from there. See `Samples/Emscripten/media/resources.cfg:1`.
- RTSS chooses `glsles` as the shader language when supported. See `Components/RTShaderSystem/src/OgreShaderGenerator.cpp:139`.
- The RTSS program writer always includes `OgreUnifiedShader.h` and the dependencies declared by SubRenderStates (e.g., `SGXLib_CookTorrance.glsl`). See `Components/RTShaderSystem/src/OgreShaderProgramWriter.cpp:175`.
- GLSL ES preprocessing defines `GL_ES`, replaces `OGRE_NATIVE_GLSL_VERSION_DIRECTIVE` with the real `#version`, and applies `OGRE_GLSLES=<version>`. See `RenderSystems/GLSupport/src/GLSL/OgreGLSLShaderCommon.cpp:55` and `OgreMain/src/OgreHighLevelGpuProgram.cpp:209`.

## Fresnel / Edge Brightening: Where It Happens
### Direct lighting (Cook-Torrance)
The Cook-Torrance path uses Schlick Fresnel and boosts reflectance at glancing angles:
- `Media/RTShaderLib/SGXLib_CookTorrance.glsl:50`:
  `F_Schlick(f0, f90, VoH) = f0 + (f90 - f0) * pow5(1.0 - VoH)`
- `Media/RTShaderLib/SGXLib_CookTorrance.glsl:140`:
  `float f90 = saturate(dot(pixel.f0, vec3_splat(50.0 * 0.33)));`
  `vec3 F = F_Schlick(pixel.f0, f90, NoH);`

This is physically intended for dielectrics, but it can look wrong for cloth.

### IBL (Image Based Lighting)
IBL also increases specular at grazing angles using a DFG LUT indexed by `NoV`:
- `Media/RTShaderLib/RTSLib_IBL.glsl:50`:
  `pixel.dfg = PrefilteredDFG_LUT(..., shading_NoV)`
  `vec3 E = specularDFG(pixel)`

If you use RTSS image-based lighting, this can reinforce the edge sheen.

## Why It Looks Unnatural On Cloth
Cloth materials are not well modeled by a single Cook-Torrance specular lobe. They have fiber-level multiple scattering and often show a “sheen” lobe rather than strong Fresnel-driven edge shine. Without a cloth-specific BRDF, the generic microfacet Fresnel can appear too glossy at glancing angles.

## Suggested Courses Of Action (No Per-Material Classification)
All options below are global and low effort. They trade physical accuracy for better art direction on cloth.

### 1) Scale Down Fresnel “White” Endpoint (Recommended)
Reduce `f90` (the grazing reflectance target) globally to damp edge brightening.

Suggested change in `Media/RTShaderLib/SGXLib_CookTorrance.glsl`:
```glsl
#ifndef OGRE_FRESNEL_F90_SCALE
#define OGRE_FRESNEL_F90_SCALE 0.35
#endif

float f90 = saturate(dot(pixel.f0, vec3_splat(50.0 * 0.33))) * OGRE_FRESNEL_F90_SCALE;
vec3 F = F_Schlick(pixel.f0, f90, NoH);
```

Pros:
- Directly targets the edge-brightening term.
- Small, isolated change.
- Easy to tune.

Cons:
- Less physically accurate for glossy dielectrics.

### 2) Scale Specular IBL Term (If Using IBL)
If IBL is enabled, scale down `E` in `Media/RTShaderLib/RTSLib_IBL.glsl`:
```glsl
#ifndef OGRE_FRESNEL_IBL_SCALE
#define OGRE_FRESNEL_IBL_SCALE 0.35
#endif

vec3 E = specularDFG(pixel) * OGRE_FRESNEL_IBL_SCALE;
```

Pros:
- Addresses the environment reflection component.

Cons:
- Globally reduces specular environment contributions.

### 3) Raise Minimum Roughness
In `Media/RTShaderLib/SGXLib_CookTorrance.glsl:11`, increase `MIN_PERCEPTUAL_ROUGHNESS`.

Pros:
- Softens specular highlights overall.

Cons:
- Affects all materials, not just cloth.
- Changes highlight size, not just edge behavior.

### 4) Lower Base Reflectance (f0)
Lower the constant 0.04 in `computeF0()` or scale `pixel.f0` in `PBR_MakeParams`.

Pros:
- Reduces specular intensity everywhere.

Cons:
- Does not specifically fix grazing-angle brightness if `f90` stays high.

### 5) Add A Runtime Global Knob (Small C++ + Shader)
Expose a global scalar in RTSS to tune Fresnel at runtime (e.g., a uniform in the generated shaders). This could be wired from `CookTorranceLighting::createCpuSubPrograms`.

Pros:
- No need to edit shader source files per build.
- Tunable from UI/console.

Cons:
- Slightly more engineering work (RTSS uniform plumbing).

## Recommended Path
1. Start with the simple `OGRE_FRESNEL_F90_SCALE` change in `SGXLib_CookTorrance.glsl`.
2. If IBL is enabled, also apply `OGRE_FRESNEL_IBL_SCALE` in `RTSLib_IBL.glsl`.
3. Tune constants in the 0.25–0.5 range until cloth edge sheen is acceptable.

This gives the strongest improvement with minimal changes and no material classification effort.

## Notes
- These changes are global and will alter how non-cloth materials look.
- If you later decide to differentiate cloth, a “sheen” lobe or cloth BRDF would be the physically correct path, but it is outside the current resource constraints.
