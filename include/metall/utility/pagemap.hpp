#include <fcntl.h>  /* open */
#include <stdint.h> /* uint64_t  */
#include <stdlib.h> /* size_t */
#include <unistd.h> /* pread, sysconf */
#include <cassert>
#include <cstdint>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include <iostream>

namespace metall::utility{


  typedef struct
  {
    uint64_t pfn : 54;
    unsigned int soft_dirty : 1;
    unsigned int file_page : 1;
    unsigned int swapped : 1;
    unsigned int present : 1;
  } PagemapEntry;


  PagemapEntry parse_pagemap_entry(uint64_t raw_entry){
    PagemapEntry entry;
    entry.pfn = raw_entry & (((uint64_t)1 << 54) - 1);
    entry.soft_dirty = (raw_entry >> 54) & 1;
    entry.file_page = (raw_entry >> 61) & 1;
    entry.swapped = (raw_entry >> 62) & 1;
    entry.present = (raw_entry >> 63) & 1;
    return entry;
  } 

  uint64_t * read_raw_pagemap(void* vaddr, size_t length){
    int pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
    if (pagemap_fd == -1){
      std::cout << "Error: Failed to open /proc/self/pagemap - " << strerror(errno) << std::endl;
      return nullptr;
    }
    
    size_t pagemap_size = (length / sysconf(_SC_PAGE_SIZE)) * sizeof(uint64_t);
    size_t nread;
    ssize_t ret;
    uint64_t *data = new uint64_t[pagemap_size];

    uintptr_t addr = (uintptr_t)vaddr;
    // std::cout << "Starting to read pagemap file" << std::endl;
    nread = 0;
    while( nread < pagemap_size){
      // std::cout << "Count: " << (pagemap_size - nread) << std::endl;
      // std::cout << "Offset: " << ((addr / sysconf(_SC_PAGE_SIZE)) * sizeof(uint64_t)) << std::endl;
      // std::cout << "pagemap_size: " << pagemap_size  << std::endl;
      ret = pread(pagemap_fd, ((uint8_t *)data) + nread, pagemap_size - nread, (addr / sysconf(_SC_PAGE_SIZE)) * sizeof(uint64_t) + nread);
      nread += ret;
      if (ret <= 0)
      {
       	return nullptr;
      }
    }
    return data;
  }

}

