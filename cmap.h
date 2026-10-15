#ifndef CMAP_H
#define CMAP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cmap_vec3 {
  double x, y, z;
} cmap_vec3_s;

typedef struct cmap_plane {
  cmap_vec3_s normal;
  double offset;
} cmap_plane_s;

typedef struct cmap_face {
  cmap_vec3_s a, b, c;
  char *texture_name;
  cmap_plane_s texture_planes[2];
  double texture_scale[2];
  struct cmap_face *next;
} cmap_face_s;

typedef struct cmap_brush {
  cmap_face_s *faces;
  struct cmap_brush *next;
} cmap_brush_s;

typedef struct cmap_entity_property {
  char *key;
  char *value;
  struct cmap_entity_property *next;
} cmap_entity_property_s;

typedef struct cmap_entity {
  cmap_entity_property_s *properties;
  cmap_brush_s *brushes;
  struct cmap_entity *next;
} cmap_entity_s;

typedef enum cmap_parse_code {
  CMAP_SUCCESS,
  CMAP_FAIL,
  CMAP_ERROR
} cmap_parse_result_e;

cmap_parse_result_e cmap_parse_map(cmap_entity_s **map, char *data);
void cmap_free_map(cmap_entity_s *map);

#ifdef __cplusplus
}
#endif
#endif // CMAP_H

#if defined(__CLANGD)
#define CMAP_IMPLEMENTATION
#endif
#ifdef CMAP_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

#if defined(CMAP_MALLOC) && defined(CMAP_CALLOC) && defined(CMAP_FREE)
#elif !defined(CMAP_MALLOC) && !defined(CMAP_CALLOC) && !defined(CMAP_FREE)
#else
#error "Must defined CMAP_MALLOC, CMAP_CALLOC and CMAP_FREE"
#endif

#ifndef CMAP_MALLOC
#define CMAP_MALLOC(sz) malloc(sz)
#define CMAP_CALLOC(nb, sz) calloc(nb, sz)
#define CMAP_FREE(p) free(p)
#endif

typedef struct cmap_state {
  char *data;
  char *cur_start;
  char *cur_end;

} cmap_state;

static void cmap_peek(cmap_state *state) {
  state->cur_end = state->cur_start;
  while (*state->cur_end != '\0') {
    if (*state->cur_end == '\n' || *state->cur_end == ' ') {
      break;
    } else if (*state->cur_end != '\t') {
      state->cur_end++;
    }
  }
}

static void cmap_consume(cmap_state *state) {
  state->cur_start = state->cur_end + 1;
  state->cur_end = state->cur_start;
}

// compare token but dont consume
static int cmap_match(const char *str, cmap_state *state) {
  cmap_peek(state);
  for (int i = 0; i < state->cur_end - state->cur_start; i++) {
    if (*(str + i) != *(state->cur_start + i))
      return 0;
  }
  return 1;
}

// compare and consume
static int cmap_accept(const char *str, cmap_state *state) {
  if (cmap_match(str, state)) {
    cmap_consume(state);
    return 1;
  }
  return 0;
}

