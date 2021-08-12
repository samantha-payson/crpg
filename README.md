# Graphics Engine in Vulkan

This repo contains a work-in-progress graphics engine in Vulkan, intended for
use with a game (or some other application that involves rendering large numbers
of skinned and static meshes).

The project is called CRPG because I might someday want to make a simple
"Computer RPG" game with it.

A lot of the base code is based on ideas from [vkguide.dev](), but I have made
significant modifications and expansions, as well as deviating from their
designs in many ways.

I've implemented this on a laptop running Arch Linux with integrated graphics
that Vulkan reports as 'Intel(R) HD Graphics 620 (KBL GT2)'. The Makefile will
probably not work on Windows or Mac without some modification, but I'm sure you
could make it work with minimal effort.

The only external dependencies are Vulkan 1.2, SDL2 and the zstd compression
library. It might be possible to run this code with a lower version of Vulkan if
you just reduce the version number in `gfx.cc`.

## Current State

Right now the engine can load and render -- and please save your applause for
the end -- a single, slowly-rotating, static mesh.

There is also a custom mesh format with a converter for single-mesh glTF 2.0
files exported from Blender. It does not support general glTF 2.0 files, nor is
there any plan to -- this was just the easiest way to mesh data from Blender and
will probably facilitate loading from most other 3D programs if that ever
becomes desirable.

## Future Plans

Planned work includes, in order from sooner to later:

  1. An expanded asset manager that allows loading static meshes and textures
     from a library, given an identifier.
	 
  2. Fast static mesh rendering via multiDrawIndirect.

  3. A camera abstraction, supporting simple first-person fly-around navigation.

  4. Text rendering for a HUD/overlay.

  5. GPU-side vertex-skinning skeletal animation, with support in the asset
     manager.

  6. A (very) simple UI/menu system
  
  7. Some kind of toy game?
  
## License

The files `vk_mem_alloc.h` and `cgltf.h` in the`include` directory are part of
the [Vulkan Memory Allocator][VMA] library and [cgltf][cgltf] library
respectively. Both files contain their own copyright notices.

[VMA]:   https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
[cgltf]: https://github.com/jkuhlmann/cgltf

Otherwise, this project is released under a 3-clause BSD license, visible at the
top of all source files and reproduced below:

```cpp
/*-
 * Copyright (c) 2021 Samantha Payson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
  */
 ```
