# chapter03/gltf asset — rubber duck

`chapter03_gltf` loads its mesh from this directory at runtime:

```
assets/chapter03/rubber_duck/scene.gltf   (+ scene.bin and texture(s) it references)
```

The path is resolved from the `COOKBOOK_ASSET_DIR_STRING` compile define
(`<repo>/assets/`, set in `cmake/Interfaces.cmake`), so the binary finds it regardless of
the working directory.

## Where to get it

This is the rubber-duck glTF used by *Vulkan 3D Graphics Rendering Cookbook (2nd ed.)* —
its companion repository ships it under `data/rubber_duck/`:

> https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition

The model files (`scene.gltf`, `scene.bin`, and the texture image(s) it references) are
committed here for convenience, sourced from the Packt companion repository linked above.
Any other glTF 2.0 model works too — drop it here as `scene.gltf` (or change the path in
`src/chapter03/gltf/main.cxx`), preserving the relative URIs inside `scene.gltf` (the
texture is loaded relative to the `.gltf`).

## Running without the asset

If `scene.gltf` is missing, the exercise builds fine but exits at startup with:

```
[glTF] : Error : failed to parse [.../assets/chapter03/rubber_duck/scene.gltf]
```

Drop the files in and re-run.
