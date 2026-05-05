# Sy Hair Materials {#Sy-Hair-Materials}

`Media/SyHair` contains a pragmatic dual-pass material path for alpha-card hair,
especially Reallusion / Character Creator style assets that already carry the
hair mask in the diffuse texture alpha channel.

The default material is `SyHair/CC5_MaskedFringe`.

For shader math, tuning rationale, and debugging recipes, see
@ref Sy-Hair-Techniques.

@tableofcontents

# Render Model

The material renders the same hair cards twice:

- `Core`: alpha-clipped, `depth_write on`, `scene_blend one zero`
- `Fringe`: narrow alpha band only, `depth_write off`, `scene_blend src_alpha one_minus_src_alpha`

The core pass makes the dense part of the hairstyle behave like opaque cutout
geometry, so most self-occlusion goes through the depth buffer. The fringe pass
adds back soft edge coverage without making the whole hairstyle transparent.

This intentionally avoids relying on per-triangle transparent sorting inside a
single `SubEntity`. OGRE sorts transparent objects at renderable granularity,
not per hair card inside one draw.

# Resource Setup

Use the normal OGRE media setup, or add this location yourself:

```cpp
Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
    "Media/SyHair", "FileSystem", "General");
```

`Media/Main` must also be available because the shaders include
`OgreUnifiedShader.h`.

# Materials

Use the CC5 reference material directly if your texture filenames match:

```cpp
subEntity->setMaterialName("SyHair/CC5_MaskedFringe");
```

For real assets, prefer a child material per hairstyle:

```cpp
material MyAvatar/Hair : SyHair/Base_MaskedFringe
{
    set $DiffuseAlpha my_avatar_hair_diffuse.png
}
```

For first bring-up, use only the core pass:

```cpp
material MyAvatar/HairCoreOnly : SyHair/Base_CoreOnly
{
    set $DiffuseAlpha my_avatar_hair_diffuse.png
}
```

`SyHair/Placeholder_MaskedFringe` uses a small built-in texture and exists only
to verify that the resource locations and shaders load.

# Feature Defaults

SyHair defaults to the Character Creator 5 export shape: diffuse RGB plus alpha
coverage mask. Normal maps are disabled at compile time because CC5 hair export
does not generate hair normals.

Fringe dither and blue noise are also disabled at compile time. In testing,
dither made the fringe look thinner and darker because stochastic discard is
applied before the already-scaled fringe alpha. Blue noise did not improve the
default look enough to justify packaging another texture.

# Tuning

Start with the defaults:

- `uAlphaClip = 0.55`
- `uFringeMin = 0.20`
- `uFringeMax = 0.55`
- `uFringeAlphaScale = 0.45`

If the hairstyle still looks transparent or soupy:

- increase `uAlphaClip`
- increase `uFringeMin`
- reduce `uFringeAlphaScale`

If the hairstyle looks chunky or shaved:

- decrease `uAlphaClip`
- lower `uFringeMin`
- increase `uFringeAlphaScale` slightly

If edges sparkle, keep dither disabled and reduce fringe contribution with
`uFringeAlphaScale` or a higher `uFringeMin`.

If the core edge looks too hard:

- enable MSAA on the render target
- keep `alpha_to_coverage on`
- keep `uUseA2C = 1.0`

If MSAA is disabled or alpha-to-coverage behaves poorly on the target backend,
set `uUseA2C = 0.0`; the core pass will still write stable depth.

# Caveats

- Do not turn `depth_write off` on the core pass.
- Do not make the whole hairstyle alpha blended again.
- Normal maps, dither, and blue noise are compile-time opt-in experiments, not
  default material features.
- This is a cheap anisotropic-ish shader, not physically correct strand
  rendering or order-independent transparency.
