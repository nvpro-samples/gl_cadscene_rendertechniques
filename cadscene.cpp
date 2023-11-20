/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#include "cadscene.hpp"
#include <fileformats/cadscenefile.h>

#include <algorithm>
#include <assert.h>
#include <cstddef>
#include "glm/gtc/type_ptr.hpp"

#define USE_CACHECOMBINE 1


glm::vec4 randomVector(float from, float to)
{
  glm::vec4 vec;
  float     width = to - from;
  for(int i = 0; i < 4; i++)
  {
    vec[i] = from + (float(rand()) / float(RAND_MAX)) * width;
  }
  return vec;
}

static void recursiveHierarchy(NodeTree& tree, CSFile* csf, int idx, int cloneoffset)
{
  for(uint32_t i = 0; i < csf->nodes[idx].numChildren; i++)
  {
    tree.setNodeParent((NodeTree::nodeID)csf->nodes[idx].children[i] + cloneoffset, (NodeTree::nodeID)idx + cloneoffset);
  }

  for(uint32_t i = 0; i < csf->nodes[idx].numChildren; i++)
  {
    recursiveHierarchy(tree, csf, csf->nodes[idx].children[i], cloneoffset);
  }
}

bool CadScene::loadCSF(const char* filename, int clones, int cloneaxis)
{
  CSFile*         csf;
  CSFileMemoryPTR mem = CSFileMemory_new();
  if(CSFile_loadExt(&csf, filename, mem) != CADSCENEFILE_NOERROR || !(csf->fileFlags & CADSCENEFILE_FLAG_UNIQUENODES))
  {
    CSFileMemory_delete(mem);
    return false;
  }

  int copies = clones + 1;

  CSFile_transform(csf);

  srand(234525);

  // materials
  m_materials.resize(csf->numMaterials);
  for(int n = 0; n < csf->numMaterials; n++)
  {
    CSFMaterial* csfmaterial = &csf->materials[n];
    Material&    material    = m_materials[n];

    for(int i = 0; i < 2; i++)
    {
      material.sides[i].ambient  = randomVector(0.0f, 0.1f);
      material.sides[i].diffuse  = glm::make_vec4(csf->materials[n].color) + randomVector(0.0f, 0.07f);
      material.sides[i].specular = randomVector(0.25f, 0.55f);
      material.sides[i].emissive = randomVector(0.0f, 0.05f);
    }
  }

  glCreateBuffers(1, &m_materialsGL);
  glNamedBufferStorage(m_materialsGL, sizeof(Material) * m_materials.size(), &m_materials[0], 0);
  //glMapNamedBufferRange(m_materialsGL, 0, sizeof(Material) * m_materials.size(), GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT);

  // geometry
  int numGeoms = csf->numGeometries;
  m_geometry.resize(csf->numGeometries * copies);
  m_geometryBboxes.resize(csf->numGeometries * copies);
  for(int n = 0; n < csf->numGeometries; n++)
  {
    CSFGeometry* csfgeom = &csf->geometries[n];
    Geometry&    geom    = m_geometry[n];

    geom.cloneIdx = -1;

    geom.numVertices   = csfgeom->numVertices;
    geom.numIndexSolid = csfgeom->numIndexSolid;
    geom.numIndexWire  = csfgeom->numIndexWire;

    std::vector<Vertex> vertices(csfgeom->numVertices);
    for(uint32_t i = 0; i < csfgeom->numVertices; i++)
    {
      vertices[i].position[0] = csfgeom->vertex[3 * i + 0];
      vertices[i].position[1] = csfgeom->vertex[3 * i + 1];
      vertices[i].position[2] = csfgeom->vertex[3 * i + 2];
      vertices[i].position[3] = 1.0f;
      if(csfgeom->normal)
      {
        vertices[i].normal[0] = csfgeom->normal[3 * i + 0];
        vertices[i].normal[1] = csfgeom->normal[3 * i + 1];
        vertices[i].normal[2] = csfgeom->normal[3 * i + 2];
        vertices[i].normal[3] = 0.0f;
      }
      else
      {
        vertices[i].normal = glm::vec4(normalize(glm::vec3(vertices[i].position)), 0.0f);
      }


      m_geometryBboxes[n].merge(vertices[i].position);
    }

    geom.vboSize = sizeof(Vertex) * vertices.size();

    glCreateBuffers(1, &geom.vboGL);
    glNamedBufferStorage(geom.vboGL, geom.vboSize, &vertices[0], 0);

    std::vector<GLuint> indices(csfgeom->numIndexSolid + csfgeom->numIndexWire);
    memcpy(&indices[0], csfgeom->indexSolid, sizeof(GLuint) * csfgeom->numIndexSolid);
    if(csfgeom->indexWire)
    {
      memcpy(&indices[csfgeom->numIndexSolid], csfgeom->indexWire, sizeof(GLuint) * csfgeom->numIndexWire);
    }

    geom.iboSize = sizeof(GLuint) * indices.size();

    glCreateBuffers(1, &geom.iboGL);
    glNamedBufferStorage(geom.iboGL, geom.iboSize, &indices[0], 0);

    if(has_GL_NV_vertex_buffer_unified_memory)
    {
      glGetNamedBufferParameterui64vNV(geom.vboGL, GL_BUFFER_GPU_ADDRESS_NV, &geom.vboADDR);
      glMakeNamedBufferResidentNV(geom.vboGL, GL_READ_ONLY);

      glGetNamedBufferParameterui64vNV(geom.iboGL, GL_BUFFER_GPU_ADDRESS_NV, &geom.iboADDR);
      glMakeNamedBufferResidentNV(geom.iboGL, GL_READ_ONLY);
    }

    geom.parts.resize(csfgeom->numParts);

    size_t offsetSolid = 0;
    size_t offsetWire  = csfgeom->numIndexSolid * sizeof(GLuint);
    for(uint32_t i = 0; i < csfgeom->numParts; i++)
    {
      geom.parts[i].indexWire.count  = csfgeom->parts[i].numIndexWire;
      geom.parts[i].indexSolid.count = csfgeom->parts[i].numIndexSolid;

      geom.parts[i].indexWire.offset  = offsetWire;
      geom.parts[i].indexSolid.offset = offsetSolid;

      offsetWire += csfgeom->parts[i].numIndexWire * sizeof(GLuint);
      offsetSolid += csfgeom->parts[i].numIndexSolid * sizeof(GLuint);
    }
  }
  for(int c = 1; c <= clones; c++)
  {
    for(int n = 0; n < numGeoms; n++)
    {
      m_geometryBboxes[n + numGeoms * c] = m_geometryBboxes[n];

      const Geometry& geomorig = m_geometry[n];
      Geometry&       geom     = m_geometry[n + numGeoms * c];

      geom = geomorig;

#if 1
      geom.cloneIdx = n;
#else
      geom.cloneIdx = -1;
      glCreateBuffers(1, &geom.vboGL);
      glNamedBufferStorage(geom.vboGL, geom.vboSize, 0, 0);

      glCreateBuffers(1, &geom.iboGL);
      glNamedBufferStorage(geom.iboGL, geom.iboSize, 0, 0);

      if(has_GL_NV_vertex_buffer_unified_memory)
      {
        glGetNamedBufferParameterui64vNV(geom.vboGL, GL_BUFFER_GPU_ADDRESS_NV, &geom.vboADDR);
        glMakeNamedBufferResidentNV(geom.vboGL, GL_READ_ONLY);

        glGetNamedBufferParameterui64vNV(geom.iboGL, GL_BUFFER_GPU_ADDRESS_NV, &geom.iboADDR);
        glMakeNamedBufferResidentNV(geom.iboGL, GL_READ_ONLY);
      }

      glCopyNamedBufferSubData(geomorig.vboGL, geom.vboGL, 0, 0, geom.vboSize);
      glCopyNamedBufferSubData(geomorig.iboGL, geom.iboGL, 0, 0, geom.iboSize);
#endif
    }
  }


  glCreateBuffers(1, &m_geometryBboxesGL);
  glNamedBufferStorage(m_geometryBboxesGL, sizeof(BBox) * m_geometryBboxes.size(), &m_geometryBboxes[0], 0);
  glCreateTextures(GL_TEXTURE_BUFFER, 1, &m_geometryBboxesTexGL);
  glTextureBuffer(m_geometryBboxesTexGL, GL_RGBA32F, m_geometryBboxesGL);

  // nodes
  int numObjects = 0;
  m_matrices.resize(csf->numNodes * copies);

  for(int n = 0; n < csf->numNodes; n++)
  {
    CSFNode* csfnode = &csf->nodes[n];

    memcpy(glm::value_ptr(m_matrices[n].objectMatrix), csfnode->objectTM, sizeof(float) * 16);
    memcpy(glm::value_ptr(m_matrices[n].worldMatrix), csfnode->worldTM, sizeof(float) * 16);

    m_matrices[n].objectMatrixIT = glm::transpose(glm::inverse(m_matrices[n].objectMatrix));
    m_matrices[n].worldMatrixIT  = glm::transpose(glm::inverse(m_matrices[n].worldMatrix));

    if(csfnode->geometryIDX < 0)
      continue;

    numObjects++;
  }


  // objects
  m_objects.resize(numObjects * copies);
  m_objectAssigns.resize(numObjects * copies);
  numObjects = 0;
  for(int n = 0; n < csf->numNodes; n++)
  {
    CSFNode* csfnode = &csf->nodes[n];

    if(csfnode->geometryIDX < 0)
      continue;

    Object& object = m_objects[numObjects];

    object.matrixIndex   = n;
    object.geometryIndex = csfnode->geometryIDX;

    m_objectAssigns[numObjects] = glm::ivec2(object.matrixIndex, object.geometryIndex);

    object.parts.resize(csfnode->numParts);
    for(uint32_t i = 0; i < csfnode->numParts; i++)
    {
      object.parts[i].active        = 1;
      object.parts[i].matrixIndex   = csfnode->parts[i].nodeIDX < 0 ? object.matrixIndex : csfnode->parts[i].nodeIDX;
      object.parts[i].materialIndex = csfnode->parts[i].materialIDX;
    }

    BBox bbox = m_geometryBboxes[object.geometryIndex].transformed(m_matrices[n].worldMatrix);
    m_bbox.merge(bbox);

    updateObjectDrawCache(object);

    numObjects++;
  }

  // compute clone move delta based on m_bbox;

  glm::vec4 dim = m_bbox.max - m_bbox.min;

  int sq      = 1;
  int numAxis = 0;
  for(int i = 0; i < 3; i++)
  {
    numAxis += (cloneaxis & (1 << i)) ? 1 : 0;
  }

  assert(numAxis);

  switch(numAxis)
  {
    case 1:
      sq = copies;
      break;
    case 2:
      while(sq * sq < copies)
      {
        sq++;
      }
      break;
    case 3:
      while(sq * sq * sq < copies)
      {
        sq++;
      }
      break;
  }


  for(int c = 1; c <= clones; c++)
  {
    int numNodes = csf->numNodes;

    glm::vec4 shift = dim * 1.05f;

    float u = 0;
    float v = 0;
    float w = 0;

    switch(numAxis)
    {
      case 1:
        u = float(c);
        break;
      case 2:
        u = float(c % sq);
        v = float(c / sq);
        break;
      case 3:
        u = float(c % sq);
        v = float((c / sq) % sq);
        w = float(c / (sq * sq));
        break;
    }

    float use = u;

    if(cloneaxis & (1 << 0))
    {
      shift.x *= -use;
      if(numAxis > 1)
        use = v;
    }
    else
    {
      shift.x = 0;
    }

    if(cloneaxis & (1 << 1))
    {
      shift.y *= use;
      if(numAxis > 2)
        use = w;
      else if(numAxis > 1)
        use = v;
    }
    else
    {
      shift.y = 0;
    }

    if(cloneaxis & (1 << 2))
    {
      shift.z *= -use;
    }
    else
    {
      shift.z = 0;
    }

    shift.w = 0;

    // move all world matrices
    for(int n = 0; n < numNodes; n++)
    {
      MatrixNode& node     = m_matrices[n + numNodes * c];
      MatrixNode& nodeOrig = m_matrices[n];
      node                 = nodeOrig;
      node.worldMatrix[3]  = node.worldMatrix[3] + shift;
      node.worldMatrixIT   = glm::transpose(glm::inverse(node.worldMatrix));
    }

    {
      // patch object matrix of root
      MatrixNode& node     = m_matrices[csf->rootIDX + numNodes * c];
      node.objectMatrix[3] = node.objectMatrix[3] + shift;
      node.objectMatrixIT  = glm::transpose(glm::inverse(node.objectMatrix));
    }

    // clone objects
    for(int n = 0; n < numObjects; n++)
    {
      const Object& objectorig = m_objects[n];
      Object&       object     = m_objects[n + numObjects * c];

      object = objectorig;
      object.geometryIndex += c * numGeoms;
      object.matrixIndex += c * numNodes;
      for(size_t i = 0; i < object.parts.size(); i++)
      {
        object.parts[i].matrixIndex += c * numNodes;
      }
      for(size_t i = 0; i < object.cacheSolid.state.size(); i++)
      {
        object.cacheSolid.state[i].matrixIndex += c * numNodes;
      }
      for(size_t i = 0; i < object.cacheWire.state.size(); i++)
      {
        object.cacheWire.state[i].matrixIndex += c * numNodes;
      }

      m_objectAssigns[n + numObjects * c] = glm::ivec2(object.matrixIndex, object.geometryIndex);
    }
  }

  glCreateBuffers(1, &m_matricesGL);
  glNamedBufferStorage(m_matricesGL, sizeof(MatrixNode) * m_matrices.size(), &m_matrices[0], 0);
  //glMapNamedBufferRange(m_matricesGL, 0, sizeof(MatrixNode) * m_matrices.size(), GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT);

  glCreateTextures(GL_TEXTURE_BUFFER, 1, &m_matricesTexGL);
  glTextureBuffer(m_matricesTexGL, GL_RGBA32F, m_matricesGL);

  glCreateBuffers(1, &m_objectAssignsGL);
  glNamedBufferStorage(m_objectAssignsGL, sizeof(glm::ivec2) * m_objectAssigns.size(), &m_objectAssigns[0], 0);

  if(has_GL_NV_vertex_buffer_unified_memory)
  {
    glGetNamedBufferParameterui64vNV(m_materialsGL, GL_BUFFER_GPU_ADDRESS_NV, &m_materialsADDR);
    glMakeNamedBufferResidentNV(m_materialsGL, GL_READ_ONLY);

    glGetNamedBufferParameterui64vNV(m_matricesGL, GL_BUFFER_GPU_ADDRESS_NV, &m_matricesADDR);
    glMakeNamedBufferResidentNV(m_matricesGL, GL_READ_ONLY);

    if(has_GL_ARB_bindless_texture)
    {
      m_matricesTexGLADDR = glGetTextureHandleARB(m_matricesTexGL);
      glMakeTextureHandleResidentARB(m_matricesTexGLADDR);
    }
  }

  m_nodeTree.create(copies * csf->numNodes);
  for(int i = 0; i < copies; i++)
  {
    int cloneoffset = (csf->numNodes) * i;
    int root        = csf->rootIDX + cloneoffset;
    recursiveHierarchy(m_nodeTree, csf, csf->rootIDX, cloneoffset);

    m_nodeTree.setNodeParent((NodeTree::nodeID)root, m_nodeTree.getTreeRoot());
    m_nodeTree.addToTree((NodeTree::nodeID)root);
  }

  glCreateBuffers(1, &m_parentIDsGL);
  glNamedBufferStorage(m_parentIDsGL, m_nodeTree.getTreeCompactNodes().size() * sizeof(GLuint),
                       &m_nodeTree.getTreeCompactNodes()[0], 0);

  glCreateBuffers(1, &m_matricesOrigGL);
  glNamedBufferStorage(m_matricesOrigGL, sizeof(MatrixNode) * m_matrices.size(), &m_matrices[0], 0);
  glCreateTextures(GL_TEXTURE_BUFFER, 1, &m_matricesOrigTexGL);
  glTextureBuffer(m_matricesOrigTexGL, GL_RGBA32F, m_matricesOrigGL);

  CSFileMemory_delete(mem);
  return true;
}


