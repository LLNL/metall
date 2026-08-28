// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/c_api/metall.h>

int main(void) {
  metall_manager* manager = metall_create("/tmp/dir");
  metall_close(manager);

  return 0;
}