#include <stdio.h>

#include <GLFW/glfw3.h>

#include "ecs.h"
#include "editor.h"
#include "goop.h"

#define CAM_SPEED 4.0f

enum EditorState { STATE_NONE, STATE_MOVE, STATE_RESIZE, STATE_MATERIAL };

void editor_input(Entity ent, InputEvent *event) {
  Editor *editor = entity_get_component(ent, COMPONENT_EDITOR);
  GoopEngine *goop = editor->goop;

  if (event->type == INPUT_KEY) {
    if (!event->key.pressed)
      return;

    if (event->key.key == GLFW_KEY_ESCAPE) {
      editor->state = STATE_NONE;
      return;
    }

    if (editor->state == STATE_MATERIAL) {
      if (event->key.key >= GLFW_KEY_0 && event->key.key <= GLFW_KEY_9) {
        if (editor->selected != -1) {
          SolidBlob *b = fixed_array_get(&goop->bs.solids, editor->selected);
          b->mat_idx = event->key.key - GLFW_KEY_0;
        }
      }
    }

    if (editor->state != STATE_NONE) {
      return;
    }

    if (event->key.mods & GLFW_MOD_CONTROL) {
      if (event->key.key == GLFW_KEY_S) {
        FILE *f = fopen("assets/_editor_out.blvl", "w");
        if (!f) {
          fprintf(stderr, "Failed to open assets/_editor_out.blvl\n");
          return;
        }

        FixedArray *solids = &goop->bs.solids;

        fprintf(f, "[solids]\nradius = [");
        for (int i = 0; i < solids->count; i++) {
          if (i != 0)
            fprintf(f, ", ");
          SolidBlob *b = fixed_array_get(solids, i);
          fprintf(f, "%.2f", b->radius);
        }
        fprintf(f, "]\npos = [");
        for (int i = 0; i < solids->count; i++) {
          if (i != 0)
            fprintf(f, ", ");
          SolidBlob *b = fixed_array_get(solids, i);
          fprintf(f, "[%.2f, %.2f, %.2f]", b->pos.X, b->pos.Y, b->pos.Z);
        }
        fprintf(f, "]\nmat_idx=[");
        for (int i = 0; i < solids->count; i++) {
          if (i != 0)
            fprintf(f, ", ");
          SolidBlob *b = fixed_array_get(solids, i);
          fprintf(f, "%d", b->mat_idx);
        }
        fprintf(f, "]\n");

        fclose(f);
      }

      return;
    } else if (event->key.mods & GLFW_MOD_SHIFT) {
      switch (event->key.key) {
      case GLFW_KEY_A: {
        HMM_Vec3 pos = goop->br.cam_trans.Columns[3].XYZ;
        pos = HMM_AddV3(pos,
                        HMM_MulV3F(goop->br.cam_trans.Columns[2].XYZ, -2.0f));

        SolidBlob *b = solid_blob_create(&goop->bs);
        if (b) {
          solid_blob_set_radius_pos(&goop->bs, b, 0.5, &pos);
          b->mat_idx = 1;
          editor->selected = fixed_array_get_idx_from_ptr(&goop->bs.solids, b);
          editor->state = STATE_NONE;
        }
        break;
      }
      case GLFW_KEY_D: {
        if (editor->selected == -1)
          break;
        SolidBlob *o = fixed_array_get(&goop->bs.solids, editor->selected);
        SolidBlob *b = solid_blob_create(&goop->bs);
        if (b) {
          solid_blob_set_radius_pos(&goop->bs, b, o->radius, &o->pos);
          b->mat_idx = o->mat_idx;
          editor->selected = fixed_array_get_idx_from_ptr(&goop->bs.solids, b);
          editor->state = STATE_MOVE;
        }
        break;
      }
      default:
        break;
      }

      return;
    }

    if (editor->selected == -1)
      return;
    switch (event->key.key) {
    case GLFW_KEY_G:
      editor->state = STATE_MOVE;
      break;
    case GLFW_KEY_R:
      editor->state = STATE_RESIZE;
      break;
    case GLFW_KEY_M:
      editor->state = STATE_MATERIAL;
      break;
    default:
      break;
    }
  } else if (event->type == INPUT_MOUSE_BUTTON) {
    if (event->mouse_button.button == GLFW_MOUSE_BUTTON_1 &&
        event->mouse_button.pressed) {
      if (editor->state != STATE_NONE) {
        editor->state = STATE_NONE;
        return;
      }

      double mouse_x, mouse_y;
      glfwGetCursorPos(goop->window, &mouse_x, &mouse_y);
      HMM_Vec2 screen_pos = {((float)mouse_x - global.win_width * 0.5f) /
                                 global.win_width * 2.0f,
                             -((float)mouse_y - global.win_height * 0.5f) /
                                 global.win_height * 2.0f};

      HMM_Mat4 inv_proj = HMM_InvGeneralM4(goop->br.proj_mat);
      HMM_Mat4 inv_view = HMM_InvGeneralM4(goop->br.view_mat);

      HMM_Vec4 start = HMM_MulM4V4(
          inv_view, HMM_MulM4V4(inv_proj, HMM_V4(screen_pos.X, screen_pos.Y,
                                                 0.0f, 1.0f)));
      HMM_Vec4 end = HMM_MulM4V4(
          inv_view, HMM_MulM4V4(inv_proj, HMM_V4(screen_pos.X, screen_pos.Y,
                                                 1.0f, 1.0f)));

      start.W = 1.0f / start.W;
      start.X *= start.W;
      start.Y *= start.W;
      start.Z *= start.W;

      end.W = 1.0f / end.W;
      end.X *= end.W;
      end.Y *= end.W;
      end.Z *= end.W;

      RaycastResult result;
      blob_sim_raycast(&result, &goop->bs, start.XYZ,
                       HMM_SubV3(end.XYZ, start.XYZ));

      if (result.has_hit) {
        editor->selected = result.blob_idx;
      } else {
        editor->selected = -1;
      }
    }
  } else if (event->type == INPUT_MOUSE_MOTION) {
    if (editor->selected == -1) {
      return;
    }

    SolidBlob *b = fixed_array_get(&goop->bs.solids, editor->selected);

    if (editor->state == STATE_MOVE) {
      HMM_Vec3 rel = {0};
      rel = HMM_AddV3(rel, HMM_MulV4F(goop->br.cam_trans.Columns[0],
                                      (float)event->mouse_motion.relx * 0.03f)
                               .XYZ);
      rel = HMM_AddV3(rel, HMM_MulV4F(goop->br.cam_trans.Columns[1],
                                      (float)-event->mouse_motion.rely * 0.03f)
                               .XYZ);
      HMM_Vec3 new_pos = HMM_AddV3(b->pos, rel);

      solid_blob_set_radius_pos(&goop->bs, b, b->radius, &new_pos);
    } else if (editor->state == STATE_RESIZE) {
      float radius = b->radius;
      radius += (float)event->mouse_motion.relx * 0.05f;

      solid_blob_set_radius_pos(&goop->bs, b, radius, &b->pos);
    }
  }
}