struct ListItem
{
  CadScene::DrawStateInfo state;
  CadScene::DrawRange     range;
};

static bool ListItem_compare(const ListItem& a, const ListItem& b)
{
  int diff = 0;
  diff     = diff != 0 ? diff : (a.state.materialIndex - b.state.materialIndex);
  diff     = diff != 0 ? diff : (a.state.matrixIndex - b.state.matrixIndex);
  diff     = diff != 0 ? diff : int(a.range.offset - b.range.offset);

  return diff < 0;
}

static void fillCache(CadScene::DrawRangeCache& cache, const std::vector<ListItem>& list)
{
  cache = CadScene::DrawRangeCache();

  if(!list.size())
    return;

  CadScene::DrawStateInfo state = list[0].state;
  CadScene::DrawRange     range = list[0].range;

  int stateCount = 0;

  for(size_t i = 1; i < list.size() + 1; i++)
  {
    bool newrange = false;
    if(i == list.size() || list[i].state != state)
    {
      // push range
      stateCount++;
      cache.offsets.push_back(range.offset);
      cache.counts.push_back(range.count);

      // emit
      cache.state.push_back(state);
      cache.stateCount.push_back(stateCount);

      stateCount = 0;

      if(i == list.size())
      {
        break;
      }
      else
      {
        state        = list[i].state;
        range.offset = list[i].range.offset;
        range.count  = 0;
        newrange     = true;
      }
    }

    const CadScene::DrawRange& currange = list[i].range;
    if(newrange || (USE_CACHECOMBINE && currange.offset == (range.offset + sizeof(GLuint) * range.count)))
    {
      // merge
      range.count += currange.count;
    }
    else
    {
      // push
      stateCount++;
      cache.offsets.push_back(range.offset);
      cache.counts.push_back(range.count);

      range = currange;
    }
  }
}

