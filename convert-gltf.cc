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

#include <cstdio>
#include <cstring>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "asset.h"
#include "str-id.h"

#define FAILURE(FMT, ...) do {			\
    fprintf (stderr,				\
	     "\n"				\
	     "    error: " FMT "\n"		\
	     "\n"				\
	     , ##__VA_ARGS__);			\
    std::exit(-1);				\
  } while (0)					\
  // End of multi-line macro

IDDB iddb;

// This is a specialized parser designed to convert a single-mesh glTF as
// exported by blender.
static void staticMeshFromGLTF(asset::StaticMeshData    *meshData,
			       asset::StaticVertexData  **vertexData,
			       uint16_t                 **indexData,
			       char const               *gltfPath)
{
  cgltf_options options { };
  cgltf_data *data { NULL };

  if (cgltf_result_success != cgltf_parse_file(&options, gltfPath, &data)) {
    FAILURE("Failed to parse glTF file: %s",
	    gltfPath);
  }
  

  if (data->meshes_count != 1) {
    FAILURE("Expected exactly 1 mesh in this file, got %zu",
	    data->meshes_count);
  }

  cgltf_mesh *mesh = data->meshes;

  if (data->meshes[0].primitives_count != 1) {
    FAILURE("Expected exactly 1 primitive in this mesh, got %zu",
	    data->meshes[0].primitives_count);
  }

  cgltf_primitive *primitive = mesh->primitives;
  
  if (data->meshes[0].primitives[0].attributes_count != 4) {
    FAILURE("Expected exactly 4 attributes for this mesh, got %zu",
	    data->meshes[0].primitives[0].attributes_count);
  }

  cgltf_attribute *attributes = primitive->attributes;

  cgltf_attribute *pos_attr     { nullptr };
  cgltf_attribute *norm_attr    { nullptr };
  cgltf_attribute *uv_attr      { nullptr };
  cgltf_attribute *tangent_attr { nullptr };

  for (int i = 0; i < 4; i++) {
    if (!strcmp("POSITION", attributes[i].name)) {
      pos_attr = attributes + i;
    } else if (!strcmp("NORMAL", attributes[i].name)) {
      norm_attr = attributes + i;
    } else if (!strcmp("TEXCOORD_0", attributes[i].name)) {
      uv_attr = attributes + i;
    } else if (!strcmp("TANGENT", attributes[i].name)) {
      tangent_attr = attributes + i;
    }
  }

  if (pos_attr == nullptr) {
    FAILURE("This mesh has no POSITION attribute");
  } else if (norm_attr == nullptr) {
    FAILURE("This mesh has no NORMAL attribute");
  } else if (uv_attr == nullptr) {
    FAILURE("This mesh has no TEXCOORD_0 attribute");
  } else if (tangent_attr == nullptr) {
    FAILURE("This mesh has no TANGENT attribute");
  }

  size_t vertexCount = pos_attr->data->count;

  if (norm_attr->data->count != vertexCount) {
    FAILURE("sanity check: normal count (%zu) != position count (%zu)",
	    norm_attr->data->count,
	    vertexCount);
  } else if (uv_attr->data->count != vertexCount) {
    FAILURE("sanity check: uv count (%zu) != position count (%zu)",
	    norm_attr->data->count,
	    vertexCount);
  }

  size_t indexCount = primitive->indices->count;

  asset::StaticVertexData  *vertsOut =
    (asset::StaticVertexData *)malloc(vertexCount * sizeof(asset::StaticVertexData));
  uint16_t *indicesOut  =
    (uint16_t *)malloc(indexCount * sizeof(uint16_t));

  if (cgltf_result_success != cgltf_load_buffers(&options, data, gltfPath)) {
    FAILURE("Failed to load buffers for glTF file: %s",
	    gltfPath);
  }

  meshData->id           = iddb.getID(mesh->name);
  meshData->indexOffset  = 0;
  meshData->vertexOffset = 0;
  meshData->indexCount   = indexCount;
  meshData->vertexCount  = vertexCount;

  for (size_t i = 0; i < indexCount; i++) {
    size_t index = cgltf_accessor_read_index(primitive->indices, i);
    if (index > 0xFFFF) {
      FAILURE("This mesh has %zu as an index, which won't fit in a uint16_t!", index);
    }

    indicesOut[i] = (uint16_t)index;
  }

  if (pos_attr->data->is_sparse) {
    FAILURE("position data is sparse -- we can't handle this at the moment...");
  } else if (norm_attr->data->is_sparse) {
    FAILURE("normal data is sparse -- we can't handle this at the moment...");
  } else if (uv_attr->data->is_sparse) {
    FAILURE("uv data is sparse -- we can't handle this at the moment...");
  } else if (tangent_attr->data->is_sparse) {
    FAILURE("tangent data is sparse -- we can't handle this at the moment...");
  }

  if (vertexCount > 0) {
    cgltf_accessor_read_float(pos_attr->data, 0, (float *)&meshData->bounds.min, 3);
    meshData->bounds.max = meshData->bounds.min;
  }

  for (size_t i = 0; i < vertexCount; i++) {
    cgltf_accessor_read_float(pos_attr->data, i, (float *)&vertsOut[i].position, 3);
    cgltf_accessor_read_float(uv_attr->data, i, (float *)&vertsOut[i].uv, 2);
    cgltf_accessor_read_float(norm_attr->data, i, (float *)&vertsOut[i].normal, 3);
    cgltf_accessor_read_float(tangent_attr->data, i, (float *)&vertsOut[i].tangent, 3);

    meshData->bounds.max = glm::max(meshData->bounds.max, vertsOut[i].position);
    meshData->bounds.min = glm::min(meshData->bounds.min, vertsOut[i].position);
  }

  *vertexData = vertsOut;
  *indexData  = indicesOut;

  cgltf_free(data);
}

int main(int argc, char const *argv[]) {
  if (argc != 3) {
    char const *strippedName = strrchr(argv[0], '/');

    fprintf(stderr,
	    "\n"
	    "    error: got %d arguments\n"
	    "\n"
	    "    usage: %s <gltf filename> <output filename>\n"
	    "\n",
	    argc - 1,
	    strippedName ? strippedName + 1 : argv[0]);

    std::exit(-1);
  }

  iddb = IDDB::fromFile(".iddb");

  asset::StaticMeshData   meshData;
  asset::StaticVertexData *vertexData;
  uint16_t                *indexData;

  staticMeshFromGLTF(&meshData, &vertexData, &indexData, argv[1]);

  iddb.write(".iddb");

  asset::writeMeshFile(argv[2], &meshData, 1, vertexData, meshData.vertexCount, indexData, meshData.indexCount);

  auto handle = asset::openMeshFile(argv[2]);

  // std::cout << handle << std::endl;

  asset::closeMeshFile(handle);

  return 0;
}
