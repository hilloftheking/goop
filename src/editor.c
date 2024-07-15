#include <stdio.h>

#include <GLFW/glfw3.h>

#include "ecs.h"
#include "editor.h"
#include "goop.h"

#define CAM_SPEED 4.0f

void editor_input(Entity ent, InputEvent *event) {
  Editor *editor = entity_get_component(ent, COMPONENT_EDITOR);
  GoopEngine *goop = editor->goop;

  if (event->type == INPUT_KEY) {
    if (event->key.key == GLFW_KEY_N && event->key.pressed) {
      HMM_Vec3 pos = goop->br.cam_trans.Columns[3].XYZ;
      pos = HMM_AddV3(pos, HMM_MulV3F(goop->br.cam_trans.Columns[2].XYZ, -2.0f));

      SolidBlob *b = solid_blob_create(&goop->bs);
      if (b) {
        solid_blob_set_radius_pos(&goop->bs, b, 0.5, &pos);
        b->mat_idx = 1;
      }
    }
  } else if (event->type == INPUT_MOUSE_BUTTON) {
    if (event->mouse_button.button == GLFW_MOUSE_BUTTON_1 &&
        event->mouse_button.pressed) {
      RaycastResult result;
      HMM_Vec3 ro = goop->br.cam_trans.Columns[3].XYZ;
      HMM_Vec3 rd = HMM_MulV3F(goop->br.cam_trans.Columns[2].XYZ, -20.0f);
      blob_sim_raycast(&result, &goop->bs, ro, rd);

      if (result.has_hit) {
        printf("has hit\n");
      }
    }
  }
}

void editor_init(GoopEngine* goop) {
  Entity editor_ent = entity_create();
  Editor *editor = entity_add_component(editor_ent, COMPONENT_EDITOR);
  editor->goop = goop;
  editor->selected = -1;
  InputHandler *handler =
      entity_add_component(editor_ent, COMPONENT_INPUT_HANDLER);
  handler->mask = INPUT_KEY | INPUT_MOUSE_BUTTON;
  handler->callback = editor_input;

  goop->br.cam_trans =
      HMM_LookAt_RH(HMM_V3(0, 0, 0), HMM_V3(0, 0, -2), HMM_V3(0, 1, 0));
  goop->br.view_mat = HMM_InvGeneralM4(goop->br.cam_trans);
}

void editor_process(Entity ent) {
  Editor *editor = entity_get_component(ent, COMPONENT_EDITOR);
  GoopEngine *goop = editor->goop;

  HMM_Mat4 *cam_trans = &goop->br.cam_trans;
  HMM_Vec3 cam_pos = goop->br.cam_trans.Columns[3].XYZ;
  *cam_trans = HMM_M4D(1.0f);

  *cam_trans = HMM_MulM4(
      *cam_trans, HMM_Rotate_RH(global.cam_rot_y, (HMM_Vec3){{0, 1, 0}}));
  *cam_trans = HMM_MulM4(
      *cam_trans, HMM_Rotate_RH(global.cam_rot_x, (HMM_Vec3){{1, 0, 0}}));

  GLFWwindow *window = goop->window;
  bool w_press = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  bool s_press = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  bool a_press = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  bool d_press = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  HMM_Vec4 dir = {0};
  dir.X = (float)(d_press - a_press);
  dir.Z = (float)(s_press - w_press);
  if (dir.X || dir.Z) {
    dir = HMM_MulM4V4(*cam_trans, dir);
    dir.W = 0;
    dir = HMM_NormV4(dir);

    cam_pos = HMM_AddV3(
        cam_pos, HMM_MulV3F(dir.XYZ, (float)global.curr_delta * CAM_SPEED));
  }

  cam_trans->Columns[3].XYZ = cam_pos;
  goop->bs.active_pos = cam_pos;
}