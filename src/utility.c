#include "opium/opium.h"

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


void
opi_intvec_init(struct opi_intvec *vec)
{
  vec->cap = 0x10;
  vec->size = 0;
  vec->data = malloc(sizeof(intmax_t) * vec->cap);
}

void
opi_intvec_destroy(struct opi_intvec *vec)
{ free(vec->data); }

void
opi_intvec_push(struct opi_intvec *vec, intmax_t x)
{
  if (vec->size == vec->cap) {
    vec->cap <<= 1;
    vec->data = realloc(vec->data, sizeof(intmax_t) * vec->cap);
  }
  vec->data[vec->size++] = x;
}

void
opi_intvec_pop(struct opi_intvec *vec, size_t n)
{
  opi_assert(n <= vec->size);
  vec->size -= n;
}

void
opi_intvec_insert(struct opi_intvec *vec, intmax_t x, size_t at)
{
  if (at == vec->size) {
    opi_intvec_push(vec, x);
  } else {
    if (vec->size == vec->cap) {
      vec->cap <<= 1;
      vec->data = realloc(vec->data, sizeof(intmax_t) * vec->cap);
    }
    memmove(vec->data + at + 1, vec->data + at, sizeof(intmax_t) * (vec->size - at));
    vec->data[at] = x;
    vec->size += 1;
  }
}

long long int
opi_intvec_find(struct opi_intvec *vec, intmax_t x)
{
  for (size_t i = 0; i < vec->size; ++i) {
    if (vec->data[i] == x)
      return i;
  }
  return -1;
}

long long int
opi_intvec_rfind(struct opi_intvec *vec, intmax_t x)
{
  for (ssize_t i = vec->size - 1; i >= 0; --i) {
    if (vec->data[i] == x)
      return i;
  }
  return -1;
}


void
opi_ptrvec_init(struct opi_ptrvec *vec)
{
  vec->cap = 0x10;
  vec->size = 0;
  vec->data = malloc(sizeof(void*) * vec->cap);
}

void
opi_ptrvec_destroy(struct opi_ptrvec *vec, void (*delete)(void*))
{
  if (delete) {
    while (vec->size--)
      delete(vec->data[vec->size]);
  }
  free(vec->data);
}

void
opi_ptrvec_push(struct opi_ptrvec *vec, void *ptr, void* (*copy)(void*))
{
  if (vec->size == vec->cap) {
    vec->cap <<= 1;
    vec->data = realloc(vec->data, sizeof(char*) * vec->cap);
  }
  vec->data[vec->size++] = copy ? copy(ptr) : ptr;
}

void
opi_ptrvec_pop(struct opi_ptrvec *vec, void (*delete)(void*))
{
  if (delete)
    delete(vec->data[vec->size - 1]);
  vec->size -= 1;
}

void
opi_ptrvec_insert(struct opi_ptrvec *vec, void *ptr, size_t at, void* (*copy)(void*))
{
  if (at == vec->size) {
    opi_ptrvec_push(vec, ptr, copy);
  } else {
    if (vec->size == vec->cap) {
      vec->cap <<= 1;
      vec->data = realloc(vec->data, sizeof(void*) * vec->cap);
    }
    memmove(vec->data + at + 1, vec->data + at, sizeof(void*) * (vec->size - at));
    vec->data[at] = copy ? copy(ptr) : ptr;
    vec->size += 1;
  }
}
