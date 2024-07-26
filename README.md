# RT64
RT64 is an N64 graphics renderer for playing games with enhancements in emulators and native ports.

# Work in Progress

### **Emulator Support (Plugin) and Ray Tracing (RT) are not available in this repository yet.**

This repository has been made public to provide a working implementation to native ports that wish to use RT64 as their renderer.

**Development of these features is still ongoing and will be added to this repository when they're ready.** Thank you for your patience!

# Features available
* Modern N64 renderer built on the latest APIs (D3D12 and Vulkan).
* Uses ubershaders to guarantee no stutters due to pipeline compilation.
* Brand new architecture designed around offering novel enhancements.
* High level of accuracy and no tolerance for game-specific workarounds to achieve correct rendering.
* Input latency reduction options to either skip the game's native buffering or draw as early as possible.
* Render with a higher resolution and downsample to a resolution closer to the original game.
* Support for Widescreen with arbitrary aspect ratios, including Ultrawide support (limited game support).
* Interpolate the game's visuals to 60 FPS or above (HFR) by generating new frames and modifying them in 3D space (limited game support).
* Extended command set for better integration of widescreen, interpolation and path tracing features (for use with rom patches, rom hacks, and ports).
* Texture packs with DDS and asynchronous streaming support. Compatible with Rice filenames. [Read about how to make texture packs for RT64 here](TEXTURE-PACKS.md).
* Supports Windows 10, Windows 11 and Linux.

# Features in development (in priority order)
* Game script interpreter.
  * Support a runtime language for configuring the path traced renderer based on the contents of the game's memory.
  * Support patching the game's memory to provide various enhancements automatically integrated with the game script.
* Fully path traced renderer (RT).
  * Calculate all lighting in real time and replace the contents of the drawn scene entirely with a path traced version.
  * Provide support for extra modifications for altering the material properties of the surfaces in the game.
  * Game support will be limited to a very small selection of games initially.
* Emulator integration.
  * Game compatibility database and feature whitelist.
  * Configuration screen.
  * List of supported emulators to be determined.
* Model replacements.
  * Details to be determined.


# Building

* CMake 3.20 or above.
* C++17 compiler.
  * Known to work with Microsoft Visual C++, Clang or GCC.
* Windows 10 or 11 SDK (Windows only).
* Make sure to clone submodules correctly when checking out the repository.
  * Use `git submodule update --init --recursive` if you cloned without using `--recursive`.

# Architecture

## Deferred frames
Draw calls are never sent right away to the GPU but instead stored on an auxiliary structure that logs the history of an entire frame. A frame's contents can be examined in great detail with the in-game debugger included with RT64. The renderer requires this to be able to optimize the rendering process, perform enhancements, replace assets and even generate new frames entirely.

## Deferred RDP
Any operation related to reading memory and using it as texture information during rendering is completely deferred until the game requires a full synchronization of the RDP. All requests are stored in a list of operations and flushed before it is time to render. This allows RT64 to perform extra optimizations and detect when it is necessary to synchronize the output of the RDP back to memory. It also opens up the ability to detect when direct copies can be made on GPU memory and even detect patterns that can be replaced with equivalent but faster operations.

## Deferred RSP (Compute)
All vertex transformations by the RSP (e.g. position, lighting, texturing, etc.) are performed by a highly parallel compute shader in the GPU. This saves a lot of CPU processing time and allows for much higher vertex and polygon counts. Since all transformations are deferred to this step, it's very easy for RT64 to patch transformations of the objects in the scene and the camera and produce a new frame very quickly. This effectively removes CPU bottlenecks that are critical for reaching very high target framerates.

## Texture Decoder (Compute)
RT64 does not decode textures on the CPU and instead opts for uploading TMEM (4 KB) directly to the GPU. If possible, a RGBA32 version of the texture will be decoded and cached using a compute shader. If the sampling parameters prove to be too troublesome for that (e.g. giant texture masks with no clamp due to bad configuration), then RT64 can sample TMEM directly just like the console with a small performance sacrifice.

