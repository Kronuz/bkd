// bkd: box queries and counts against brute force for 1, 2, 3, and 8 dimensions, at
// several leaf sizes, over uniform, clustered, and duplicate-heavy points; and interval
// intersection through a 2-D tree over (start, end).
#include "bkd.hh"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>

static int failures = 0;
#define CHECK(cond) \
	do { \
		if (!(cond)) { std::fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); if (++failures > 20) std::exit(1); } \
	} while (0)

template <unsigned D>
static void against_brute_force(std::mt19937_64& rng) {
	for (std::uint32_t leaf : {1u, 7u, 64u, 512u}) {
		for (int shape = 0; shape < 3; ++shape) {
			const std::size_t n = rng() % 5000;
			std::vector<bkd::point<D>> points(n);
			for (std::uint32_t i = 0; i < n; ++i) {
				for (unsigned d = 0; d < D; ++d) {
					points[i].key[d] = shape == 0 ? rng() : shape == 1 ? (rng() % 8) * 1000 + rng() % 50 : rng() % 4;
				}
				points[i].ordinal = i;
			}
			const auto copy = points;
			const auto tree = bkd::tree<D>::build(points, leaf);
			CHECK(tree.size() == n);
			for (int q = 0; q < 50; ++q) {
				bkd::box<D> b = q == 0 ? bkd::box<D>::all() : bkd::box<D>{};
				if (q) {
					for (unsigned d = 0; d < D; ++d) {
						const auto a = copy.empty() ? rng() : copy[rng() % copy.size()].key[d] + (rng() % 3) - 1, c = copy.empty() ? rng() : copy[rng() % copy.size()].key[d];
						b.lo[d] = std::min(a, c); b.hi[d] = std::max(a, c);
					}
				}
				std::set<std::uint32_t> want, got;
				for (const auto& p : copy) { if (b.contains(p.key)) { want.insert(p.ordinal); } }
				std::size_t emitted = 0;
				tree.query(b, [&](std::uint32_t ordinal) { got.insert(ordinal); ++emitted; });
				CHECK(got == want && emitted == want.size());
				CHECK(tree.count(b) == want.size());
			}
		}
	}
}

int main() {
	std::mt19937_64 rng(2026);
	against_brute_force<1>(rng);
	against_brute_force<2>(rng);
	against_brute_force<3>(rng);
	against_brute_force<8>(rng);
	// Interval intersection: [a, b] meets [c, d] when a <= d and b >= c.
	{
		std::vector<bkd::point<2>> intervals;
		std::vector<std::pair<std::uint64_t, std::uint64_t>> spans;
		for (std::uint32_t i = 0; i < 3000; ++i) {
			const auto a = rng() % 100000, len = rng() % (i % 10 ? 100 : 20000);
			intervals.push_back({{a, a + len}, i}); spans.push_back({a, a + len});
		}
		const auto tree = bkd::tree<2>::build(intervals, 32);
		for (int q = 0; q < 200; ++q) {
			const auto c = rng() % 100000, d = c + rng() % 500;
			bkd::box<2> b{{0, c}, {d, std::numeric_limits<std::uint64_t>::max()}};
			std::size_t want = 0;
			for (const auto& [a, e] : spans) { want += a <= d && e >= c; }
			CHECK(tree.count(b) == want);
		}
	}
	// An empty tree and a zero leaf size.
	{
		const auto empty = bkd::tree<3>::build({});
		std::size_t n = 0; empty.query(bkd::box<3>::all(), [&](std::uint32_t) { ++n; });
		CHECK(n == 0 && empty.count(bkd::box<3>::all()) == 0);
		bool threw = false;
		try { (void)bkd::tree<1>::build({}, 0); } catch (const std::invalid_argument&) { threw = true; }
		CHECK(threw);
	}
	std::printf(failures ? "bkd: FAILED\n" : "bkd: ok\n");
	return failures ? 1 : 0;
}
