// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <metall/logger.hpp>

int main(int argc, char *argv[]) {
#ifdef _FILE_OFFSET_BITS
  std::cout << "_FILE_OFFSET_BITS = " << _FILE_OFFSET_BITS << std::endl;
#else
  std::cerr << "_FILE_OFFSET_BITS is not defined" << std::endl;
#endif

  std::cout << "sizeof(off_t) = " << sizeof(off_t) << std::endl;

  if (argc != 3) {
    std::cerr << "Wrong arguments" << std::endl;
    std::cerr << "./verify_64bits_file_io [file_name] [size]" << std::endl;
    std::abort();
  }
  std::string file_name = argv[1];
  ssize_t size = std::stoull(argv[2]);

  const int fd =
      ::open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    metall::logger::perror(metall::logger::level::critical, __FILE__, __LINE__,
                           "open");
    std::abort();
  }

  auto buf = new char[size];
  for (ssize_t i = 0; i < size; ++i) {
    buf[i] = static_cast<char>(i);
  }

  const auto written_size = ::pwrite(fd, buf, size, 0);
  if (written_size == size) {
    std::cout << "Write succeeded!" << std::endl;
  } else {
    std::cerr << "Requested write size " << size << std::endl;
    std::cerr << "Actually written size " << written_size << std::endl;
    metall::logger::perror(metall::logger::level::critical, __FILE__, __LINE__,
                           "write");
    std::abort();
  }

  if (::close(fd) == -1) {
    metall::logger::perror(metall::logger::level::critical, __FILE__, __LINE__,
                           "close");
    std::abort();
  }

  delete[] buf;

  return 0;
}