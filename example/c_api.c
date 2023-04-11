// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <assert.h>
#include <metall/c_api/metall.h>

int main(void) {
  // Basic allocation
  {
    metall_open(METALL_CREATE_ONLY, "/tmp/metall1");

    uint64_t *x = metall_malloc(sizeof(uint64_t));
    x[0] = 1;

    metall_free(x);

    metall_close();
  }

  // Allocate named object
  {
    metall_open(METALL_CREATE_ONLY, "/tmp/metall2");

    uint64_t *array = metall_named_malloc("array", sizeof(uint64_t) * 10);

    array[0] = 0;

    metall_flush();

    array[1] = 1;

    metall_close();
  }

  // Retrieve named object
  {
    metall_open(METALL_OPEN_ONLY, "/tmp/metall2");

    uint64_t *array = metall_find("array");

    assert(array[0] == 0);
    assert(array[1] == 1);

    metall_named_free("array");

    metall_close();
  }

  return 0;
}