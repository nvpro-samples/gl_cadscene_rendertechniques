/*-----------------------------------------------------------------------
  Copyright (c) 2014, NVIDIA. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Neither the name of its contributors may be used to endorse 
     or promote products derived from this software without specific
     prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------*/

#ifndef CADSCENE_H__
#define CADSCENE_H__

#include <GL/glew.h>
#include <nv_math/nv_math.h>
#include <vector>
#include "nodetree.hpp"

class CadScene {

public:

  struct BBox {
    nv_math::vec4f    min;
    nv_math::vec4f    max;

    BBox() : min(FLT_MAX), max(-FLT_MAX) {}

    inline void merge( const nv_math::vec4f& point )
    {
      min = nv_math::nv_min(min, point);
      max = nv_math::nv_max(max, point);
    }

    inline void merge( const BBox& bbox )
    {
      min = nv_math::nv_min(min, bbox.min);
      max = nv_math::nv_max(max, bbox.max);
    }

    inline BBox transformed ( const nv_math::mat4f &matrix, int dim=3)
    {
      int i;
      nv_math::vec4f box[16];
      // create box corners
      box[0] = nv_math::vec4f(min.x,min.y,min.z,min.w);
      box[1] = nv_math::vec4f(max.x,min.y,min.z,min.w);
      box[2] = nv_math::vec4f(min.x,max.y,min.z,min.w);
      box[3] = nv_math::vec4f(max.x,max.y,min.z,min.w);
      box[4] = nv_math::vec4f(min.x,min.y,max.z,min.w);
      box[5] = nv_math::vec4f(max.x,min.y,max.z,min.w);
      box[6] = nv_math::vec4f(min.x,max.y,max.z,min.w);
      box[7] = nv_math::vec4f(max.x,max.y,max.z,min.w);

      box[8] = nv_math::vec4f(min.x,min.y,min.z,max.w);
      box[9] = nv_math::vec4f(max.x,min.y,min.z,max.w);
      box[10] = nv_math::vec4f(min.x,max.y,min.z,max.w);
      box[11] = nv_math::vec4f(max.x,max.y,min.z,max.w);
      box[12] = nv_math::vec4f(min.x,min.y,max.z,max.w);
      box[13] = nv_math::vec4f(max.x,min.y,max.z,max.w);
      box[14] = nv_math::vec4f(min.x,max.y,max.z,max.w);
      box[15] = nv_math::vec4f(max.x,max.y,max.z,max.w);

      // transform box corners
      // and find new mins,maxs
      BBox bbox;

      for (i = 0; i < (1<<dim) ; i++){
        nv_math::vec4f point = matrix * box[i];
        bbox.merge(point);
      }

      return bbox;
    }
  };

  struct MaterialSide {
    nv_math::vec4f ambient;
    nv_math::vec4f diffuse;
    nv_math::vec4f specular;
    nv_math::vec4f emissive;
  };

  // need to keep this 256 byte aligned (UBO range)
  struct Material {
    MaterialSide  sides[2];
    GLuint64      texturesADDR[4];
    GLuint        textures[4];
    GLuint        _pad[4+16];

    Material() {
      memset(this,0,sizeof(Material));
    }
  };

  // need to keep this 256 byte aligned (UBO range)
  struct MatrixNode {
    nv_math::mat4f  worldMatrix;
    nv_math::mat4f  worldMatrixIT;
    nv_math::mat4f  objectMatrix;
    nv_math::mat4f  objectMatrixIT;
  };

  struct Vertex {
    nv_math::vec4f position;
    nv_math::vec4f normal;
  };

  struct DrawRange {
    size_t        offset;
    int           count;

    DrawRange() : offset(0) , count(0) {}
  };

  struct DrawStateInfo {
    int           materialIndex;
    int           matrixIndex;

    friend bool operator != ( const DrawStateInfo &lhs,  const DrawStateInfo &rhs){
      return lhs.materialIndex != rhs.materialIndex || lhs.matrixIndex != rhs.matrixIndex;
    }

    friend bool operator == ( const DrawStateInfo &lhs,  const DrawStateInfo &rhs){
      return lhs.materialIndex == rhs.materialIndex && lhs.matrixIndex == rhs.matrixIndex;
    }
  };

  struct DrawRangeCache {
    std::vector<DrawStateInfo>    state;
    std::vector<int>          stateCount;

    std::vector<size_t>       offsets;
    std::vector<int>          counts;
  };

  struct GeometryPart {
    DrawRange     indexSolid;
    DrawRange     indexWire;
  };

  struct Geometry {
    GLuint    vboGL;
    GLuint    iboGL;
    GLuint64  vboADDR;
    GLuint64  iboADDR;
    size_t    vboSize;
    size_t    iboSize;

    std::vector<GeometryPart> parts;

    int       numVertices;
    int       numIndexSolid;
    int       numIndexWire;
    
    int       cloneIdx;
  };

  struct ObjectPart {
    int   active;
    int   materialIndex;
    int   matrixIndex;
  };

  struct Object {
    int             matrixIndex;
    int             geometryIndex;

    std::vector<ObjectPart> parts;

    DrawRangeCache  cacheSolid;
    DrawRangeCache  cacheWire;
  };

  std::vector<Material>       m_materials;
  std::vector<BBox>           m_geometryBboxes;
  std::vector<Geometry>       m_geometry;
  std::vector<MatrixNode>     m_matrices;
  std::vector<Object>         m_objects;
  std::vector<nv_math::vec2i>  m_objectAssigns;


  BBox      m_bbox;

  GLuint    m_materialsGL;
  GLuint64  m_materialsADDR;
  GLuint    m_matricesGL;
  GLuint64  m_matricesADDR;
  GLuint    m_matricesTexGL;
  GLuint64  m_matricesTexGLADDR;
  GLuint    m_geometryBboxesGL;
  GLuint    m_geometryBboxesTexGL;
  GLuint    m_objectAssignsGL;

  GLuint    m_parentIDsGL;

  GLuint    m_matricesOrigGL;
  GLuint    m_matricesOrigTexGL;

  NodeTree  m_nodeTree;

  void  updateObjectDrawCache(Object& object);
  
  bool  loadCSF(const char* filename, int clones = 0, int cloneaxis=3);
  void  unload();

  static void enableVertexFormat(int attrPos, int attrNormal);
  static void disableVertexFormat(int attrPos, int attrNormal);
  void resetMatrices();
};


#endif

