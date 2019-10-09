#include "opium/utility.h"

#include <stdlib.h>
#include <string.h>

void
opi_strvec_init(struct opi_strvec *vec)
{
  vec->cap = 0x10;
  vec->size = 0;
  vec->data = malloc(sizeof(char*) * vec->cap);
}

void
opi_strvec_destroy(struct opi_strvec *vec)
{
  while (vec->size--)
    free(vec->data[vec->size]);
  free(vec->data);
}

void
opi_strvec_push(struct opi_strvec *vec, const char *str)
{
  if (vec->size == vec->cap) {
    vec->cap <<= 1;
    vec->data = realloc(vec->data, sizeof(char*) * vec->cap);
  }
  vec->data[vec->size++] = strdup(str);
}

void
opi_strvec_pop(struct opi_strvec *vec)
{ free(vec->data[--vec->size]); }

void
opi_strvec_insert(struct opi_strvec *vec, const char *str, size_t at)
{
  if (at == vec->size) {
    opi_strvec_push(vec, str);
  } else {
    if (vec->size == vec->cap) {
      vec->cap <<= 1;
      vec->data = realloc(vec->data, sizeof(char*) * vec->cap);
    }
    memmove(vec->data + at + 1, vec->data + at, sizeof(char*) * (vec->size - at));
    vec->data[at] = strdup(str);
    vec->size += 1;
  }
}

long long int
opi_strvec_find(struct opi_strvec *vec, const char *str)
{
  for (size_t i = 0; i < vec->size; ++i) {
    if (strcmp(vec->data[i], str) == 0)
      return i;
  }
  return -1;
}

long long int
opi_strvec_rfind(struct opi_strvec *vec, const char *str)
{
  for (ssize_t i = vec->size - 1; i >= 0; --i) {
    if (strcmp(vec->data[i], str) == 0)
      return i;
  }
  return -1;
}