void CadScene::updateObjectDrawCache(Object& object)
{
  Geometry& geom = m_geometry[object.geometryIndex];

  std::vector<ListItem> listSolid;
  std::vector<ListItem> listWire;

  listSolid.reserve(geom.parts.size());
  listWire.reserve(geom.parts.size());

  for(size_t i = 0; i < geom.parts.size(); i++)
  {
    if(!object.parts[i].active)
      continue;

    ListItem item;
    item.state.materialIndex = object.parts[i].materialIndex;

    item.range             = geom.parts[i].indexSolid;
    item.state.matrixIndex = object.parts[i].matrixIndex;
    listSolid.push_back(item);

    item.range             = geom.parts[i].indexWire;
    item.state.matrixIndex = object.parts[i].matrixIndex;
    listWire.push_back(item);
  }

  std::sort(listSolid.begin(), listSolid.end(), ListItem_compare);
  std::sort(listWire.begin(), listWire.end(), ListItem_compare);

  fillCache(object.cacheSolid, listSolid);
  fillCache(object.cacheWire, listWire);
}

void CadScene::enableVertexFormat(int attrPos, int attrNormal)
{
  glVertexAttribFormat(attrPos, 3, GL_FLOAT, GL_FALSE, 0);
  glVertexAttribFormat(attrNormal, 3, GL_FLOAT, GL_FALSE, offsetof(CadScene::Vertex, normal));
  glVertexAttribBinding(attrPos, 0);
  glVertexAttribBinding(attrNormal, 0);
  glEnableVertexAttribArray(attrPos);
  glEnableVertexAttribArray(attrNormal);
  glBindVertexBuffer(0, 0, 0, sizeof(CadScene::Vertex));
}

