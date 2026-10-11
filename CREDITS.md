# Credits

Third-party software and assets used by Billiards Saloon, with their licences.

## Libraries

| Component | Use | Licence |
|-----------|-----|---------|
| [GLFW](https://www.glfw.org/) 3.4 | Window, input, OpenGL context | zlib/libpng |
| [GLAD](https://github.com/Dav1dde/glad) | OpenGL loader (generated, `external/glad`) | MIT / public domain (generated code) |
| [GLM](https://github.com/g-truc/glm) 1.0.1 | Vector and matrix maths | MIT |
| [nlohmann/json](https://github.com/nlohmann/json) 3.11.3 | Game data files | MIT |
| [RmlUi](https://github.com/mikke89/RmlUi) 6.3 | Menus and HUD (HTML/CSS-style UI) | MIT |
| [FreeType](https://freetype.org/) 2.13.3 | Font rendering for RmlUi | FreeType License (FTL) |
| [stb_image_write](https://github.com/nothings/stb) 1.16 | PNG screenshots (`external/stb`) | MIT / public domain |
| [stb_image](https://github.com/nothings/stb) 2.30 | Model textures (`external/stb`) | MIT / public domain |
| [cgltf](https://github.com/jkuhlmann/cgltf) 1.15 | glTF 2.0 model loading | MIT |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) 1.3 | Levels of detail for models | MIT |
| [miniaudio](https://miniaud.io/) 0.11.22 | Audio playback | Public domain / MIT-0 |
| [doctest](https://github.com/doctest/doctest) 2.4.12 | Unit tests (not shipped) | MIT |

## Sound

The 48 bundled sound samples are edited from downloaded pool recordings, material
foley, crowd recordings and interface sounds. Every source, author, licence,
edit recipe and file hash is listed in [assets/audio/CREDITS.md](assets/audio/CREDITS.md),
with licence texts in `assets/audio/licenses/`.

## Models

Furniture and decor in the saloon, from [Poly Haven](https://polyhaven.com/models), all
released under [CC0](https://creativecommons.org/publicdomain/zero/1.0/). The 1k glTF
versions were downloaded and their textures scaled to 512 px (`assets/models/props/`).

| Model | Authors |
|-------|---------|
| Arm Chair 01 | Kirill Sannikov |
| Bar Chair Round 01 | Dairon Sanchez |
| Coffee Table Round 01 | Ulan Cabanilla |
| Dartboard | Satyaki Mandal |
| Potted Plant 04 | James Ray Cock |
| Sofa 01 | Kirill Sannikov |
| Vintage Grandfather Clock 01 | Yann Kervran, James Ray Cock |
| Wall Clock | PierreB3D |
| Wine Barrel 01 | James Ray Cock |
| Wine Bottles 01 | Rico Cilliers, Jurita Burger |

## Fonts

| Font | Licence |
|------|---------|
| [Barlow and Barlow Condensed](https://github.com/jpt/barlow), © 2017 The Barlow Project Authors | SIL Open Font License 1.1 (`assets/fonts/OFL-Barlow.txt`) |

## References

Physics and rules references are listed in `docs/ARCHITECTURE.md`; AI references in `docs/design/INTELLIGENCE.md`.