void editor_init(GoopEngine* goop) {
  Entity editor_ent = entity_create();
  Editor *editor = entity_add_component(editor_ent, COMPONENT_EDITOR);
  editor->goop = goop;
  editor->state = STATE_NONE;
  editor->selected = -1;
  editor->selected_text_box_ent = entity_create();
  TextBox *text_box =
      entity_add_component(editor->selected_text_box_ent, COMPONENT_TEXT_BOX);
  text_box->pos.X = 32;
  text_box->pos.Y = 200;
  text_box->text = "";
  InputHandler *handler =
      entity_add_component(editor_ent, COMPONENT_INPUT_HANDLER);
  handler->mask = INPUT_KEY | INPUT_MOUSE_BUTTON | INPUT_MOUSE_MOTION;
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

  global.mouse_captured = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2);

  if (global.mouse_captured && !glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) &&
      !glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)) {
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
  }

  cam_trans->Columns[3].XYZ = cam_pos;
  goop->bs.active_pos = cam_pos;

  TextBox *text_box =
      entity_get_component(editor->selected_text_box_ent, COMPONENT_TEXT_BOX);
  if (editor->selected != -1) {
    const char *action = "";
    switch (editor->state) {
    case STATE_NONE:
      action = "";
      break;
    case STATE_MOVE:
      action = "[MOVE]";
      break;
    case STATE_RESIZE:
      action = "[RESIZE]";
      break;
    case STATE_MATERIAL:
      action = "[MATERIAL]";
      break;
    }

    SolidBlob *b = fixed_array_get(&goop->bs.solids, editor->selected);
    snprintf(editor->selected_text, sizeof(editor->selected_text),
             "Selected: %d\nPos: (%.2f, %.2f, %.2f)\nRadius: %.2f\nMaterial: "
             "%d\n\n%s",
             editor->selected, b->pos.X, b->pos.Y, b->pos.Z, b->radius,
             b->mat_idx, action);

    text_box->text = editor->selected_text;
  } else {
    text_box->text = "";
  }
}