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
/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#include "cadscenefile.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#if CSF_ZIP_SUPPORT
#include <zlib.h>
#endif

#define CADSCENEFILE_MAGIC 1567262451
#if defined(__amd64__) || defined(__x86_64__) || defined(_M_X64) || defined(__AMD64__)
#define xftell(f) _ftelli64(f)
#define xfseek(f,pos,encoded) _fseeki64(f,pos,encoded)
#else
#define xftell(f) ftell(f)
#define xfseek(f,pos,encoded) fseek(f,(long)pos,encoded)
#endif

struct CSFileMemory_s
{
  std::vector<void*>    m_allocations;

  void* alloc(size_t size, const void* indata=NULL)
  {
    void* data = malloc(size);
    if (indata){
      memcpy(data,indata,size);
    }
    m_allocations.push_back(data);
    return data;
  }

  ~CSFileMemory_s()
  {
    for (size_t i = 0; i < m_allocations.size(); i++){
      free(m_allocations[i]);
    }
  }
};

CSFAPI CSFileMemoryPTR CSFileMemory_new()
{
  return new CSFileMemory_s;
}
CSFAPI void CSFileMemory_delete(CSFileMemoryPTR mem)
{
  delete mem;
}

CSFAPI void* CSFileMemory_alloc(CSFileMemoryPTR mem, size_t sz, const void*fill)
{
  return mem->alloc(sz,fill);
}

static size_t CSFile_getRawSize(CSFile* csf)
{
  if (csf->magic != CADSCENEFILE_MAGIC || csf->version < CADSCENEFILE_VERSION_COMPAT) return 0;

  return csf->pointersOFFSET + csf->numPointers * sizeof(CSFoffset);
}

CSFAPI int     CSFile_loadRaw (CSFile** outcsf, size_t size, void* dataraw)
{
  char* data  = (char*)dataraw;
  CSFile* csf = (CSFile*)data;

  if (size < sizeof(CSFile) || csf->magic != CADSCENEFILE_MAGIC || (csf->version < CADSCENEFILE_VERSION_COMPAT)){
    *outcsf = 0;
    return CADSCENEFILE_ERROR_VERSION;
  }

  if (size < CSFile_getRawSize((CSFile*)dataraw))
  {
    *outcsf = 0;
    return CADSCENEFILE_ERROR_VERSION;
  }

  if (csf->version < 3){
    csf->fileFlags = csf->fileFlags ? CADSCENEFILE_FLAG_UNIQUENODES : 0;
  }

  csf->pointersOFFSET += (CSFoffset)csf;
  for (int i = 0; i < csf->numPointers; i++){
    CSFoffset* ptr = (CSFoffset*)(data + csf->pointers[i]);
    *(ptr) += (CSFoffset)csf;
  }

  if (csf->version < 4){
    for (int i = 0; i < csf->numNodes; i++){
      for (int p = 0; p < csf->nodes[i].numParts; p++){
        csf->nodes[i].parts[p].nodeIDX = -1;
      }
    }
  }

  *outcsf = csf;

  return CADSCENEFILE_NOERROR;
}

CSFAPI int CSFile_load(CSFile** outcsf, const char* filename, CSFileMemoryPTR mem)
{
  FILE* file;
  if (fopen_s(&file,filename,"rb")){
    *outcsf = 0;
    return CADSCENEFILE_ERROR_NOFILE;
  }

  CSFile header;
  size_t sizeshould = 0;
  if (!fread_s(&header,sizeof(header),sizeof(header),1,file) ||
      !(sizeshould=CSFile_getRawSize(&header)))
  {
    fclose(file);
    *outcsf = 0;
    return CADSCENEFILE_ERROR_VERSION;
  }
  
  // load the full file to memory
  xfseek(file, 0, SEEK_END);
  size_t size = (size_t)xftell(file);
  xfseek(file, 0, SEEK_SET);

  if (sizeshould != size){
    fclose(file);
    *outcsf = 0;
    return CADSCENEFILE_ERROR_VERSION;
  }
  
  char* data  = (char*)mem->alloc(size);
  fread_s(data,size,size,1,file);
  fclose(file);

  return CSFile_loadRaw(outcsf,size,data);
}

