#ifndef __MYTBF_H
#define __MYTBF_H

#define MYTBF_MAX   1024
typedef void mytbf_t;


mytbf_t *mytbf_init(int cps, int burst);

int mytbf_fetchtoken(mytbf_t*, int);//取token

int mytbf_returntokne(mytbf_t*, int);//还token

int mytbf_destroy(mytbf_t *);

#endif