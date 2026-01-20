# SIMD Opportunities Survey (OGRE fork)

Date: 2026-01-20
Scope: quick scan outside existing `OgreOptimisedUtilSSE` coverage to identify SIMD-friendly hotspots.
Target: wasm SIMD + SSE-style intrinsics (as used in the recent commit).

## Executive summary
Most high-impact SIMD opportunities outside `OgreOptimisedUtilSSE` fall into two buckets:
1) **Pixel/image processing** (bulk conversions, gamma, resampling)
2) **CPU-generated geometry** (billboards and some static mesh preprocessing)

Culling and animation math could benefit too, but require more structural changes to be effective.

## Top opportunities (ranked)

### 1) Pixel conversions
**Files:** `OgreMain/src/OgrePixelFormat.cpp`, `OgreMain/src/OgrePixelConversions.h`

**Why:** `PixelUtil::bulkPixelConversion` and the per-pixel converters are scalar. The common 32-bit RGBA/BGRA/ARGB/BGRX conversions are bit-shuffle heavy and map well to SIMD.

**Estimated impact:** Medium (affects texture uploads, conversions, and some offline processing). Gains most visible when loading or converting large textures.

**Difficulty:** Medium.
- You can add SIMD-specialized paths for a small subset of frequently used formats (e.g., PF_A8R8G8B8 <-> PF_A8B8G8R8, PF_B8G8R8A8, PF_R8G8B8A8, and their X8 variants).
- Keep scalar fallback for other formats.

**Notes:**
- The conversion routing already uses a specialized conversion table; SIMD versions can be added as fast paths in `doOptimizedConversion` or within `bulkPixelConversion` for common pairs.
- wasm SIMD and SSE both support byte-level shuffles (with some differences), so consider a small abstraction layer if you want to share code.

---

### 2) Image resample + gamma
**Files:** `OgreMain/src/OgreImage.cpp`

**Why:** `Image::applyGamma` and `Image::scale` (nearest/linear resamplers) are tight per-pixel loops.

**Estimated impact:** Low–Medium (mostly asset load/processing rather than frame-time). Useful for large textures or tools.

**Difficulty:** Medium–High.
- Multiple pixel sizes (1,2,3,4,6,8,12,16 bytes) complicate SIMD.
- Linear resampler uses format-dependent logic; SIMD gives better ROI for 8-bit RGBA and float32 variants.

**Notes:**
- A good first pass is SIMD for `applyGamma` on 24/32-bit data via lookup table and vectorized loads/stores.
- For resample: focus on nearest/linear for 4-byte pixels.

---

### 3) Billboard generation
**Files:** `OgreMain/src/OgreBillboardSet.cpp`

**Why:** `injectBillboard`/`genQuadVertices` do per-billboard vertex generation and writes 4 vertices per billboard, often every frame for particle-heavy scenes.

**Estimated impact:** Medium–High in scenes with many billboards/particles.

**Difficulty:** Medium.
- You can SIMD-accelerate the common case (no per-billboard rotation, shared size, shared axes) by batching the 4-vertex position add and interleaving color/UV writes.
- Branchy modes (self-oriented, accurate facing, per-billboard rotation) reduce SIMD efficiency unless you separate paths.

**Notes:**
- Consider structuring two fast paths: (1) shared offsets, no rotation; (2) per-billboard offsets with shared axes.

---

### 4) Tangent space computation
**Files:** `OgreMain/src/OgreTangentSpaceCalc.cpp`

**Why:** The tangent/binormal accumulation and normalization are vector-heavy per-vertex operations. However, the algorithm has branching and split-vertex logic.

**Estimated impact:** Low runtime / Medium offline (mesh processing).

**Difficulty:** High.
- SIMD benefits are limited without refactoring data layout to SoA and batching triangles/vertices.

---

### 5) Static geometry bounds
**Files:** `OgreMain/src/OgreStaticGeometry.cpp`

**Why:** `calculateBounds` transforms positions and reduces min/max over all vertices. Purely data-parallel.

**Estimated impact:** Low runtime / Medium build-time.

**Difficulty:** Medium.
- SIMD can accelerate transform and min/max reduction, but you still have per-vertex matrix/quaternion math.

---

### 6) Culling / frustum tests
**Files:** `OgreMain/src/OgreFrustum.cpp`, `OgreMain/src/OgreSceneManager.cpp`

**Why:** `Frustum::isVisible` runs per object. SIMD helps only if you batch AABBs (SoA layout), not as much in the current per-object API.

**Estimated impact:** Medium–High for many objects, but only if you change data flow.

**Difficulty:** High (requires structural changes).

---

### 7) Skeleton/bone transforms
**Files:** `OgreMain/src/OgreSkeleton.cpp`, `OgreMain/src/OgreBone.cpp`

**Why:** Per-bone quaternion + vector math in `_getBoneMatrices` / `_getOffsetTransform`.

**Estimated impact:** Medium for CPU-skinned scenes.

**Difficulty:** High.
- Current AoS math and virtual calls limit SIMD benefits.
- Would need SoA or batch-friendly math types.

## Quick recommendations
1) **Start with pixel conversion SIMD**: self-contained, clear ROI, limited risk.
2) **Add a fast-path for billboard generation**: high payoff in particle-heavy scenes, keep scalar fallback for complex modes.
3) **Optional: gamma SIMD**: easy win for 24/32-bit data.

## Risk notes
- SIMD should not alter results in ways that change rendering output (e.g., rounding differences). Validate on reference scenes.
- wasm SIMD uses v128 ops; SSE intrinsics are acceptable with `-msimd128 -msse4.2` but may need care for shuffles.