#if CSF_ZIP_SUPPORT
CSFAPI int CSFile_loadExt(CSFile** outcsf, const char* filename, CSFileMemoryPTR mem)
{
  size_t len = strlen(filename);
  if (strcmp(filename+len-3,".gz")==0) {
    gzFile  filegz = gzopen(filename,"rb");
    if (!filegz){
      *outcsf = 0;
      return CADSCENEFILE_ERROR_NOFILE;
    }

    CSFile header;
    size_t sizeshould = 0;
    if (!gzread(filegz,&header, (z_off_t)sizeof(header)) ||
      !(sizeshould=CSFile_getRawSize(&header)))
    {
      gzclose(filegz);
      *outcsf = 0;
      return CADSCENEFILE_ERROR_VERSION;
    }
    

    gzseek(filegz,0,SEEK_SET);
    char* data  = (char*)CSFileMemory_alloc(mem,sizeshould,0);
    if (!gzread(filegz,data, (z_off_t)sizeshould)){
      gzclose(filegz);
      *outcsf = 0;
      return CADSCENEFILE_ERROR_VERSION;
    }
    gzclose(filegz);

    return CSFile_loadRaw(outcsf,sizeshould,data);
  }
  else{
    return CSFile_load(outcsf,filename,mem);
  }
}
#endif

struct OutputFILE {
  FILE*   m_file;

  int  open(const char* filename, CSFileMemoryPTR mem)
  {
    return fopen_s(&m_file,filename,"wb");
  }
  void close()
  {
    fclose(m_file);
  }
  void seek(size_t offset, int pos)
  {
    xfseek (m_file, offset, pos);
  }
  void write(const void* data, size_t dataSize)
  {
    fwrite(data,dataSize,1,m_file);
  }
};


struct OutputBuf {
  char*   m_data;
  size_t  m_allocated;
  size_t  m_used;
  size_t  m_cur;

  int  open(const char* filename, CSFileMemoryPTR mem)
  {
    m_allocated = 1024*1024;
    m_data = (char*)malloc(m_allocated);
    m_used = 0;
    m_cur = 0;
    return 0;
  }
  void close()
  {
    if (m_data){
      free(m_data);
    }
    m_data = 0;
    m_allocated = 0;
    m_used = 0;
    m_cur = 0;
  }
  void seek(size_t offset, int pos)
  {
    switch(pos){
    case SEEK_CUR:
      m_cur += offset;
      break;
    case SEEK_SET:
      m_cur = offset;
      break;
    case SEEK_END:
      m_cur = m_used;
      break;
    }
  }
  void write(const void* data, size_t dataSize)
  {
    if (m_cur + dataSize > m_used) {
      m_used = m_cur + dataSize;
    }

    if (m_cur + dataSize > m_allocated){
      size_t add = m_allocated*2;
      if (add < dataSize) add = dataSize;

      size_t chunk = 1024*1024*128;
      if (add > chunk && dataSize < chunk){
        add = chunk;
      }
      m_data = (char*)realloc(m_data, m_allocated + add);
      m_allocated += add;
    }
    memcpy(m_data + m_cur, data, dataSize);
    m_cur += dataSize;
  }
};


#if CSF_ZIP_SUPPORT
struct OutputGZ {
  gzFile    m_file;
  OutputBuf m_buf;

  int  open(const char* filename, CSFileMemoryPTR mem)
  {
    m_buf.open(filename,mem);
    m_file = gzopen(filename,"wb");
    return m_file == 0;
  }
  void close()
  {
    gzwrite(m_file,m_buf.m_data,(z_off_t)m_buf.m_used);
    gzclose(m_file);
    m_buf.close();
  }
  void seek(size_t offset, int pos)
  {
    m_buf.seek(offset, pos);
  }
  void write(const void* data, size_t dataSize)
  {
    m_buf.write(data,dataSize);
  }
};
#endif

template<class T>
struct CSFOffsetMgr {
  struct Entry {
    CSFoffset             offset;
    CSFoffset             location;
  };
  T&                      m_file;
  std::vector<Entry>      m_offsetLocations;
  size_t                  m_current;


  CSFOffsetMgr(T& file) : m_current(0), m_file(file) {

  }

  size_t store(const void* data, size_t dataSize)
  {
    size_t last = m_current;
    m_file.write(data,dataSize);

    m_current += dataSize;
    return last;
  }

  size_t store(size_t location, const void* data, size_t dataSize)
  {
    size_t last = m_current;
    m_file.write(data,dataSize);

    m_current += dataSize;

    Entry entry = {last, location};
    m_offsetLocations.push_back(entry);

    return last;
  }

