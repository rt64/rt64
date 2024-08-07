# RT64 Texture Packs

RT64 introduces a texture pack system which gives some extra flexibility for artists compared to previous offerings by other emulators. This document provides a detailed explanation of how the texture pack system works, the configuration file format, and the tools available for creating and managing texture packs.

### This document is aimed at creators who wish to make texture packs for a game. **These are not instructions for end users**.

## Texture Pack Tools
Download the latest version of the texture tools that will be required for making a texture pack. 

[Texture Pack Tools Release](https://github.com/rt64/rt64/releases/tag/texture_pack_tools).

You can also build these tools yourself by building RT64.

## Overview

An RT64 texture pack consists of:
- Image files in **DDS** or **PNG** format.
  - **DDS** is the first-class format supported and expected by RT64.
    - Offers very efficient and modern compression formats such as BC7 which will drastically reduce the VRAM requirements by **almost four times** compared to PNG.
    - Supports mipmaps, which allow the renderer to offer high quality [anisotropic filtering](https://en.wikipedia.org/wiki/Anisotropic_filtering), greatly improving the final look of the textures in the game and avoiding most of the problems commonly associated with HD texture packs such as shimmering.
    - You can look into using automatic conversion tools such as [Compressonator](https://gpuopen.com/compressonator/) or [Texconv](https://github.com/Microsoft/DirectXTex/wiki/Texconv).
      - Note: Compressonator's GPU Encoder mode has a bug at the moment where precision of the colors are lost (e.g. full white values won't result in 1.0 channels), leading to incorrect values in some textures where accuracy is critical.
  - **PNG** is supported for the sake of convenience and being able to work with source files during the development of a texture pack.
    - **Do not ship texture packs using PNG files to end users**. Their experience will be significantly impacted by having to decompress these files at runtime and have much higher memory consumption.
- A database configuration JSON file (`rt64.json`) that maps texture files to the texture hashes identified by RT64 at runtime.

## Configuration File

The configuration file (`rt64.json`) follows this format:

```
{
    "configuration": {
        "autoPath": "rice",
        "configurationVersion": 2,
        "hashVersion": 2
    },
    "operationFilters": [
        {
            "wildcard": "UI/Text/*",
            "operation": "preload"
        },

        ...

    ],
    "textures": [
        {
            "hashes": {
                "rice": "61094ee7#4#1",
                "rt64": "0001d0556bc980b5"
            },
            "path": ""
        },
        {
            "hashes": {
                "rice": "20fff290#4#1",
                "rt64": "000226fbda4cecff"
            },
            "path": ""
        },

        ...

    ]
}
```

### Configuration

- **autoPath**: Specifies the hash format used for automatically searching texture files inside the pack. Auto paths are only used if a path is not specified for a texture in the database.
  - **rice**: One of the most common formats supported by many N64 emulators. Note that RT64 cannot produce Rice hashes at runtime. Use this if you have a texture pack already made for other emulators.
  - **rt64**: The hash from RT64 as a file name. No texture packs use this yet and it's unlikely a texture pack author would prefer to use this instead of naming the files manually.
- **configurationVersion**: The version of the configuration file format.
- **hashVersion**: The version of the hash algorithm used by RT64.

### Operation Filters

Operation filters are optional filters that define how textures should be loaded based on their names. This can be handy if your files are well organized, as it's easier to set the operation you want using filters instead of having to add an entry for each file separately.

- **wildcard**: Pattern to match texture paths. Use * to indicate any number of characters (including *zero*) and ? to indicate any *one* character. Some examples include:
  - "Terrain/Grass001.*" will include any file called "Grass001" inside the directory "Terrain".
  - "Text/*" will include any files inside the directory "Text".
  - "\*/Button\*" will include any files that start with "Button" inside any directory.
  - "UI/Letter?.*" will include any files called Letter with exactly one character that can be anything (LetterA, LetterB, etc.) inside the directory "UI".
- **operation**: Determines the loading behavior. Possible values are:
  - **stream**: Textures are loaded asynchronously, potentially causing visual pop-in. This is **solved by generating a low mipmap cache** via the `texture_packer` tool. This is the default behavior. 
  - **preload**: Textures are loaded at the start of the game and will remain in memory, preventing any sort of pop-in at the expense of memory usage and increased initial loading times. Only recommended for textures where no kind of pop-in is tolerable, even when using the low mipmap cache.
  - **stall**: Stops the renderer until the texture is loaded. Can be useful for instances where loading screens are known to appear. **Not recommended**.

Filters are processed in the order they appear. Keep this in mind to resolve any potential conflicts between the filters. Feel free to ignore this feature until you feel the need to control the loading behavior of the textures in more detail.

### Textures

Each texture entry can map hashes together as well as an optional file path.

- **hashes**: The hash that identifies a texture uniquely according to each algorithm such as Rice or RT64.
- **path**: An optional path to the texture file relative to the root of the texture pack.
  - A filename extension does not need to be included and will be ignored if specified.
  - RT64 attempts to load the DDS version of a texture first, and the PNG version later if a DDS file can't be found.
  - Both forward and backward slashes are accepted and should work independently of the platform.
  - The texture file does not need to follow a particular naming format.
  - If empty, the auto path scheme will be used to try to find the file instead.
 
## In-Game Replacement

It is possible to replace textures by using the debugger while running RT64.

1. Enter developer mode (press F1 after having enabled developer mode for RT64 in the host application via its configuration file).
2. Go to the textures tab.
3. Load the texture pack directory.
4. Right-click to bring up a drop down list of elements in the screen. You can also find the element by scrolling in the debugger and hovering over draw calls.
5. It is recommended to use the "Pause" button in the debugger while doing this to freeze the frame and avoid draw calls changing around.
6. Expand a texture tile to see its information. Click on the "Replace" button.
7. On the file dialog that gets brought up, select the texture to use. **This texture must be inside the texture pack directory already**.
8. Save your changes by going back to the "Textures" tab and clicking the "Save directory" button.

It's not possible to make these type of replacements while using a pack in `.rtz` format: you must only load one directory at a time.

## Hashing Schemes

RT64 uses its own hashing scheme based on using [XXH3](https://github.com/Cyan4973/xxHash) on the bytes from `TMEM`, the 4 KB of texture memory used by the N64, which is known to be much more accurate at identifying textures. It also supports the Rice hash format for compatibility with existing texture packs. Note that RT64 cannot produce Rice hashes at runtime due to the limitations and inaccuracies of the Rice algorithm.

### Do I Need To Generate Hashes?

If the game you're trying to mod already has a texture pack available for RT64, then the most likely answer is no. You can just grab the database file off a texture pack someone has created already. **It is possible to include a database file without having all the files defined inside it**.

If you wish to shrink the size of a database to its essentials to improve the loading time of the texture pack, you can use the "Remove Unused Entries" button in Developer Mode in the Textures tab. This will remove any entries for textures that could not be found inside the texture pack directory.

If you plan to just reuse a database file created by someone else, you can skip ahead to the ["Create Pack"](#create-pack) section.

### History and Limitations of Rice Hashes

The first graphics plugin to support texture dumping for N64 emulators was Rice. However, the Rice hashing algorithm is inaccurate and inefficient, often relying on reading unrelated memory. This can cause the hash to break if the asset changes its location in `RDRAM`. Additionally, the code for Rice is licensed under GPL, making it incompatible with RT64's licensing.

Unfortunately, Rice is pretty much the standard for most of the texture packs that have been released over the years, which leaves no choice but to come up with a tool to automate the process. RT64 includes a separate tool licensed under the GPL to work around the problems mentioned above.

### Generating Hashes

To automate hash generation, use the `texture_hasher` tool included in the texture pack tools zip file. This tool processes [texture dumps created by RT64](#texture-dumps) and generates matching hashes for both RT64 and Rice formats. Note that a 1:1 correspondence might not always be possible due to errors in the original implementation of Rice. If automatic matching is not feasible, you can manually specify the name of the texture with the RT64 hash or do the replacement manually inside RT64 for convenience. Do not attempt to assign a Rice hash manually, as it is not possible to rely on this algorithm to get unique usable values anymore and you are likely to run into collisions.

```powershell
./texture_hasher <dump_directory> --rice
```

An rt64.json file will be generated. If a file already exists, the database will be updated with the information from the dump. It is possible to do partial upgrades to a database by only dumping the textures that are missing from a database and running the tool to process them.

#### Texture Dumps

Generate a texture dump by:

1. Enter developer mode (press F1 after having enabled developer mode for RT64 in the host application via its configuration file).
2. Go to the textures tab.
3. Start a dump to a directory by clicking on the button.
4. Play the game and try to encounter as many instances of unique textures as you can.

The texture dump is not what is commonly known as a texture dump in other emulators, but rather includes raw memory dumps from `TMEM`, `RDRAM` and the necessary tile information for hash generation. These dumps are archivable and can be reused in the future if necessary.

> [!NOTE]  
> The current version of the dumper does not support making PNG versions out of the texture dumps because RT64 can only decode textures on the GPU. This will come in a future version of the tool to make it easier to identify the contents of the dumps.

## Creating Packs

The `texture_packer` tool is used to create distributable texture packs. It compresses all the textures available in the directory, including the `rt64.json` database file and the low mipmap cache, into a `.rtz` file. The pack itself is just a zip with a different file extension, but it is heavily recommended to use the tool as it'll use `zstd` as the compression algorithm, which is both really fast for decompression at runtime and achieves very good compression ratios. Users can load the texture pack with the resulting `.rtz` file without needing to extract it.

### Low Mipmap Cache
When using DDS texture files, it is **heavily recommended** to generate a low mipmap cache before creating the final pack. The cache will include all the low quality mipmaps extracted from the DDS textures in one big file that will be loaded at the start of the game. These textures will be used in place while the higher quality version loads, basically eliminating most visual pop-in that can happen while textures load in the background. Note that it will be necessary to regenerate this cache any time the contents of the DDS files are changed.

The cache can be generated with the following command.
```powershell
./texture_packer <texture_pack_directory> --create-low-mip-cache
```
You can test out the low mipmap cache in RT64 by loading the texture pack directory before creating the final pack file. You can use this to verify if the process worked successfully.

### Pack File
Creating the final `.rtz` file can be done with the following command. 
- If both DDS and PNG files for a particular texture exist, only the DDS file will be included in the `.rtz`.
- `zstd` compression is used by default. You can specify `--deflate` or `--store` if you wish to make the file compatible with more third-party archive tools at the expense of compression ratio and performance.
- `zstd` has been verified to perform faster than uncompressed file reading even in modern M2 SSD drives. There should be no performance benefit to leaving the files uncompressed (unless you use PNG files, **which you shouldn't ship to end users**).
```powershell
./texture_packer <texture_pack_directory> --create-pack
```
> [!NOTE]
> This process can take several minutes and can consume many of your system's resources depending on the amount of textures. You can specify how many threads you wish to use with the `--threads n` option. The default option will use all threads available on the system.

When finished, a `.rtz` file should be available with the same name as the directory. You can now load and play with this file directly on RT64 and you can ship this file to end users.

## Performance

- If all recommendations were followed correctly, end users should not experience stutters in most cases when playing with the texture pack.
- While it is recommended for end users to store the texture pack in the fastest storage available to them, texture packs have been verified to work without stutters even from mechanical hard drives.
- There are no memory requirements for end users to use a texture pack, as textures are just streamed in and kicked off the cache based on the user's available video memory.
- Preloaded textures and the low mipmap cache will permanently consume a chunk off this streaming pool. Make sure there's as few preloaded files as possible and that the low mipmap cache isn't too big (shoot for around 100 to 200 MB at most).
- Users can't mess with settings on how to load a texture pack or "pre-fetch" it. You can control this behavior with more granularity by configuring the operations filters correctly.
- You can monitor the memory usage of your texture pack by checking the "Render" tab while in developer mode.
