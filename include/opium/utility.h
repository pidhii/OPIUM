#ifndef OPI_UTILITY_H
#define OPI_UTILITY_H

#include <stddef.h>
#include <stdint.h>


struct opi_strvec {
  char **data;
  size_t size;
  size_t cap;
};

void
opi_strvec_init(struct opi_strvec *vec);

void
opi_strvec_destroy(struct opi_strvec *vec);

void
opi_strvec_push(struct opi_strvec *vec, const char *str);

void
opi_strvec_pop(struct opi_strvec *vec);

void
opi_strvec_insert(struct opi_strvec *vec, const char *str, size_t at);

long long int
opi_strvec_find(struct opi_strvec *vec, const char *str);

long long int
opi_strvec_rfind(struct opi_strvec *vec, const char *str);


struct opi_intvec {
  intmax_t *data;
  size_t size;
  size_t cap;
};

void
opi_intvec_init(struct opi_intvec *vec);

void
opi_intvec_destroy(struct opi_intvec *vec);

void
opi_intvec_push(struct opi_intvec *vec, intmax_t x);

void
opi_intvec_pop(struct opi_intvec *vec, size_t n);

void
opi_intvec_insert(struct opi_intvec *vec, intmax_t x, size_t at);

long long int
opi_intvec_find(struct opi_intvec *vec, intmax_t x);

long long int
opi_intvec_rfind(struct opi_intvec *vec, intmax_t x);


struct opi_ptrvec {
  void **data;
  size_t size;
  size_t cap;
};

void
opi_ptrvec_init(struct opi_ptrvec *vec);

void
opi_ptrvec_destroy(struct opi_ptrvec *vec, void (*free)(void*));

void
opi_ptrvec_push(struct opi_ptrvec *vec, void *ptr, void* (*copy)(void*));

void
opi_ptrvec_pop(struct opi_ptrvec *vec, void (*free)(void*));

void
opi_ptrvec_insert(struct opi_ptrvec *vec, void *str, size_t at, void* (*copy)(void*));

#endif