#define CMAP_PARSE_0_OR_MORE(type, state, func, into)                          \
  {                                                                            \
    type **head = into;                                                        \
    type *tail = *head;                                                        \
    while (1) {                                                                \
      type *node = (type *)CMAP_CALLOC(sizeof(type), 1);                       \
      if (!node) {                                                             \
        return CMAP_ERROR;                                                     \
      }                                                                        \
                                                                               \
      cmap_parse_result_e ret = func(&node, state);                            \
      if (ret == CMAP_FAIL) {                                                  \
        break;                                                                 \
      } else if (ret == CMAP_ERROR) {                                          \
        CMAP_FREE(node);                                                       \
        return CMAP_ERROR;                                                     \
      }                                                                        \
                                                                               \
      node->next = NULL;                                                       \
                                                                               \
      if (*head == NULL) {                                                     \
        *head = tail = node;                                                   \
      } else {                                                                 \
        tail->next = node;                                                     \
        tail = node;                                                           \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  (void)0


static cmap_parse_result_e cmap_parse_string(char **str, cmap_state *state) {
  cmap_peek(state);
  if (*state->cur_start == '"' && *(state->cur_end - 1) == '"') {
    size_t len = state->cur_end - state->cur_start - 2;
    *str = (char *)CMAP_MALLOC(sizeof(char) * len + 1);
    if (!*str) {
      return CMAP_ERROR;
    }
    if (!memcpy(*str, state->cur_start + 1, len)) {
      CMAP_FREE(*str);
      return CMAP_ERROR;
    }
    *(*str + len) = '\0';
    cmap_consume(state);
    return CMAP_SUCCESS;
  }
  return CMAP_FAIL;
}

cmap_parse_result_e cmap_parse_double(double *d, cmap_state *state) {
  cmap_peek(state);

  size_t len = state->cur_end - state->cur_start;
  char *tmp = (char *)CMAP_MALLOC(sizeof(char) * (len + 1));
  if (!tmp) {
    return CMAP_ERROR;
  }
  if (!memcpy(tmp, state->cur_start, len)) {
    CMAP_FREE(tmp);
    return CMAP_ERROR;
  }
  *(tmp + len) = '\0';

  char *end_ptr;
  double d_tmp = strtod(tmp, &end_ptr);
  if (*end_ptr != '\0') {
    if (end_ptr == tmp) {
      CMAP_FREE(tmp);
      return CMAP_FAIL;
    } else {
      CMAP_FREE(tmp);
      return CMAP_ERROR;
    }
  }

  *d = d_tmp;
  CMAP_FREE(tmp);
  cmap_consume(state);
  return CMAP_SUCCESS;
}

cmap_parse_result_e cmap_parse_vec3(cmap_vec3_s *vec3, cmap_state *state) {
  if (!cmap_accept("(", state)) {
    return CMAP_FAIL;
  }

  cmap_parse_result_e ret;
  ret = cmap_parse_double(&vec3->x, state);
  if (ret != CMAP_SUCCESS)
    return ret;
  ret = cmap_parse_double(&vec3->y, state);
  if (ret != CMAP_SUCCESS)
    return ret;
  ret = cmap_parse_double(&vec3->z, state);
  if (ret != CMAP_SUCCESS)
    return ret;

  if (!cmap_accept(")", state)) {
    return CMAP_ERROR;
  }

  return CMAP_SUCCESS;
}

cmap_parse_result_e cmap_parse_plane(cmap_plane_s *plane, cmap_state *state) {
  if (!cmap_accept("[", state)) {
    return CMAP_FAIL;
  }

  cmap_parse_result_e ret;
  ret = cmap_parse_double(&plane->normal.x, state);
  if (ret != CMAP_SUCCESS)
    return ret;
  ret = cmap_parse_double(&plane->normal.y, state);
  if (ret != CMAP_SUCCESS)
    return ret;
  ret = cmap_parse_double(&plane->normal.z, state);
  if (ret != CMAP_SUCCESS)
    return ret;
  ret = cmap_parse_double(&plane->offset, state);
  if (ret != CMAP_SUCCESS)
    return ret;

  if (!cmap_accept("]", state)) {
    return CMAP_ERROR;
  }

  return CMAP_SUCCESS;
}

cmap_parse_result_e cmap_parse_face(cmap_face_s **face, cmap_state *state) {
  cmap_parse_result_e ret;

  ret = cmap_parse_vec3(&(*face)->a, state);
  if (ret != CMAP_SUCCESS)
    return ret;
  ret = cmap_parse_vec3(&(*face)->b, state);
  if (ret != CMAP_SUCCESS)
    return ret;
  ret = cmap_parse_vec3(&(*face)->c, state);
  if (ret != CMAP_SUCCESS)
    return ret;

  cmap_peek(state);
  size_t len = state->cur_end - state->cur_start;
  char *str= (char*)CMAP_MALLOC(sizeof(char) * len + 1);
  if(!str) {
    return CMAP_ERROR;
  }
  if (!memcpy(str, state->cur_start, len)) {
    CMAP_FREE(str);
    return CMAP_ERROR;
  }
  *(str + len) = '\0';
  (*face)->texture_name = str;
  cmap_consume(state);


  ret = cmap_parse_plane(((*face)->texture_planes + 0), state);
  if (ret != CMAP_SUCCESS)
    return ret;
  ret = cmap_parse_plane(((*face)->texture_planes + 1), state);
  if (ret != CMAP_SUCCESS)
    return ret;

  // skip rotation
  cmap_peek(state);
  cmap_consume(state);

  ret = cmap_parse_double(((*face)->texture_scale + 0), state);
  if (ret != CMAP_SUCCESS)
    return ret;
  ret = cmap_parse_double(((*face)->texture_scale + 1), state);
  if (ret != CMAP_SUCCESS)
    return ret;

  return CMAP_SUCCESS;
}

cmap_parse_result_e cmap_parse_brush(cmap_brush_s **brush, cmap_state *state) {
  if (!cmap_accept("{", state)) {
    return CMAP_FAIL;
  }

  CMAP_PARSE_0_OR_MORE(cmap_face_s, state, cmap_parse_face, &(*brush)->faces);

  if (!cmap_accept("}", state)) {
    return CMAP_ERROR;
  }

  return CMAP_SUCCESS;
}

cmap_parse_result_e
cmap_parse_entity_property(cmap_entity_property_s **entity_property,
                           cmap_state *state) {
  cmap_parse_result_e ret;
  ret = cmap_parse_string(&(*entity_property)->key, state);
  if (ret != CMAP_SUCCESS) {
    return ret;
  }
  ret = cmap_parse_string(&(*entity_property)->value, state);
  if (ret != CMAP_SUCCESS) {
    return ret;
  }

  return CMAP_SUCCESS;
}

static cmap_parse_result_e cmap_parse_entity(cmap_entity_s **entity,
                                             cmap_state *state) {
  if (!cmap_accept("{", state)) {
    return CMAP_FAIL;
  }

  CMAP_PARSE_0_OR_MORE(cmap_entity_property_s, state,
                       cmap_parse_entity_property, &(*entity)->properties);
  CMAP_PARSE_0_OR_MORE(cmap_brush_s, state, cmap_parse_brush,
                       &(*entity)->brushes);

  if (!cmap_accept("}", state)) {
    return CMAP_ERROR;
  }

  return CMAP_SUCCESS;
}

cmap_parse_result_e cmap_parse_map(cmap_entity_s **map, char *data) {
  cmap_state state;
  state.cur_end = state.cur_start = state.data = data;

  CMAP_PARSE_0_OR_MORE(cmap_entity_s, &state, cmap_parse_entity, map);

  return CMAP_SUCCESS;
}

static void cmap_free_face(cmap_face_s *face) {
  if (!face) {
    return;
  }

  CMAP_FREE(face->texture_name);
  CMAP_FREE(face);
}

static void cmap_free_brush(cmap_brush_s *brush) {
  if (!brush) {
    return;
  }

  cmap_face_s *face = brush->faces;
  while (face != NULL) {
    cmap_face_s *next = face->next;
    cmap_free_face(face);
    face = next;
  }

  CMAP_FREE(brush);
}

static void cmap_free_entity_property(cmap_entity_property_s *prop) {
  if (!prop) {
    return;
  }

  CMAP_FREE(prop->key);
  CMAP_FREE(prop->value);
  CMAP_FREE(prop);
}

static void cmap_free_entity(cmap_entity_s *entity) {
  if (!entity) {
    return;
  }

  cmap_entity_property_s *prop = entity->properties;
  while (prop != NULL) {
    cmap_entity_property_s *next = prop->next;
    cmap_free_entity_property(prop);
    prop = next;
  }

  cmap_brush_s *brush = entity->brushes;
  while (brush != NULL) {
    cmap_brush_s *next = brush->next;
    cmap_free_brush(brush);
    brush = next;
  }

  CMAP_FREE(entity);
}

void cmap_free_map(cmap_entity_s *map) {
  cmap_entity_s *entity = map;
  while (entity != NULL) {
    cmap_entity_s *next = entity->next;
    cmap_free_entity(entity);
    entity = next;
  }
}

#endif
