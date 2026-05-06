#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INIT_POWERUP_DELAY 30
#define INIT_ENEMY_SPAWN_DELAY 0.5
#define INIT_PLAYER_MOVE_SPEED 180.0
#define INIT_PLAYER_SHOOT_DELAY 0.3
#define INIT_PLAYER_BULLET_SPEED 500.0
#define INIT_WORLD_BORDER 20

// these can change during gameplay
float PLAYER_MOVE_SPEED = INIT_PLAYER_MOVE_SPEED;
float PLAYER_SHOOT_DELAY = INIT_PLAYER_SHOOT_DELAY;
float PLAYER_BULLET_SPEED = INIT_PLAYER_BULLET_SPEED;
int WORLD_BORDER = INIT_WORLD_BORDER;
float POWERUP_DELAY = INIT_POWERUP_DELAY;
float ENEMY_SPAWN_DELAY = INIT_ENEMY_SPAWN_DELAY;
bool PLAYER_INVINCIBLE = false;

#define GAMEPAD_DEADZONE 0.2f
#define MAX_OBJECTS 10000
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#define BACKGROUND_COLOR BLACK

typedef enum {
  t_none,
  t_player,
  t_enemy,
  t_bullet,
  t_powerup,
} o_type;

typedef enum {
  st_none,
  st_enemy_purple,
  st_enemy_orange,
  st_enemy_green,
  st_bullet_player,
  st_bullet_enemy,
  st_powerup_shoot_speed,
  st_powerup_move_speed,
  st_powerup_invincible,
} o_subtype;

typedef int oid;

typedef struct {
  oid id;
  o_type type;
  o_subtype subtype;
  Vector2 velocity;
  Vector2 position;
  float radius;
  float rotationSpeed;
  float rotation;
} Object;

static const o_subtype ENEMY_SUBTYPES[] = {
    st_enemy_purple,
    st_enemy_orange,
    st_enemy_green,
};

static const o_subtype POWERUP_SUBTYPES[] = {
    st_powerup_shoot_speed,
    st_powerup_move_speed,
    st_powerup_invincible,
};

Object objects[MAX_OBJECTS];
oid active_objects[MAX_OBJECTS];
int active_object_count = 0;
int active_object_index[MAX_OBJECTS];
oid free_objects[MAX_OBJECTS];
int free_object_count = 0;
oid playerid;

float movex = 0.0f;
float movey = 0.0f;

void remove_object(oid id) {
  if (id <= 0 || id >= MAX_OBJECTS || objects[id].type == t_none) {
    return;
  }

  int removedIndex = active_object_index[id];
  oid movedId = active_objects[active_object_count - 1];
  active_objects[removedIndex] = movedId;
  active_object_index[movedId] = removedIndex;
  active_object_count--;

  objects[id].type = t_none;
  active_object_index[id] = -1;
  free_objects[free_object_count++] = id;
}

oid add_object(void) {
  if (free_object_count == 0) {
    return 0;
  }

  oid id = free_objects[--free_object_count];
  active_object_index[id] = active_object_count;
  active_objects[active_object_count++] = id;
  objects[id].id = id;
  return id;
}

float enemy_radius_for_subtype(o_subtype subtype) {
  switch (subtype) {
  case st_enemy_purple:
    return 20;
  case st_enemy_orange:
    return 25;
  case st_enemy_green:
    return 15;
  default:
    return 0;
  }
}

int signof(int val) {
  // returns 1 for 0
  return (val >= 0) - (val < 0);
}

int randbetween(int min, int max) { return (rand() % (max - min + 1)) + min; }

float enemy_rotationSpeed_for_subtype(o_subtype subtype) {
  int dir = signof(randbetween(-5, 5));

  switch (subtype) {
  case st_enemy_purple:
    return 300.0f * dir;
  case st_enemy_orange:
    return 100.0f * dir;
  case st_enemy_green:
    return 600.0f * dir;
  default:
    return 0;
  }
}

