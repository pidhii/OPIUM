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

#endif
