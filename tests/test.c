#define CMAP_IMPLEMENTATION
#include "cmap.h"

#include <stdio.h>

int main(int argc, char **argv) {
  char *buffer = 0;
  long length;
  FILE *f = fopen(argv[1], "rb");

  if (f) {
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    buffer = malloc(length);
    if (buffer) {
      fread(buffer, 1, length, f);
    }
    fclose(f);
  }

  if (buffer) {
    // start to process your data / extract strings here..
    cmap_entity_s *map = NULL;
    cmap_parse_map(&map, buffer);
    cmap_entity_s *entity = map;
    int i = 0;
    while (entity != NULL) {
      printf("Entity %d\n", i);
      cmap_entity_property_s *prop = entity->properties;
      while (prop != NULL) {
	printf("%s: %s\n", prop->key, prop->value);
	prop = prop->next;
      }
      entity = entity->next;
      i++;
    }

    return 0;
  }

  return 1;
}