void spawn_enemy(void) {
  Object enemy = {.id = add_object(), .type = t_enemy, .velocity = {0, 0}};
  if (enemy.id == 0) {
    return;
  }

  enemy.subtype = ENEMY_SUBTYPES[rand() % ARRAY_COUNT(ENEMY_SUBTYPES)];
  enemy.radius = enemy_radius_for_subtype(enemy.subtype);
  enemy.rotationSpeed = enemy_rotationSpeed_for_subtype(enemy.subtype);

  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  float outside = enemy.radius + 1.0f;

  switch (rand() % 4) {
  case 0:
    enemy.position = (Vector2){-outside, randbetween(0, screenHeight)};
    break;
  case 1:
    enemy.position =
        (Vector2){screenWidth + outside, randbetween(0, screenHeight)};
    break;
  case 2:
    enemy.position = (Vector2){randbetween(0, screenWidth), -outside};
    break;
  default:
    enemy.position =
        (Vector2){randbetween(0, screenWidth), screenHeight + outside};
    break;
  }

  objects[enemy.id] = enemy;
}

void setup(void) {
  memset(objects, 0, sizeof(objects));
  active_object_count = 0;
  free_object_count = 0;

  for (int i = 0; i < MAX_OBJECTS; i++) {
    active_object_index[i] = -1;
  }

  for (int i = MAX_OBJECTS - 1; i > 0; i--) {
    free_objects[free_object_count++] = i;
  }

  Object player = {
      .id = add_object(),
      .position = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f},
      .type = t_player,
      .radius = 15,
      .velocity = {0, 0}};

  playerid = player.id;
  objects[playerid] = player;

  for (int e = 0; e < 10; e++) {
    spawn_enemy();
  }
}

void player_shoot(Vector2 pos, Vector2 dir) {

  Object bullet = {
      .id = add_object(),
      .position = pos,
      .type = t_bullet,
      .radius = 3,
      .subtype = st_bullet_player,
      .velocity = {dir.x * PLAYER_BULLET_SPEED, dir.y * PLAYER_BULLET_SPEED}};
  if (bullet.id == 0) {
    return;
  }

  objects[bullet.id] = bullet;
}

float lastShot = 0;

Vector2 normalize_or_zero(Vector2 v) {
  if (Vector2LengthSqr(v) <= 0.0f) {
    return (Vector2){0.0f, 0.0f};
  }

  return Vector2Normalize(v);
}

Vector2 get_gamepad_stick(int gamepad, int axisX, int axisY) {
  Vector2 stick = {GetGamepadAxisMovement(gamepad, axisX),
                   GetGamepadAxisMovement(gamepad, axisY)};

  if (Vector2Length(stick) < GAMEPAD_DEADZONE) {
    return (Vector2){0.0f, 0.0f};
  }

  return stick;
}

void input(void) {
  movex = 0.0f;
  movey = 0.0f;
  if (playerid <= 0)
    return;

  if (IsKeyDown(KEY_W)) {
    movey -= 1.0f;
  }
  if (IsKeyDown(KEY_A)) {
    movex -= 1.0f;
  }
  if (IsKeyDown(KEY_S)) {
    movey += 1.0f;
  }
  if (IsKeyDown(KEY_D)) {
    movex += 1.0f;
  }
  if (IsKeyPressed(KEY_ENTER)) {
    spawn_enemy();
  }

  Vector2 moveDir = {movex, movey};
  Vector2 aimDir =
      Vector2Subtract(GetMousePosition(), objects[playerid].position);

  if (IsGamepadAvailable(0)) {
    Vector2 leftStick =
        get_gamepad_stick(0, GAMEPAD_AXIS_LEFT_X, GAMEPAD_AXIS_LEFT_Y);
    Vector2 rightStick =
        get_gamepad_stick(0, GAMEPAD_AXIS_RIGHT_X, GAMEPAD_AXIS_RIGHT_Y);

    if (Vector2LengthSqr(leftStick) > 0.0f) {
      moveDir = leftStick;
    }
    if (Vector2LengthSqr(rightStick) > 0.0f) {
      aimDir = rightStick;
    }
  }

  if (GetTime() - lastShot > PLAYER_SHOOT_DELAY) {
    Vector2 shotDir = normalize_or_zero(aimDir);

    if (Vector2LengthSqr(shotDir) > 0.0f) {
      player_shoot(objects[playerid].position, shotDir);
      lastShot = GetTime();
    }
  }

  moveDir = normalize_or_zero(moveDir);

  objects[playerid].velocity.x = moveDir.x * PLAYER_MOVE_SPEED;
  objects[playerid].velocity.y = moveDir.y * PLAYER_MOVE_SPEED;
}

