# IBL Primer For OGRE RTSS (Emscripten / WebGL)

## What IBL Is
Image-Based Lighting (IBL) uses an environment map (usually a cubemap) to approximate global illumination. Instead of computing light from explicit light sources only, it samples the surrounding environment to add diffuse and specular contributions.

In RTSS, IBL shows up when the `ImageBasedLighting` SubRenderState is added. It builds on the same Cook-Torrance shading model used by the PBR path and adds:
- Diffuse environment lighting (low-frequency)
- Specular environment reflections (high-frequency, view-dependent)

## Why It Matters For Emscripten
Emscripten targets WebGL via the GLES2 RenderSystem (`glsles`), so shader precision and performance are tighter. IBL is still supported, but it relies on shader code in `Media/RTShaderLib` and sampler access that must be WebGL-compatible.

## The RTSS IBL Shader Flow (Simplified)
1. **Inputs**
   - View-space normal and view position
   - Inverse view matrix (for converting reflection vectors to world space)
   - Environment cubemap (`iblEnvTex`)
   - DFG LUT (`dfgTex`) used to approximate microfacet BRDF integration

2. **Compute view angle**
   - `NoV = dot(normal, view)`
   - Used to look up BRDF response from the DFG LUT

3. **Specular IBL**
   - Compute reflection vector
   - Prefilter the env cubemap based on roughness
   - Scale using DFG LUT and Fresnel response

4. **Diffuse IBL**
   - Sample roughness=1 mip level for diffuse irradiance
   - Multiply by diffuse color

5. **Combine**
   - Add to the existing shading result

## Where The Code Lives
- DFG LUT + specular and diffuse IBL are in:
  - `Media/RTShaderLib/RTSLib_IBL.glsl`
- PBR parameters and Fresnel are in:
  - `Media/RTShaderLib/SGXLib_CookTorrance.glsl`
- RTSS wires IBL from:
  - `Components/RTShaderSystem/src/OgreShaderImageBasedLighting.cpp`

## Key Math Terms (RTSS)
- `NoV`: normal · view
- `dfg`: a 2D LUT storing integrated Fresnel and geometric terms
- `prefilteredRadiance`: environment sampled at roughness-dependent mip
- `specularDFG`: combines `dfg` with `f0` to produce a specular response

## Common Visual Issues
- **Over-bright edges**: Fresnel and IBL both raise specular at grazing angles.
- **Too glossy cloth**: Cloth needs a sheen model, not a single microfacet lobe.
- **Banding or shimmer**: Often due to precision or low-quality cubemap mip chain.

## Practical Guidance
- Use a cubemap with proper mip levels for roughness sampling.
- Keep DFG LUT in the RTShaderLib directory and ensure it is in the Emscripten VFS.
- If cloth looks too shiny, reduce Fresnel in the Cook-Torrance path and scale IBL specular.

## References In This Repo
- `Media/RTShaderLib/RTSLib_IBL.glsl`
- `Media/RTShaderLib/SGXLib_CookTorrance.glsl`
- `Components/RTShaderSystem/src/OgreShaderImageBasedLighting.cpp`
- `Samples/ShaderSystem/src/ShaderSystem.cpp` (adds IBL SubRenderState)
