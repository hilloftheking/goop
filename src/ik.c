#include <stdio.h>

#include "core.h"
#include "ik.h"

void ik_chain_create(IKChain *ik_chain, const IKChainCreateInfo *create_info) {
  ik_chain->joint_count = 0;
}

static void solve_backward(IKChain *ik_chain, const HMM_Vec3 *goal) {
  int n = ik_chain->joint_count;
  ik_chain->joints[n - 1].pos = *goal;
  for (int i = n - 2; i >= 0; i--) {
    HMM_Vec3 r =
        HMM_SubV3(ik_chain->joints[i + 1].pos, ik_chain->joints[i].pos);
    float l = ik_chain->joints[i].length / HMM_LenV3(r);

    ik_chain->joints[i].pos =
        HMM_AddV3(HMM_MulV3F(ik_chain->joints[i + 1].pos, 1.0f - l),
                  HMM_MulV3F(ik_chain->joints[i].pos, l));
  }
}

static void solve_forward(IKChain *ik_chain, const HMM_Vec3 *origin) {
  int n = ik_chain->joint_count;
  ik_chain->joints[0].pos = *origin;
  for (int i = 0; i < n - 1; i++) {
    HMM_Vec3 r =
        HMM_SubV3(ik_chain->joints[i + 1].pos, ik_chain->joints[i].pos);
    float l = ik_chain->joints[i].length / HMM_LenV3(r);

    ik_chain->joints[i + 1].pos =
        HMM_AddV3(HMM_MulV3F(ik_chain->joints[i].pos, 1.0f - l),
                  HMM_MulV3F(ik_chain->joints[i + 1].pos, l));
  }
}

void ik_chain_solve(IKChain *ik_chain, const HMM_Vec3 *origin,
                    const HMM_Vec3 *goal) {
  for (int i = 0; i < 10; i++) {
    solve_backward(ik_chain, goal);
    solve_forward(ik_chain, origin);
  }
}

void ik_chain_joint_add(IKChain *ik_chain, const IKChainJointInfo *joint_info) {
  if (ik_chain->joint_count >= IK_CHAIN_MAX_JOINTS) {
    fprintf(stderr, "IK Chain has too many joints\n");
    exit_fatal_error();
  }

  ik_chain->joints[ik_chain->joint_count].length = joint_info->length;
  ik_chain->joints[ik_chain->joint_count].pos = joint_info->pos;
  ik_chain->joint_count++;
}

HMM_Vec3 ik_chain_joint_get_pos(IKChain *ik_chain, int joint_idx) {
  return ik_chain->joints[joint_idx].pos;
}