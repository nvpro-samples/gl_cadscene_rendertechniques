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

#pragma once

#include <vector>

class NodeTree {
public:
  enum Flags {
    INVALID = 0xFFFFFFFF,
    ROOT = 0x7FFFFFFF,
    LEVELBITS = 8,
    PARENTBITS = 32 - LEVELBITS
  };

  static constexpr unsigned INVALID_LEVEL = (1 << LEVELBITS) - 1;
  static constexpr unsigned INVALID_PARENT = (1 << PARENTBITS) - 1;

  struct compactID {
    unsigned level : LEVELBITS;
    unsigned parent : PARENTBITS;

    compactID(){
      level = INVALID_LEVEL;
      parent = INVALID_PARENT;
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
  std::vector<compactID>            m_treeCompactNodes;
  std::vector<Level>                m_levels;
  unsigned int                      m_treeCompactChangeID;
  int                               m_nodesActive;
  int                               m_levelsUsed;

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




