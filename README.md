# cmap

A Valve 220 `.map` file parser written as a single header lib in C99.

`cmap.h` is a single-header library: drop it into your project, define
`CMAP_IMPLEMENTATION` in exactly one translation unit, and you're ready to parse
`.map` files into a linked structure of entities, properties, brushes and faces.

## Features

- Single-header, C99, no dependencies beyond the standard library.
- Parses entities and their `key`/`value` properties.
- Parses brushes and their faces, including:
  - the three defining points (`a`, `b`, `c`) of each face,
  - the texture name,
  - the two texture planes (`u`/`v` axis + offset),
  - the texture scale.
- Overridable allocation via `CMAP_MALLOC`, `CMAP_CALLOC` and `CMAP_FREE`.
- Ready for C++ (`extern "C"` guards) and clangd tooling (`__CLANGD`).

## Layout

```
cmap.h          the entire library (header + implementation)
Makefile        build rules for the example (currently references test.c)
.clangd         clangd config that compiles cmap.h with -D__CLANGD
tests/          example program and a sample .map file
  test.c        minimal usage: loads a file, prints entity properties
  test.map      small Valve 220 map used as input
```

## Quick start

Copy `cmap.h` into your project, then in **one** `.c` file define the
implementation before including it:

```c
#define CMAP_IMPLEMENTATION
#include "cmap.h"
```

Everywhere else, just include the header as usual:

```c
#include "cmap.h"
```

### Parsing a file

```c
#define CMAP_IMPLEMENTATION
#include "cmap.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb");
  fseek(f, 0, SEEK_END);
  long length = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *buffer = malloc(length + 1);
  fread(buffer, 1, length, f);
  buffer[length] = '\0'; /* the parser relies on NUL termination */
  fclose(f);

  cmap_entity_s *map = NULL;
  if (cmap_parse_map(&map, buffer) != CMAP_SUCCESS) {
    free(buffer);
    return 1;
  }

  for (cmap_entity_s *e = map; e != NULL; e = e->next) {
    for (cmap_entity_property_s *p = e->properties; p != NULL; p = p->next) {
      printf("%s = %s\n", p->key, p->value);
    }
  }

  cmap_free_map(map);
  free(buffer);
  return 0;
}
```

## Building the example

The bundled example reads a `.map` path from the command line and prints the
properties of each entity:

```sh
cc -std=c99 -Wall -I. tests/test.c -o cmap_demo
./cmap_demo tests/test.map
```

> Note: the checked-in `Makefile` still points at `test.c` in the repo root,
> while the example now lives in `tests/`. Use the command above, or update the
> `Makefile` to reference `tests/test.c`.

## API

```c
cmap_parse_result_e cmap_parse_map(cmap_entity_s **map, char *data);
void cmap_free_map(cmap_entity_s *map);
```

- `cmap_parse_map` parses a NUL-terminated map string and stores the head of an
  entity list in `*map`. It returns one of the `cmap_parse_result_e` values:

  | Value          | Meaning                                        |
  | -------------- | ---------------------------------------------- |
  | `CMAP_SUCCESS` | Parsing completed.                             |
  | `CMAP_FAIL`    | The parser did not match the expected token.   |
  | `CMAP_ERROR`   | Allocation or malformed-input failure.         |

- `cmap_free_map` frees a map returned by `cmap_parse_map`, including every
  entity, property, brush, face and copied string. Passing `NULL` is a no-op.

### Data structures

```c
typedef struct cmap_vec3 { double x, y, z; } cmap_vec3_s;

typedef struct cmap_plane {
  cmap_vec3_s normal;
  double offset;
} cmap_plane_s;

typedef struct cmap_face {
  cmap_vec3_s a, b, c;              /* defining points          */
  char *texture_name;               /* owned string             */
  cmap_plane_s texture_planes[2];   /* u and v texture axes     */
  double texture_scale[2];          /* u and v scale            */
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
```

The map returned by the parser is a singly linked list of entities; each entity
owns a list of properties and a list of brushes, and each brush owns a list of
faces.

## Custom allocation

By default the library uses `malloc`, `calloc` and `free`. To route allocations
through your own allocator, define all three of the following before including
the implementation:

```c
#define CMAP_MALLOC(sz)      my_malloc(sz)
#define CMAP_CALLOC(nb, sz)  my_calloc(nb, sz)
#define CMAP_FREE(p)         my_free(p)
#define CMAP_IMPLEMENTATION
#include "cmap.h"
```

All three macros must be defined together; defining only some of them is a
compile-time error.

## Notes and limitations

- Face texture **rotation** is currently skipped during parsing and is not
  stored on `cmap_face_s`.
- All strings in the returned structure (texture names, property keys and
  values) are copied and owned by the library, so the input buffer may be
  freed after parsing.
- Free a parsed map only with `cmap_free_map`; it mirrors the internal
  allocation structure.
