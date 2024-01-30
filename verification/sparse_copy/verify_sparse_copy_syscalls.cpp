#undef NDEBUG

#include <metall/metall.hpp>

#include <metall/detail/file.hpp>

#include <random>
#include <sys/mman.h>

std::filesystem::path make_test_path();
void randomized_copy_file_sparse_linux(std::filesystem::path const &p);
void adjacent_hole_copy_file_sparse_linux(std::filesystem::path const &p);

int main() {
  auto p = make_test_path();

  randomized_copy_file_sparse_linux(p);
  adjacent_hole_copy_file_sparse_linux(p);
}


// Utility

constexpr size_t FILE_SIZE = 4096*4*11;
std::default_random_engine rng{std::random_device{}()};

/**
 * Fills file at fd with FILE_SIZE random bytes
 */
void fill_file(int fd);

/**
 * Randomly punches 1-9 holes into fd.
 *
 * \param hole_at_start forces a hole to be created at the start of the file
 * \param hole_at_end forces a hole to be created at the end of the file
 */
void punch_holes(int fd, bool hole_at_start = false, bool hole_at_end = false);

/**
 * Convenience wrapper around ::mmap
 * \param fd file descriptor to mmap
 * \return pair (pointer to mapped range, size of mapped range)
 */
std::pair<unsigned char const *, off_t> mmap(int fd);

/**
 * Checks if files a and b are equal byte by byte
 */
void check_files_eq(int a, int b);

/**
 * Returns a list of all holes in the given file
 */
std::vector<std::pair<off_t, off_t>> get_holes(int fd);

/**
 * print all given holes
 */
 void list_holes(std::vector<std::pair<off_t, off_t>> const &holes);

/**
 * Checks if the holes in a and b are in the same places and of the same size
 */
void check_holes_eq(std::vector<std::pair<off_t, off_t>> const &holes_a, std::vector<std::pair<off_t, off_t>> const &holes_b);

/**
 * Runs the test
 */
template<typename P>
void sparse_copy_test(std::filesystem::path const &srcp,
                      std::filesystem::path const &dstp,
                      std::filesystem::path const &dst2p,
                      P &&punch_holes);


// Impl

std::filesystem::path make_test_path() {
  std::filesystem::path p{"/tmp/metallsparsecopytest" + std::to_string(rng())};
  std::filesystem::create_directory(p);
  return p;
}

void randomized_copy_file_sparse_linux(std::filesystem::path const &p) {
  auto srcp = p / "copy_file_sparse-src.bin";
  auto dstp = p / "copy_file_sparse-dst.bin";
  auto dst2p = p / "copy_file_sparse-dst2.bin";

  std::uniform_int_distribution<size_t> b{0, 1};

  for (size_t ix = 0; ix < 1000; ++ix) {
    sparse_copy_test(srcp, dstp, dst2p, [&](int fd) {
      punch_holes(fd, b(rng), b(rng));
    });
  }

  std::filesystem::remove(srcp);
  std::filesystem::remove(dstp);
  std::filesystem::remove(dst2p);
}

void adjacent_hole_copy_file_sparse_linux(std::filesystem::path const &p) {
  auto srcp = p / "adj-copy_file_sparse-src.bin";
  auto dstp = p / "adj-copy_file_sparse-dst.bin";
  auto dst2p = p / "adj-copy_file_sparse-dst2.bin";

  sparse_copy_test(srcp, dstp, dst2p, [&](int fd) {
    std::cout << "punched holes:\n";

    if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 4096, 4096) == -1) {
      perror("fallocate (punch_holes)");
      std::exit(1);
    }

    std::cout << 4096 << ".." << (4096 * 2) << std::endl;

    if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 4096 * 2, 4096) == -1) {
      perror("fallocate (punch_holes)");
      std::exit(1);
    }

    std::cout << (4096 * 2) << ".." << (4096 * 3) << std::endl;
  });

  std::filesystem::remove(srcp);
  std::filesystem::remove(dstp);
  std::filesystem::remove(dst2p);
}

