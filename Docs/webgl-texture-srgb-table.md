# WebGL/GLES2 sRGB Texture Handling (Emscripten)

This table summarizes how OGRE's GLES2 RenderSystem (used by Emscripten/WebGL) maps
"hardware gamma" (sRGB decode on sampling) for common texture formats.

Legend: "hwGamma" is enabled by the material script `texture ... gamma` or via
`TextureUnitState::setHardwareGammaEnabled(true)`.

| Source texture | hwGamma -> sRGB decode? | GLES2 internal format when hwGamma = true | Notes |
| --- | --- | --- | --- |
| PNG (uncompressed RGB/RGBA) | Yes | `GL_SRGB8` / `GL_SRGB8_ALPHA8` | Mapped from `GL_RGB8` / `GL_RGBA8`. |
| DDS DXT1/3/5 (S3TC) | No | `GL_COMPRESSED_RGBA_S3TC_DXT*_EXT` | GLES2 mapping does not switch to sRGB DXT formats. |
| ASTC LDR | Yes | `GL_COMPRESSED_SRGB8_ALPHA8_ASTC_*_KHR` | OGRE uses the +0x20 sRGB ASTC offset. |
| ETC2 | No | `GL_COMPRESSED_*_ETC2*` (linear) | No sRGB remap for ETC2 in GLES2. |

## Where this is controlled in OGRE

- Material script `texture ... gamma` sets sRGB read (hardware gamma) in
  `OgreMain/src/OgreScriptTranslator.cpp`.
- The GLES2 internal format is selected in
  `RenderSystems/GLES2/src/OgreGLES2PixelFormat.cpp` (`getGLInternalFormat`).
- The chosen internal format is used in the actual GL upload in
  `RenderSystems/GLES2/src/OgreGLES2Texture.cpp`.

## Notes

- In WebGL/GLES2, sRGB support is capability-gated (see
  `RenderSystems/GLES2/src/OgreGLES2RenderSystem.cpp`). If sRGB is unsupported,
  `hwGamma` cannot be honored.
- For DDS DXT sRGB on WebGL, OGRE would need to map DXT formats to the
  `GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT*` enums when `hwGamma` is true.
- For ETC2 sRGB, OGRE would need explicit mapping to the sRGB ETC2 enums.