void spawn_powerup(Vector2 pos, o_subtype subtype) {
  Object pup = {.id = add_object(),
                .type = t_powerup,
                .subtype = subtype,
                .velocity = {0, 0},
                .radius = 21,
                .position = pos};
  if (pup.id == 0) {
    return;
  }

  objects[pup.id] = pup;
}

void spawn_random_powerup(Vector2 pos) {
  o_subtype subtype = POWERUP_SUBTYPES[rand() % ARRAY_COUNT(POWERUP_SUBTYPES)];
  spawn_powerup(pos, subtype);
}

float lastEnemySpawn = 0.0f;

void update(void) {
  for (int objectIndex = 0; objectIndex < active_object_count;) {
    int i = active_objects[objectIndex];

    if (objects[i].type == t_none) {
      objectIndex++;
      continue;
    }

    if (GetTime() - lastEnemySpawn > ENEMY_SPAWN_DELAY) {
      spawn_enemy();
      lastEnemySpawn = GetTime();
    }

    // integrate velocity and rotation
    float dx = objects[i].velocity.x * GetFrameTime();
    float dy = objects[i].velocity.y * GetFrameTime();
    objects[i].position.x += dx;
    objects[i].position.y += dy;

    objects[i].rotation += objects[i].rotationSpeed * GetFrameTime();

    // keep the player in bounds
    if (objects[i].type == t_player) {
      if (objects[i].position.x >
              GetScreenWidth() - WORLD_BORDER - objects[i].radius ||
          objects[i].position.x < WORLD_BORDER + objects[i].radius) {
        // undo movement
        objects[i].position.x -= dx;
      }
      if (objects[i].position.y >
              GetScreenHeight() - WORLD_BORDER - objects[i].radius ||
          objects[i].position.y < WORLD_BORDER + objects[i].radius) {
        // undo movement
        objects[i].position.y -= dy;
      }
    }

    // despawn bullets when they leave the world bounds
    if (objects[i].type == t_bullet) {
      if (objects[i].position.x > GetScreenWidth() - WORLD_BORDER ||
          objects[i].position.y > GetScreenHeight() - WORLD_BORDER ||
          objects[i].position.x < WORLD_BORDER ||
          objects[i].position.y < WORLD_BORDER) {
        remove_object(i);
        continue;
      }
    }

    // basic collision detection
    if (objects[i].type == t_bullet && objects[i].subtype == st_bullet_player) {
      // just loop over every object again :D
      for (int targetIndex = 0; targetIndex < active_object_count;) {
        int j = active_objects[targetIndex];
        if (objects[j].type == t_enemy) {
          float dist =
              Vector2Distance(objects[i].position, objects[j].position);
          if (dist <= objects[j].radius + objects[i].radius) {
            Vector2 powerupPos = objects[j].position;
            // should probably do this at the end of frame or something
            remove_object(j);
            remove_object(i);

            if (randbetween(0, 100) > 90) {
              spawn_random_powerup(powerupPos);
            }
            break;
          }
        }

        targetIndex++;
      }
    }

    if (objects[i].type == t_player) {

      for (int targetIndex = 0; targetIndex < active_object_count;) {
        int j = active_objects[targetIndex];

        if (j == i) {
          targetIndex++;
          continue;
        }

        float dist = Vector2Distance(objects[i].position, objects[j].position);
        bool collided = dist <= objects[i].radius + objects[j].radius;

        if (objects[j].type == t_enemy) {
          // TODO: die
          if (collided && !PLAYER_INVINCIBLE) {
            printf("Should die here\n");
          }
        }

        if (objects[j].type == t_powerup && collided) {
          switch (objects[j].subtype) {
          case st_powerup_move_speed:
            PLAYER_MOVE_SPEED *= 1.5;
            break;
          case st_powerup_shoot_speed:
            PLAYER_SHOOT_DELAY /= 2;
            break;
          case st_powerup_invincible:
            PLAYER_INVINCIBLE = true;
            break;
          default:
            break;
          }

          remove_object(objects[j].id);
          continue;
        }

        targetIndex++;
      }
    }

    // "AI"
    if (objects[i].type == t_enemy) {
      Vector2 playerPos = objects[playerid].position;
      Vector2 vel =
          Vector2Normalize(Vector2Subtract(playerPos, objects[i].position));

      switch (objects[i].subtype) {
      case st_enemy_purple:
        objects[i].velocity.x = vel.x * 75;
        objects[i].velocity.y = vel.y * 75;
        break;
      case st_enemy_orange:
        objects[i].velocity.x = vel.x * 50;
        objects[i].velocity.y = vel.y * 50;
        break;
      case st_enemy_green:
        objects[i].velocity.x = vel.x * 20;
        objects[i].velocity.y = vel.y * 20;
        break;
      default:
        break;
      }
    }

    objectIndex++;
  }
}