RT64 features one of the most accurate TMEM loaders to date so far which has been directly reverse engineered by observing console behavior with the aid of homebrew test ROMs developed by [Wiseguy](https://github.com/Mr-Wiseguy). All the color conversion formulas for decoding have also been sourced from [Tharo](https://github.com/Thar0)'s excellent RDP research.

## Dual renderers
RT64 takes advantage of the multi-threaded capabilities of modern APIs to run two renderers at the same time with highly different goals. One renderer draws at native resolution (e.g. 240p) and synchronizes back immediately with the game running at its original rate. The other renderer replays all the draw calls detected by the main renderer at higher resolution and even at a different rate when using interpolation. This design guarantees that the game will see the correct data in RAM, which is very important for games that read from these memory regions for gameplay reasons. If it's not required by the game, this native resolution renderer can be turned off completely to save performance.

## Framebuffer detection
RT64 will keep track of any memory addresses used as framebuffers, their dimensions and their matching contents that live in GPU memory. If other operations in the frame load and sample a part of memory owned by a framebuffer, then a direct copy of one of its regions is performed and stored in memory.

The framebuffer copy mechanic is only used when the load and sample parameters line up correctly and the final result would look no different than if it was sampled from RAM (except for preserving the high precision of the image). As framebuffer detection works at the RDP level, that means all related performance and visual enhancements can apply even in LLE mode.

## Framebuffer upscaling
When framebuffer operations are detected, the renderer is able to upscale these effects by increasing the resolution of all framebuffers and any associated region copies by a scaling factor. The resolution increase is only performed in the high resolution renderer. All texture coordinates are adjusted automatically to account for this change.

## Framebuffer reinterpretation
Games will sometimes draw to a framebuffer using one format and then read it as another format to perform post-processing on the screen. RT64 supports this operation natively at high resolution using per-pixel reinterpretation inside a dedicated compute shader. While this requires adding specific paths encountered on a case-by-case basis (to preserve high precision), the framework can be easily extended to support as many cases as possible. This feature is essential for emulating some of the more infamous effects in higher resolutions such as greyscale filters.

## Draw call matching
Once all information for a particular game frame has been recorded, RT64 will attempt to perform automatic matching of draw calls between frames based on their rendering parameters, the textures in use, their position, orientation, their tracked velocities and much more. Matching is essential to generating interpolated frames and providing motion vector information to the path traced renderer. Draw call matching is still a highly experimental area that is prone to errors and requires a lot of processing to get right. The algorithms behind this feature will be likely to change as more research is done to accommodate as many games as possible.

## Frame interpolation
New frames are generated using the information from the previous draw call matching step. For any calls that match, new 3D transformations will be created via interpolation and uploaded to the GPU for the deferred RSP to do its work again. When possible, transformations are decomposed into their constituent components. Those components are interpolated separately and recomposed into the resultant transform to prevent interpolation artifacts caused by naive transformation interpolation. This interpolation can also be applied to texture scrolling coordinates and even per vertex if vertex velocities are computed. More parameters are planned to be supported in the future as long as they're part of the recorded frame data.

## Extended command set
RT64 offers an extended command set that can be used by native ports, ROM patches, and ROM hacks to provide extra information that the renderer can use to enhance the final result. Using this feature is vital to providing accurate frame interpolation and widescreen enhancements for 2D elements. If you're interested in using this feature, check out the [included header in the repository](https://github.com/rt64/rt64/blob/main/include/rt64_extended_gbi.h). The main activation method is encoded into a NOOP command so it should be safe to run it in real hardware. RT64 will recognize this command and enable the extended command set feature, allowing for an extended repertoire of functions aimed at fixing some common grievances that show up at high framerates or wide aspect ratios.

For example, objects that rely on pixel depth checks to draw can instead use the `gEXVertexZTest()` command to specify any triangles drawn after the command should only be drawn if the vertex is not covered by something else. This means that effects such as light halos can be replaced with this command and it'll automatically work on widescreen ratios and no longer be delayed by one frame. Instead, RT64 will just flush the depth buffer at the correct step and run a small compute shader that will invalidate the draw call's triangles if the depth test doesn't pass. Though the original effect already works as-is thanks to the native renderer, the extended GBI method can be leveraged to make the experience feel even better.

## Future texture and asset replacement
Texture replacements are easily performed at runtime by using two capabilities that RT64 already supports: bindless textures and texture coordinate scaling. The entire renderer uses only one contiguous array of texture resources and replacing one of these by an HD texture replacement is straightforward. By detecting the difference in sizes between the original texture and the replacement, the texture coordinates can be scaled easily with the same logic that is used for framebuffer tiles.

Asset replacement, while not currently implemented and more of a long term feature, will leverage the fact that only one contiguous vertex and index array is used by RT64. This allows for highly efficient rendering as it minimizes the amount of times vertex and index buffers must be bound, but it also means the RSP can process the vertices placed in this buffer during rendering time as if they came from the game itself. Therefore, the entire process of model replacement will just consist of allocating chunks of the buffer directly for replacements in a native format, assigning it the correct transforms used by the draw call and letting the RSP compute shader do its job. This will allow the renderer to draw models that have orders of magnitude higher triangle counts with very little additional cost to the CPU.

## Future path tracing
RT64 will attempt to perform scene detection by merging as many draw calls as it can as long as they're inside of the same compatible perspective view point. Since all data remains untransformed, bottom-level acceleration structures (BLAS) can be built by using a compute shader to transform vertices to world space instead of screen space. Once these BLAS are built, a top-level acceleration structure (TLAS) is constructed with all the BLAS that were detected.

These structures are used directly for path tracing the scene and generating all lighting in real time. Lighting information is derived from various sources (the game itself, scripts, integrated light editor) and will be detailed more in the future as this feature is closer to a final state.
