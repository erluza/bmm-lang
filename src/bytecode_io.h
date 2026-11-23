#ifndef BYTECODE_IO_H
#define BYTECODE_IO_H

#include "vm.h"
#include "compiler.h"

int bmc_encode(Compiler *c, char **out_buf, size_t *out_len);
int bmc_decode(const char *buf, size_t len, BmmVM *vm, Compiler *c);

#endif
