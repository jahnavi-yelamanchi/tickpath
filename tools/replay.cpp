// tickpath-replay: replays a framed ITCH file into a book and prints the
// message count, book status counts and the final state hash.
//
//   tickpath-replay [--book reference|flat|tree] FILE
//
// FILE is uncompressed (gunzip sample days first). Exit code 1 if the parser
// stops before the end of the file.

#include "tickpath/book/replay.hpp"

#include <cstddef>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "tickpath/book/flat_book.hpp"
#include "tickpath/book/reference_book.hpp"
#include "tickpath/book/state_hash.hpp"
#include "tickpath/book/tree_book.hpp"

namespace {

class MappedFile {
public:
    explicit MappedFile(const std::string& path)
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): POSIX open
        : fd_(::open(path.c_str(), O_RDONLY | O_CLOEXEC)) {
        if (fd_ < 0) {
            throw std::runtime_error("cannot open " + path);
        }
        struct stat st {};
        if (::fstat(fd_, &st) != 0) {
            throw std::runtime_error("cannot stat " + path);
        }
        size_ = static_cast<std::size_t>(st.st_size);
        if (size_ > 0) {
            map_ = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
            if (map_ == MAP_FAILED) { // NOLINT(performance-no-int-to-ptr)
                map_ = nullptr;
                throw std::runtime_error("cannot mmap " + path);
            }
        }
    }
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&&) = delete;
    MappedFile& operator=(MappedFile&&) = delete;
    ~MappedFile() {
        if (map_ != nullptr) {
            ::munmap(map_, size_);
        }
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return {static_cast<const std::byte*>(map_), size_};
    }

private:
    int fd_ = -1;
    std::size_t size_ = 0;
    void* map_ = nullptr;
};

template <class B>
int run(std::span<const std::byte> bytes, std::string_view name) {
    auto book = std::make_unique<B>(tickpath::book::BookConfig{});
    const auto r = tickpath::book::replay(bytes, *book);
    std::cout << "book=" << name << " parse=" << tickpath::itch::to_string(r.frames.status)
              << " consumed=" << r.frames.consumed << "/" << bytes.size()
              << " messages=" << r.frames.messages << " skipped=" << r.frames.skipped << '\n';
    for (std::size_t i = 0; i < r.statuses.size(); ++i) {
        if (r.statuses[i] != 0) {
            std::cout << "  " << tickpath::book::to_string(static_cast<tickpath::book::Status>(i))
                      << "=" << r.statuses[i] << '\n';
        }
    }
    std::cout << "orders=" << book->order_count() << " state_hash=" << std::hex << std::setw(16)
              << std::setfill('0') << tickpath::book::state_hash(*book) << std::dec << '\n';
    return r.frames.status == tickpath::itch::ParseStatus::Ok ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    std::string_view book = "reference";
    std::string path;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--book" && i + 1 < args.size()) {
            book = args[++i];
        } else {
            path = std::string(args[i]);
        }
    }
    if (path.empty()) {
        std::cerr << "usage: tickpath-replay [--book reference|flat|tree] FILE\n";
        return 2;
    }
    try {
        const MappedFile f(path);
        if (book == "reference") {
            return run<tickpath::book::ReferenceBook>(f.bytes(), book);
        }
        if (book == "flat") {
            return run<tickpath::book::FlatBook>(f.bytes(), book);
        }
        if (book == "tree") {
            return run<tickpath::book::TreeBook>(f.bytes(), book);
        }
        std::cerr << "unknown book: " << book << '\n';
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