void CadScene::disableVertexFormat(int attrPos, int attrNormal)
{
  glDisableVertexAttribArray(attrPos);
  glDisableVertexAttribArray(attrNormal);
  glBindVertexBuffer(0, 0, 0, sizeof(CadScene::Vertex));
}

void CadScene::unload()
{
  if(m_geometry.empty())
    return;

  glFinish();

  if(has_GL_NV_vertex_buffer_unified_memory)
  {
    if(has_GL_ARB_bindless_texture)
    {
      glMakeTextureHandleNonResidentARB(m_matricesTexGLADDR);
    }

    glMakeNamedBufferNonResidentNV(m_matricesGL);
    glMakeNamedBufferNonResidentNV(m_materialsGL);
  }

  glDeleteTextures(1, &m_matricesOrigTexGL);
  glDeleteTextures(1, &m_matricesTexGL);
  glDeleteTextures(1, &m_geometryBboxesTexGL);

  glDeleteBuffers(1, &m_matricesOrigGL);
  glDeleteBuffers(1, &m_matricesGL);
  glDeleteBuffers(1, &m_materialsGL);
  glDeleteBuffers(1, &m_objectAssignsGL);
  glDeleteBuffers(1, &m_geometryBboxesGL);
  glDeleteBuffers(1, &m_parentIDsGL);


  for(size_t i = 0; i < m_geometry.size(); i++)
  {
    if(m_geometry[i].cloneIdx >= 0)
      continue;

    if(has_GL_NV_vertex_buffer_unified_memory)
    {
      glMakeNamedBufferNonResidentNV(m_geometry[i].iboGL);
      glMakeNamedBufferNonResidentNV(m_geometry[i].vboGL);
    }
    glDeleteBuffers(1, &m_geometry[i].iboGL);
    glDeleteBuffers(1, &m_geometry[i].vboGL);
  }

  m_matrices.clear();
  m_geometryBboxes.clear();
  m_geometry.clear();
  m_objectAssigns.clear();
  m_objects.clear();
  m_geometryBboxes.clear();
  m_nodeTree.clear();

  glFinish();
}

void CadScene::resetMatrices()
{
  glCopyNamedBufferSubData(m_matricesOrigGL, m_matricesGL, 0, 0, sizeof(CadScene::MatrixNode) * m_matrices.size());
}
