# OGRE Fork

This is a public fork of OGRE (Object-Oriented Graphics Rendering Engine).

- **Origin**: `git@github.com:syandus/ogre.git`
- **Upstream**: `https://github.com/OGRECave/ogre.git`

## Differences from Upstream

This fork contains the following modifications compared to `upstream/master`:

### 1. Expanded PVRTC Codec Support (`OgreMain/src/OgrePVRTCCodec.cpp`)

Significantly extended the PVR texture codec to support additional compressed texture formats:

- **ETC formats**: ETC1, ETC2_RGB, ETC2_RGBA, ETC2_RGB_A1
- **DXT/BC formats**: DXT1-5, BC4, BC5, BC6, BC7
- **ASTC formats**: Multiple block sizes (4x4 through 12x12)

The enum definitions were updated to match the PVRTexLib header format for better compatibility.

### 2. Android Shadow Texture Fix (`Samples/Character/include/CharacterSample.h`)

Changed shadow texture pixel format from `PF_DEPTH16` to `PF_BYTE_RGBA` as a workaround because `PF_DEPTH16` does not work correctly on Android.

### 3. Build Configuration

- Added build directories to `.gitignore` (`/build-wasm/` and others)
- Added VS Code workspace settings (`.vscode/settings.json`)

## Development

There are no tests.

### Building (WebAssembly)

Build directories are pre-configured. To build:

```bash
cd build-wasm.debug   # or build-wasm.release
emmake make -j8
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
