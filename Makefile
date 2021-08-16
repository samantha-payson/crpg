PKG_DEPS=sdl2 vulkan libzstd
PKG_LIBS=$(patsubst %,$$(pkg-config --libs %),$(PKG_DEPS))
PKG_CFLAGS=$(patsubst %,$$(pkg-config --cflags %),$(PKG_DEPS))

CXXFLAGS = $(shell echo $(PKG_CFLAGS)) \
           -Wall \
           -std=gnu++17 \
           -g -O0 \
           -Iinclude \
           -Wno-reorder-init-list \
           -Wno-unused-private-field \

LDFLAGS=$(shell echo $(PKG_LIBS))

BINARIES = convert-gltf crpg
BINFILES = $(patsubst %,bin/%,$(BINARIES))

CCFILES = gfx.cc vma.cc asset.cc
OFILES  = $(patsubst %.cc,.obj/%.o,$(CCFILES))

SHADERFILES = triangle.vert triangle.frag static-mesh.vert static-mesh.frag
SPIRVFILES = $(patsubst %,.data/%.spv,$(SHADERFILES))

MESHFILES = cube.mesh monkey.mesh fancy-cube.mesh

MESHDATA = $(patsubst %,.data/%,$(MESHFILES))

all: shaders meshes $(BINFILES)

bin/%: .obj/%.o $(OFILES)
	@ mkdir -p bin
	@ echo "    [LD]         $@"
	@ clang++ $(LDFLAGS) $(CXXFLAGS) $^ -o $@

# The Vulkan Memory Allocator project needs some special flags to avoid
# complaining...
.obj/vma.o: vma.cc
	@ mkdir -p .obj
	@ echo "    [C++]        $<"
	@ clang++ $(CXXFLAGS) -Wno-nullability-completeness -Wno-unused-variable \
                  -c $< -o $@

.obj/%.o: %.cc
	@ mkdir -p .obj
	@ echo "    [C++]        $<"
	@ clang++ $(CXXFLAGS) -c $< -o $@

meshes: $(MESHDATA)

.data/%.mesh: assets/%.glb bin/convert-gltf
	@ echo "    [CONVERT]    $<"
	@ ./bin/convert-gltf $< $@ .data/library.assets

.data/%.spv: shaders/% .data
	@ echo "    [GLSL]       $<"
	@ glslc $< -o $@

shaders: $(SPIRVFILES)

run: crpg shaders
	@ echo "    [RUN]        $<"
	@ ./crpg

.obj:
	@ echo "    [MKDIR]      $@"

.data:
	@ echo "    [MKDIR]      $@"
	@ mkdir -p .data

clean:
	@ echo "    [CLEAN]"
	@ rm -f *.spv crpg $(BINFILES)
	@ rm -rf  .obj .data

.PHONY: shaders
.PRECIOUS: .obj/%.o