  void finalize(size_t tableCountLocation, size_t tableLocation) {
    m_file.seek (tableCountLocation, SEEK_SET);
    int num = int(m_offsetLocations.size());
    m_file.write(&num,sizeof(int));

    CSFoffset offset = (CSFoffset)m_current;
    m_file.seek (tableLocation, SEEK_SET);
    m_file.write(&offset,sizeof(CSFoffset));

    for (size_t i = 0; i < m_offsetLocations.size(); i++){
      m_file.seek (m_offsetLocations[i].location, SEEK_SET);
      m_file.write(&m_offsetLocations[i].offset,sizeof(CSFoffset));
    }

    // dump table
    m_file.seek (0, SEEK_END);
    for (size_t i = 0; i < m_offsetLocations.size(); i++){
      m_file.write(&m_offsetLocations[i].location,sizeof(CSFoffset));
    }

  }

};

template<class T>
static int CSFile_saveInternal(const CSFile* csf, const char* filename, CSFileMemoryPTR mem)
{
  T file;
  if (file.open(filename,mem)){
    return CADSCENEFILE_ERROR_NOFILE;
  }

  CSFOffsetMgr<T> mgr(file);

  CSFile dump = *csf;
  dump.version = CADSCENEFILE_VERSION;
  dump.magic   = CADSCENEFILE_MAGIC;
  // dump main part as is
  mgr.store(&dump,sizeof(CSFile));

  // iterate the objects

  {
    size_t geomOFFSET = mgr.store(offsetof(CSFile,geometriesOFFSET), 
      csf->geometries, sizeof(CSFGeometry) * csf->numGeometries);

    for (int i = 0; i < csf->numGeometries; i++,geomOFFSET+=sizeof(CSFGeometry)){
      const CSFGeometry* geo = csf->geometries + i;
      if (geo->vertex && geo->numVertices){
        mgr.store( geomOFFSET + offsetof(CSFGeometry,vertexOFFSET),
          geo->vertex, sizeof(float) * 3 * geo->numVertices);
      }
      if (geo->normal && geo->numVertices){
        mgr.store( geomOFFSET + offsetof(CSFGeometry,normalOFFSET),
          geo->normal, sizeof(float) * 3 * geo->numVertices);
      }
      if (geo->tex && geo->numVertices){
        mgr.store( geomOFFSET + offsetof(CSFGeometry,texOFFSET),
          geo->tex, sizeof(float) * 2 * geo->numVertices);
      }
      if (geo->indexSolid && geo->numIndexSolid){
        mgr.store( geomOFFSET + offsetof(CSFGeometry,indexSolidOFFSET),
          geo->indexSolid, sizeof(int) * geo->numIndexSolid);
      }
      if (geo->indexWire && geo->numIndexWire){
        mgr.store( geomOFFSET + offsetof(CSFGeometry,indexWireOFFSET),
          geo->indexWire, sizeof(int)  * geo->numIndexWire);
      }
      if (geo->parts && geo->numParts){
        mgr.store( geomOFFSET + offsetof(CSFGeometry,partsOFFSET),
          geo->parts, sizeof(CSFGeometryPart)  * geo->numParts);
      }
    }
  }


  {
    size_t matOFFSET = mgr.store(offsetof(CSFile,materialsOFFSET), 
      csf->materials, sizeof(CSFMaterial) * csf->numMaterials);

    for (int i = 0; i < csf->numMaterials; i++, matOFFSET+= sizeof(CSFMaterial)){
      const CSFMaterial* mat = csf->materials + i;
      if (mat->bytes && mat->numBytes){
        mgr.store(matOFFSET + offsetof(CSFMaterial,bytesOFFSET),
          mat->bytes, sizeof(unsigned char) * mat->numBytes);
      }
    }
  }

  {
    size_t nodeOFFSET = mgr.store(offsetof(CSFile,nodesOFFSET), 
      csf->nodes, sizeof(CSFNode) * csf->numNodes);

    for (int i = 0; i < csf->numNodes; i++, nodeOFFSET+=sizeof(CSFNode)){
      const CSFNode* node = csf->nodes + i;
      if (node->parts && node->numParts){
        mgr.store(nodeOFFSET + offsetof(CSFNode,partsOFFSET),
          node->parts, sizeof(CSFNodePart) * node->numParts);
      }
      if (node->children && node->numChildren){
        mgr.store(nodeOFFSET + offsetof(CSFNode,childrenOFFSET),
          node->children, sizeof(int) * node->numChildren);
      }
    }
  }

  mgr.finalize(offsetof(CSFile,numPointers),offsetof(CSFile,pointersOFFSET));

  file.close();

  return CADSCENEFILE_NOERROR;
}

