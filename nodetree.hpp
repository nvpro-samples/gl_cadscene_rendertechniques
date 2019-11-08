/* Copyright (c) 2014-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#pragma once

#include <vector>

class NodeTree {
public:
  enum Flags {
    INVALID = 0xFFFFFFFF,
    ROOT = 0x7FFFFFFF,
    LEVELBITS = 8,
  };

  struct compactID {
    unsigned level : LEVELBITS;
    unsigned parent : (32-LEVELBITS);

    compactID(){
      level = INVALID;
      parent = INVALID;
    }
  };
  typedef unsigned int nodeID;
  typedef unsigned int lvlID;


  struct Level {
    unsigned int          changeID;
    std::vector<nodeID>   nodes;
    std::vector<nodeID>   leaves;

    Level(){
      changeID = 0;
    }
  };

  struct Node {
    nodeID                parentidx;
    lvlID                 levelidx;
    lvlID                 leafidx;
    int                   level;
    nodeID                childidx;
    nodeID                siblingidx;
  };

private:

  Node                              m_root;

  // general nodes
  std::vector<Node>                 m_nodes;
  std::vector<nodeID>               m_unusedNodes;

  // actual nodes added to tree
  unsigned int                      m_treeCompactChangeID;
  std::vector<compactID>            m_treeCompactNodes;
  int                               m_nodesActive;
  int                               m_levelsUsed;
  std::vector<Level>                m_levels;

public:
  NodeTree();

  const Level*  getUsedLevel(int level) const;
  inline int getNumUsedLevel() const 
  {
    return m_levelsUsed;
  }

  unsigned int getTreeParentChangeID() const;
  const std::vector<compactID>& getTreeCompactNodes() const;

  inline nodeID getTreeRoot()
  {
    return ROOT;
  }

  inline const Node& getNode(nodeID nodeidx) const
  {
    if (nodeidx == ROOT) return m_root;
    else                 return m_nodes[nodeidx];
  }

  inline bool  isValid(unsigned int id)
  {
    return id != INVALID;
  }

  inline bool  isNodeInTree(nodeID nodeidx)
  {
    return isValid(nodeidx) && isValid(getNode(nodeidx).levelidx);
  }

  inline nodeID  getParentNode(nodeID nodeidx) const
  {
    return getNode(nodeidx).parentidx;
  }

  nodeID  createNode();

  void    deleteNode(nodeID nodeidx);

  void    setNodeParent(nodeID nodeidx, nodeID parentidx);

  void    addToTree(nodeID nodeidx);

  void    removeFromTree(nodeID nodeidx);

  void    reserve(int numNodes);

  void    create(int numNodes);

  void    clear();

  int     getNumActiveNodes() const {
    return m_nodesActive;
  }

private:

  inline Level& getLevel(int level)
  {
    if ((int)m_levels.size() < level+1){
      m_levels.resize(level+1);
    }
    return m_levels[level];
  }

  inline Node& getNode(nodeID nodeidx)
  {
    if (nodeidx == ROOT) return m_root;
    else                 return m_nodes[nodeidx];
  }

  void addToLevel(nodeID nodeidx, nodeID parentidx);

  void removeFromLevel(nodeID nodeidx);

  void removeLeafNode(nodeID nodeidx);

  void addLeafNode(nodeID nodeidx);

  void updateLeafNode(nodeID nodeidx);

  void updateLevelNode(nodeID nodeidx, nodeID parentidx);

};




