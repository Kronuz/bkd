/*
 * Copyright (c) 2026 Germán Méndez Bravo (Kronuz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Block KD trees over points of 1 to 8 dimensions of uint64_t keys, for range filters that
// touch many documents. A tree splits its points at the median of the widest dimension
// until a cell holds at most a leaf's worth; every node keeps its cell's bounding box, so
// a box query skips cells outside the box and reports cells inside it without comparing
// their points. Build and query use explicit stacks, never recursion.
//
// A 2-D tree over (start, end) answers interval intersection: a document's [a, b] meets
// [c, d] when a <= d and b >= c, the box {[0, d], [c, max]}.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace bkd {

inline constexpr unsigned max_dimensions = 8;
inline constexpr std::uint32_t default_leaf_size = 512;

template <unsigned D>
struct point {
	static_assert(D >= 1 && D <= max_dimensions);
	std::array<std::uint64_t, D> key;
	std::uint32_t ordinal;
};

// An inclusive box: lo[d] <= key[d] <= hi[d] in every dimension.
template <unsigned D>
struct box {
	std::array<std::uint64_t, D> lo, hi;
	static box all() noexcept {
		box b; b.lo.fill(0); b.hi.fill(std::numeric_limits<std::uint64_t>::max()); return b;
	}
	bool contains(const std::array<std::uint64_t, D>& k) const noexcept {
		for (unsigned d = 0; d < D; ++d) { if (k[d] < lo[d] || k[d] > hi[d]) { return false; } }
		return true;
	}
	bool inside(const box& outer) const noexcept {
		for (unsigned d = 0; d < D; ++d) { if (lo[d] < outer.lo[d] || hi[d] > outer.hi[d]) { return false; } }
		return true;
	}
	bool disjoint(const box& other) const noexcept {
		for (unsigned d = 0; d < D; ++d) { if (hi[d] < other.lo[d] || lo[d] > other.hi[d]) { return true; } }
		return false;
	}
};

template <unsigned D>
class tree {
public:
	// Builds over the points (moved in); leaf_size is a preset, at least 1.
	static tree build(std::vector<point<D>> points, std::uint32_t leaf_size = default_leaf_size) {
		if (!leaf_size) { throw std::invalid_argument("bkd: leaf size must be positive"); }
		if (points.size() > std::numeric_limits<std::uint32_t>::max()) { throw std::length_error("bkd: too many points"); }
		tree t; t.leaf_size_ = leaf_size; t.points_ = std::move(points);
		if (t.points_.empty()) { return t; }
		struct pending { std::uint32_t node, begin, end; };
		std::vector<pending> stack;
		t.nodes_.push_back({});
		stack.push_back({0, 0, std::uint32_t(t.points_.size())});
		while (!stack.empty()) {
			const auto [index, begin, end] = stack.back(); stack.pop_back();
			node n{}; n.begin = begin; n.end = end;
			n.cell = bounds(t.points_.data() + begin, t.points_.data() + end);
			if (end - begin > leaf_size) {
				unsigned dim = 0; std::uint64_t widest = 0;
				for (unsigned d = 0; d < D; ++d) {
					if (n.cell.hi[d] - n.cell.lo[d] > widest) { widest = n.cell.hi[d] - n.cell.lo[d]; dim = d; }
				}
				const std::uint32_t mid = begin + (end - begin) / 2;
				std::nth_element(t.points_.begin() + begin, t.points_.begin() + mid, t.points_.begin() + end,
					[dim](const point<D>& a, const point<D>& b) { return a.key[dim] < b.key[dim]; });
				n.left = std::uint32_t(t.nodes_.size()); t.nodes_.push_back({});
				n.right = std::uint32_t(t.nodes_.size()); t.nodes_.push_back({});
				stack.push_back({n.right, mid, end});
				stack.push_back({n.left, begin, mid});
			}
			t.nodes_[index] = n;
		}
		return t;
	}

	std::size_t size() const noexcept { return points_.size(); }
	std::uint32_t leaf_size() const noexcept { return leaf_size_; }
	std::size_t node_count() const noexcept { return nodes_.size(); }

	// Calls emit(ordinal) once per point inside the box, in tree order; a cell inside the
	// box is reported without comparing its points.
	template <typename Emit>
	void query(const box<D>& query_box, Emit&& emit) const {
		if (nodes_.empty()) { return; }
		std::vector<std::uint32_t> stack{0};
		while (!stack.empty()) {
			const node& n = nodes_[stack.back()]; stack.pop_back();
			if (n.cell.disjoint(query_box)) { continue; }
			if (n.cell.inside(query_box)) {
				for (auto i = n.begin; i < n.end; ++i) { emit(points_[i].ordinal); }
			} else if (!n.left) {
				for (auto i = n.begin; i < n.end; ++i) { if (query_box.contains(points_[i].key)) { emit(points_[i].ordinal); } }
			} else {
				stack.push_back(n.right); stack.push_back(n.left);
			}
		}
	}

	// Points inside the box, counting whole cells from their sizes.
	std::size_t count(const box<D>& query_box) const {
		std::size_t total = 0;
		if (nodes_.empty()) { return 0; }
		std::vector<std::uint32_t> stack{0};
		while (!stack.empty()) {
			const node& n = nodes_[stack.back()]; stack.pop_back();
			if (n.cell.disjoint(query_box)) { continue; }
			if (n.cell.inside(query_box)) { total += n.end - n.begin; }
			else if (!n.left) { for (auto i = n.begin; i < n.end; ++i) { total += query_box.contains(points_[i].key); } }
			else { stack.push_back(n.right); stack.push_back(n.left); }
		}
		return total;
	}

private:
	struct node {
		box<D> cell;
		std::uint32_t begin = 0, end = 0, left = 0, right = 0;  // left == 0: a leaf
	};
	static box<D> bounds(const point<D>* first, const point<D>* last) noexcept {
		box<D> b; b.lo.fill(std::numeric_limits<std::uint64_t>::max()); b.hi.fill(0);
		for (auto p = first; p != last; ++p) {
			for (unsigned d = 0; d < D; ++d) { b.lo[d] = std::min(b.lo[d], p->key[d]); b.hi[d] = std::max(b.hi[d], p->key[d]); }
		}
		return b;
	}
	std::vector<point<D>> points_;
	std::vector<node> nodes_;
	std::uint32_t leaf_size_ = default_leaf_size;
};

}  // namespace bkd