void render_game(void) {
  int borderWidth = WORLD_BORDER;

  // draw borders
  DrawRectangleLinesEx((Rectangle){WORLD_BORDER, WORLD_BORDER,
                                   GetScreenWidth() - WORLD_BORDER * 2,
                                   GetScreenHeight() - WORLD_BORDER * 2},
                       2.0f, GREEN);

  for (int objectIndex = 0; objectIndex < active_object_count; objectIndex++) {
    int i = active_objects[objectIndex];

    switch (objects[i].type) {
    case t_none:
      continue;
    case t_player:
      DrawPolyLinesEx(objects[i].position, 16, objects[i].radius, 0, 2.0f,
                      WHITE);
      break;
    case t_bullet:
      if (objects[i].subtype == st_bullet_player) {
        DrawCircle(objects[i].position.x, objects[i].position.y, 3, LIGHTGRAY);
      } else {
        DrawCircle(objects[i].position.x, objects[i].position.y, 3, RED);
      }
      break;
    case t_enemy:
      switch (objects[i].subtype) {
      case st_enemy_purple:
        DrawPolyLinesEx(objects[i].position, 5, objects[i].radius,
                        objects[i].rotation, 2.0f, PURPLE);
        break;
      case st_enemy_orange:
        DrawPolyLinesEx(objects[i].position, 8, objects[i].radius,
                        objects[i].rotation, 2.0f, ORANGE);
        break;
      case st_enemy_green:
        DrawPolyLinesEx(objects[i].position, 3, objects[i].radius,
                        objects[i].rotation, 2.0f, GREEN);
        break;
      default:
        break;
      }
      break;
    case t_powerup:
      DrawCircleV(objects[i].position, objects[i].radius,
                  (Color){0, 255, 255, 80});
      DrawCircleLinesV(objects[i].position, objects[i].radius,
                       (Color){0, 255, 255, 255});
      DrawCircleLinesV(objects[i].position, objects[i].radius + 2,
                       (Color){0, 255, 255, 255});

      switch (objects[i].subtype) {
      case st_powerup_move_speed:
        DrawRectangle(objects[i].position.x - 6, objects[i].position.y - 6, 12,
                      12, YELLOW);
        break;
      case st_powerup_shoot_speed:
        DrawRectangle(objects[i].position.x - 6, objects[i].position.y - 6, 12,
                      12, RED);
        break;
      case st_powerup_invincible:
        DrawRectangle(objects[i].position.x - 6, objects[i].position.y - 6, 12,
                      12, MAGENTA);
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
  }

  if (borderWidth > 0) {
    DrawRectangle(0, 0, GetScreenWidth(), borderWidth, BACKGROUND_COLOR);
    DrawRectangle(0, GetScreenHeight() - borderWidth, GetScreenWidth(),
                  borderWidth, BACKGROUND_COLOR);
    DrawRectangle(0, 0, borderWidth, GetScreenHeight(), BACKGROUND_COLOR);
    DrawRectangle(GetScreenWidth() - borderWidth, 0, borderWidth,
                  GetScreenHeight(), BACKGROUND_COLOR);
  }
}

int main(void) {
  const int screenWidth = 1280;
  const int screenHeight = 800;

  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "SHOOTAHHH");

  setup();

  char buf[255];

  while (!WindowShouldClose()) {
    input();
    update();

    BeginDrawing();
    ClearBackground(BACKGROUND_COLOR);
    render_game();

    sprintf(buf, "objects: %d", active_object_count);
    DrawText(buf, 2, 2, 16, WHITE);
    EndDrawing();
  }

  CloseWindow();

  return 0;
}