CSFAPI int CSFile_save(const CSFile* csf, const char* filename)
{
  return CSFile_saveInternal<OutputFILE>(csf,filename,0);
}

#if CSF_ZIP_SUPPORT
CSFAPI int CSFile_saveExt(CSFile* csf, const char* filename)
{
  size_t len = strlen(filename);
  if (strcmp(filename+len-3,".gz")==0) {
    return CSFile_saveInternal<OutputGZ>(csf,filename,0);
  }
  else{
    return CSFile_saveInternal<OutputFILE>(csf,filename,0);
  }
}

#endif

static __forceinline void Matrix44Copy(float* __restrict dst, const float* __restrict  a)
{
  memcpy(dst,a,sizeof(float) * 16);
}

static __forceinline void Matrix44MultiplyFull( float* __restrict clip, const float* __restrict  proj , const float* __restrict modl)
{

  clip[ 0] = modl[ 0] * proj[ 0] + modl[ 1] * proj[ 4] + modl[ 2] * proj[ 8] + modl[ 3] * proj[12];
  clip[ 1] = modl[ 0] * proj[ 1] + modl[ 1] * proj[ 5] + modl[ 2] * proj[ 9] + modl[ 3] * proj[13];
  clip[ 2] = modl[ 0] * proj[ 2] + modl[ 1] * proj[ 6] + modl[ 2] * proj[10] + modl[ 3] * proj[14];
  clip[ 3] = modl[ 0] * proj[ 3] + modl[ 1] * proj[ 7] + modl[ 2] * proj[11] + modl[ 3] * proj[15];

  clip[ 4] = modl[ 4] * proj[ 0] + modl[ 5] * proj[ 4] + modl[ 6] * proj[ 8] + modl[ 7] * proj[12];
  clip[ 5] = modl[ 4] * proj[ 1] + modl[ 5] * proj[ 5] + modl[ 6] * proj[ 9] + modl[ 7] * proj[13];
  clip[ 6] = modl[ 4] * proj[ 2] + modl[ 5] * proj[ 6] + modl[ 6] * proj[10] + modl[ 7] * proj[14];
  clip[ 7] = modl[ 4] * proj[ 3] + modl[ 5] * proj[ 7] + modl[ 6] * proj[11] + modl[ 7] * proj[15];

  clip[ 8] = modl[ 8] * proj[ 0] + modl[ 9] * proj[ 4] + modl[10] * proj[ 8] + modl[11] * proj[12];
  clip[ 9] = modl[ 8] * proj[ 1] + modl[ 9] * proj[ 5] + modl[10] * proj[ 9] + modl[11] * proj[13];
  clip[10] = modl[ 8] * proj[ 2] + modl[ 9] * proj[ 6] + modl[10] * proj[10] + modl[11] * proj[14];
  clip[11] = modl[ 8] * proj[ 3] + modl[ 9] * proj[ 7] + modl[10] * proj[11] + modl[11] * proj[15];

  clip[12] = modl[12] * proj[ 0] + modl[13] * proj[ 4] + modl[14] * proj[ 8] + modl[15] * proj[12];
  clip[13] = modl[12] * proj[ 1] + modl[13] * proj[ 5] + modl[14] * proj[ 9] + modl[15] * proj[13];
  clip[14] = modl[12] * proj[ 2] + modl[13] * proj[ 6] + modl[14] * proj[10] + modl[15] * proj[14];
  clip[15] = modl[12] * proj[ 3] + modl[13] * proj[ 7] + modl[14] * proj[11] + modl[15] * proj[15];

}

static void CSFile_transformHierarchy(CSFile *csf, CSFNode * __restrict node, CSFNode * __restrict parent)
{
  if (parent){
    Matrix44MultiplyFull(node->worldTM, parent->worldTM, node->objectTM);
  }
  else{
    Matrix44Copy(node->worldTM,node->objectTM);
  }

  for (int i = 0; i < node->numChildren; i++){
    CSFNode* __restrict child = csf->nodes + node->children[i];
    CSFile_transformHierarchy(csf,child,node);
  }
}

CSFAPI int CSFile_transform( CSFile *csf )
{
  if (!(csf->fileFlags & CADSCENEFILE_FLAG_UNIQUENODES))
    return CADSCENEFILE_ERROR_OPERATION;

  CSFile_transformHierarchy(csf,csf->nodes + csf->rootIDX,NULL);
  return CADSCENEFILE_NOERROR;
}