template<typename P>
void sparse_copy_test(std::filesystem::path const &srcp,
                      std::filesystem::path const &dstp,
                      std::filesystem::path const &dst2p,
                      P &&punch_holes) {
  std::filesystem::remove(srcp);
  std::filesystem::remove(dstp);
  std::filesystem::remove(dst2p);

  {
      int src = ::open(srcp.c_str(), O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      if (src == -1) {
        perror("open");
        std::exit(1);
      }

      fill_file(src);
      punch_holes(src);
      close(src);
    }

    { // copy using copy_file_sparse_linux
      int src = ::open(srcp.c_str(), O_RDONLY);
      if (src == -1) {
        perror("open");
        std::exit(1);
      }

      struct stat st;
      fstat(src, &st);

      int dst = ::open(dstp.c_str(), O_CREAT | O_WRONLY | O_TRUNC, st.st_mode);
      if (dst == -1) {
        perror("open");
        std::exit(1);
      }

      metall::mtlldetail::fcpdtl::copy_file_sparse_linux(src, dst, st.st_size);
      close(src);
      close(dst);
    }

    { // copy using cp
      std::stringstream cmd;
      cmd << "cp --sparse=always " << srcp << " " << dst2p;

      int res = std::system(cmd.str().c_str());
      assert(WIFEXITED(res));
    }

    int src = ::open(srcp.c_str(), O_RDONLY);
    if (src == -1) {
      perror("open");
      std::exit(1);
    }

    int dst = ::open(dstp.c_str(), O_RDONLY);
    if (dst == -1) {
      perror("open");
      std::exit(1);
    }

    int dst2 = ::open(dst2p.c_str(), O_RDONLY);
    if (dst2 == -1) {
      perror("open");
      std::exit(1);
    }

    auto holes_src = get_holes(src);
    auto holes_dst = get_holes(dst);
    auto holes_dst2 = get_holes(dst2);

    std::cout << "src holes:\n";
    list_holes(holes_src);
    std::cout << std::endl;

    std::cout << "dst holes:\n";
    list_holes(holes_dst);
    std::cout << std::endl;

    std::cout << "dst2 holes:\n";
    list_holes(holes_dst2);
    std::cout << std::endl;

    std::cout << "comparing src, dst" << std::endl;
    check_files_eq(src, dst);
    check_holes_eq(holes_src, holes_dst);

    // Not comparing holes to what cp produced because
    // it tries to find more holes in the file or extend existing ones
    std::cout << "comparing dst, dst2" << std::endl;
    check_files_eq(dst, dst2);

    std::cout << "comparing dst2, src" << std::endl;
    check_files_eq(dst2, src);

    std::cout << std::endl;
}

void fill_file(int fd) {
  std::vector<unsigned char> buf;
  std::uniform_int_distribution<unsigned char> dist{1, std::numeric_limits<unsigned char>::max()};

  for (size_t ix = 0; ix < FILE_SIZE; ++ix) {
    buf.push_back(dist(rng));
    assert(buf.back() != 0);
  }

  size_t bytes_written = write(fd, buf.data(), buf.size());
  assert(bytes_written == FILE_SIZE);

  struct stat st;
  fstat(fd, &st);
  assert(st.st_size == FILE_SIZE);

  auto [ptr, sz] = mmap(fd);
  for (size_t ix = 0; ix < static_cast<size_t>(sz); ++ix) {
    assert(ptr[ix] != 0);
  }
}

void punch_holes(int fd, bool hole_at_start, bool hole_at_end) {
  std::vector<std::pair<off_t, off_t>> holes;

  std::uniform_int_distribution<size_t> hole_num_dist{static_cast<size_t>(1 + hole_at_start + hole_at_end), 10};
  std::uniform_int_distribution<off_t> hole_size_dist{1, FILE_SIZE/10};
  std::uniform_int_distribution<off_t> hole_off_dist{0, FILE_SIZE - FILE_SIZE/10};

  std::cout << "punched holes:\n";

  auto num_holes = hole_num_dist(rng);
  for (size_t ix = 0; ix < num_holes - hole_at_end - hole_at_end; ++ix) {
    auto hole_start = hole_off_dist(rng);
    auto hole_size = hole_size_dist(rng);

    holes.emplace_back(hole_start, hole_start + hole_size);

    if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, hole_start, hole_size) == -1) {
      perror("fallocate (punch_holes)");
      std::exit(1);
    }
  }

  if (hole_at_start) {
    auto sz = hole_size_dist(rng);
    holes.emplace_back(0, sz);

    if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 0, sz) == -1) {
      perror("fallocate (punch_holes)");
      std::exit(1);
    }
  }

  if (hole_at_end) {
    auto sz = hole_size_dist(rng);
    holes.emplace_back(FILE_SIZE - sz, FILE_SIZE);

    if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, FILE_SIZE - sz, sz) == -1) {
      perror("fallocate (punch_holes)");
      std::exit(1);
    }
  }

  std::sort(holes.begin(), holes.end());
  for (auto const &[start, end] : holes) {
    std::cout << start << ".." << end << std::endl;
  }

  std::cout << std::endl;
}

std::pair<unsigned char const *, off_t> mmap(int fd) {
  struct stat st;
  int res = fstat(fd, &st);
  assert(res >= 0);

  auto *ptr = ::mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  assert(ptr != MAP_FAILED);

  return {static_cast<unsigned char const *>(ptr), st.st_size};
}

void check_files_eq(int a, int b) {
  auto am = mmap(a);
  auto a_ptr = am.first;
  auto a_size = am.second;

  auto bm = mmap(b);
  auto b_ptr = bm.first;
  auto b_size = bm.second;

  assert(a_size == b_size);

  for (size_t ix = 0; ix < static_cast<size_t>(a_size); ++ix) {
    if (a_ptr[ix] != b_ptr[ix]) {
      std::cerr << "found different bytes: " << std::hex << static_cast<int>(a_ptr[ix]) << " vs " << static_cast<int>(b_ptr[ix]) << std::endl;
      assert(false);
    }
  }
}

std::vector<std::pair<off_t, off_t>> get_holes(int fd) {
  lseek(fd, 0, SEEK_SET);
  std::vector<std::pair<off_t, off_t>> holes;

  off_t off = 0;
  off_t hole_start = 0;

  while (true) {
    hole_start = lseek(fd, off, SEEK_HOLE);
    off_t const hole_end = lseek(fd, hole_start, SEEK_DATA);

    if (hole_end == -1) {
      break;
    }

    off = hole_end;
    holes.emplace_back(hole_start, hole_end);
  }

  struct stat st;
  fstat(fd, &st);

  if (hole_start < st.st_size) {
    holes.emplace_back(hole_start, st.st_size);
  }

  return holes;
}

void list_holes(std::vector<std::pair<off_t, off_t>> const &holes) {
  for (auto const &hole : holes) {
    std::cout << "hole: " << hole.first << ".." << hole.second << std::endl;
  }
}

void check_holes_eq(std::vector<std::pair<off_t, off_t>> const &holes_a, std::vector<std::pair<off_t, off_t>> const &holes_b) {
  assert(holes_a.size() == holes_b.size());

  for (size_t ix = 0; ix < holes_a.size(); ++ix) {
    if (holes_a[ix] != holes_b[ix]) {
      std::cerr << "hole mismatch: " << holes_a[ix].first << ".." << holes_a[ix].second << " vs " << holes_b[ix].first << ".." << holes_b[ix].second << std::endl;
      assert(false);
    }
  }
}
