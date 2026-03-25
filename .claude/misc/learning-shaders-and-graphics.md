# Learning Shaders & Graphics Programming

## Why learn shaders

Purpose-built rendering for unique games (water, simulations, novel effects) = shaders + math. Can't innovate by prompting AI for GLSL — the innovative part is the idea and understanding why the math works so you can bend it.

## Math you actually need

Not academic calculus. Just intuition for:
- **Vectors and matrices** — already using these daily with transforms
- **Dot product / cross product** — lighting, normals, projections. ~80% of shader math
- **Interpolation** — lerp, smoothstep, bezier. Foundation of everything looking good
- **Noise functions** — perlin, simplex, voronoi. Foundation of everything looking natural

That's it for the first year. No differential equations, no tensor algebra.

## Resources (ranked by usefulness)

1. **The Book of Shaders** — thebookofshaders.com
   Free, interactive, starts from zero. Write fragment shaders live in browser. Covers noise, patterns, shapes, color math. Do this first.

2. **Shadertoy** — shadertoy.com
   Not a tutorial but THE place to study. Find cool effects, read the code, break it, rebuild it. Inigo Quilez (iquilezles.org) is the god of real-time SDF/procedural rendering — his articles explain the math behind every shadertoy he's made.

3. **3Blue1Brown — Essence of Linear Algebra** (YouTube)
   15-video series that builds visual intuition for vectors/matrices. Watch once and dot products will never confuse you again.

4. **LearnOpenGL — Advanced sections** — learnopengl.com
   The advanced lighting + PBR sections: shadow mapping, SSAO, PBR, IBL, HDR/bloom. Each is a self-contained technique you can port to Realm.

5. **GPU Gems 1-3** — free online from NVIDIA
   Water simulation, subsurface scattering, fluid dynamics, terrain rendering. These are the "purpose-built systems" — older but gold.

## Complexity is layered, not exponential

- Basic Phong lighting: ~10 lines
- PBR (physically correct): ~40 lines (Phong but with better distribution functions)
- Add IBL (image-based lighting): another ~30 lines
- Each layer is digestible on its own
- Only looks insane when you see the final 200-line uber-shader without realizing it's 6 simple ideas stacked

## Water/fluid rabbit hole (progression)

1. Simple sine-wave vertex displacement (trivial)
2. Gerstner waves (just trig functions, looks shockingly good)
3. FFT ocean simulation (Tessendorf's paper)
4. SPH fluid particles
5. Eulerian grid-based fluid sim

Don't need to jump to the end. Gerstner waves in a vertex shader is a great stopping point that already looks impressive.

## General approach

- Book of Shaders first
- 3B1B linear algebra if math feels shaky
- Then port techniques from LearnOpenGL/GPU Gems into Realm one at a time
- Learning in your own engine > following tutorials in someone else's framework
