#pragma once

#include "HandmadeMath.h"

#define IK_CHAIN_MAX_JOINTS 4

typedef struct IKChainJoint {
  float length;
  HMM_Vec3 pos;
} IKChainJoint;

typedef struct IKChain {
  int joint_count;
  IKChainJoint joints[IK_CHAIN_MAX_JOINTS];
} IKChain;

typedef struct IKChainCreateInfo {
  int unused;
} IKChainCreateInfo;

typedef struct IKChainJointInfo {
  float length;
  HMM_Vec3 pos;
} IKChainJointInfo;

void ik_chain_create(IKChain *ik_chain, const IKChainCreateInfo *create_info);

void ik_chain_solve(IKChain *ik_chain, const HMM_Vec3 *origin,
                    const HMM_Vec3 *goal);

void ik_chain_joint_add(IKChain *ik_chain, const IKChainJointInfo *joint_info);

HMM_Vec3 ik_chain_joint_get_pos(IKChain *ik_chain, int joint_idx);
