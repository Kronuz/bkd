// libFuzzer target: points and boxes read from the input; a 2-D and a 3-D tree at a leaf
// size the input names answer every box exactly as brute force does.
#include "bkd.hh"

#include <cstdlib>
#include <cstring>

template <unsigned D>
static void check(const std::uint8_t* data, std::size_t size) {
	if (size < 1) { return; }
	const std::uint32_t leaf = 1 + data[0] % 16;
	data += 1; size -= 1;
	auto read = [&](std::uint64_t& v) { v = 0; if (size >= 2) { std::uint16_t x; std::memcpy(&x, data, 2); v = x % 512; data += 2; size -= 2; } };
	std::vector<bkd::point<D>> points;
	for (std::uint32_t i = 0; size >= 2 * D && i < 2000; ++i) { bkd::point<D> p{}; for (unsigned d = 0; d < D; ++d) { read(p.key[d]); } p.ordinal = i; points.push_back(p); }
	const auto copy = points;
	const auto tree = bkd::tree<D>::build(std::move(points), leaf);
	for (int q = 0; q < 8; ++q) {
		bkd::box<D> b{};
		for (unsigned d = 0; d < D; ++d) { std::uint64_t x, y; read(x); read(y); b.lo[d] = std::min(x, y); b.hi[d] = std::max(x, y); }
		std::size_t want = 0; for (const auto& p : copy) { want += b.contains(p.key); }
		std::size_t got = 0; tree.query(b, [&](std::uint32_t) { ++got; });
		if (got != want || tree.count(b) != want) { std::abort(); }
	}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	check<2>(data, size);
	check<3>(data, size);
	return 0;
}
