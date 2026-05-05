# OGRE Fork

This is a public fork of OGRE (Object-Oriented Graphics Rendering Engine).

- **Origin**: `https://github.com/syandus/ogre.git`
- **Upstream**: `https://github.com/OGRECave/ogre.git`
- **Snapshot**: compared against `upstream/master` at `36db8f4c4` (2026-05-01).
- **Current branch**: `alivesim-v2`, `32` commits ahead and `0` behind upstream as of 2026-05-05.

## Differences from Upstream

### Texture Codecs

- `OgreMain/src/OgrePVRTCCodec.cpp` expands PVR v3 format mapping beyond PVRTC/PVRTC2 to ETC1, ETC2 RGB/RGBA/RGB_A1, DXT1-5, BC1-7, and ASTC 4x4 through 12x12.
- PVR v3 handling skips metadata by the declared header size and converts PVR mip counts to Ogre's extra-mipmap convention.

### RTSS Colour and Shading

- `Ogre::RTShader::ShaderGenerator` exposes linear-output controls:
  `set/getTargetConsumesLinear`, `set/getOutputGamma`, and
  `set/getDesaturationCustomParamIndex`.
- RTSS colour output supports shader-side gamma transfer, optional per-renderable desaturation from an `ACT_CUSTOM` parameter, and texture linear-conversion logging.
- Cook-Torrance Fresnel uses `OGRE_FRESNEL_F90_SCALE` to reduce edge brightening on WebGL/Emscripten targets.

### SyHair

- `Media/SyHair` adds a dual-pass alpha-card hair path:
  `SyHair/Base_MaskedFringe`, `SyHair/Base_CoreOnly`, `SyHair/CC5_*`, and
  `SyHair/Placeholder_MaskedFringe`.
- SyHair reads per-renderable desaturation from custom parameter index `0`
  (`.x`), matching the RTSS desaturation path.
- SyHair media is wired into resource templates, install rules, Emscripten sample media, and docs.

### Mesh Animation and WebSIMD

- Emscripten builds add `-msimd128 -msse4.2` and require `__wasm_simd128__`.
- Optimised utilities use the SSE/WebSIMD path under Emscripten; pose blending has an SSE fast path when vertex layout permits.
- Mesh pose/morph normal detection logs once per mesh, entity pose-normal init/finalise logs once per entity, and `Ogre::Mesh` adds `setIgnorePoseNormals` / `getIgnorePoseNormals`.

### Emscripten, WebGL, and Android

- Emscripten EGL window creation validates requested MSAA against the actual WebGL default framebuffer and fails when the browser did not provide multisampling.
- GLES2 ETC2 texture compression capability is gated on `WEBGL_compressed_texture_etc` for Emscripten/WebGL.
- The Character sample uses `PF_BYTE_RGBA` shadow textures instead of `PF_DEPTH16` for Android compatibility.

### Repo Configuration and Docs

- `.gitignore` includes WebAssembly build directories such as `/build-wasm/`,
  `/build-wasm.debug/`, and `/build-wasm.release/`.
- `.vscode/settings.json` adds C++ file associations.
- Added docs cover SyHair, RTSS Fresnel/IBL on Emscripten, WebGL sRGB texture handling, and SIMD opportunities.

## Development

Upstream OGRE has tests, but this fork's Emscripten configuration disables them.
Primary verification for this fork is the WebAssembly build:

```bash
cd build-wasm.debug   # or build-wasm.release
emmake make -j8
```

For doc-only updates, run:

```bash
git diff --check AGENTS.md
```

If the build fails with Emscripten not found, ask the user to restart the session with:

```bash
source ~/src/emsdk/emsdk_env.fish
```

### Recreating Build Directory (if necessary)

Only use this if the build directory is corrupted or missing. Confirm with the user first.

**Debug:**
```bash
rm -rf build-wasm.debug
mkdir -p build-wasm.debug
cd build-wasm.debug
emcmake cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -D__OGRE_HAVE_SSE=1 \
  -DOGRE_CONFIG_ENABLE_PVRTC=1
emmake make -j8
```

**Release:**
```bash
rm -rf build-wasm.release
mkdir -p build-wasm.release
cd build-wasm.release
emcmake cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -D__OGRE_HAVE_SSE=1 \
  -DOGRE_CONFIG_ENABLE_PVRTC=1
emmake make -j8
```
