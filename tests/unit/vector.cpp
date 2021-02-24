/*
	TODO:
		-exception handling (for example when an element's ctor fails in vector's ctor)
*/

#include <cmath>
#include <jpl/vector.hpp>
#include <vector>
#include <string>
#include <tuple>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

template<class T>
void test_empty_ctor(int capacity) {
	T vec;
	CHECK(vec.size() == 0);
	CHECK(vec.capacity() == capacity);
	CHECK(vec.empty() == true);
}

TEST_CASE("empty ctor") {
	test_empty_ctor<::jpl::vector<int>>(0);
	test_empty_ctor<::jpl::vector<int, 5>>(5);
	test_empty_ctor<::jpl::vector<::std::string>>(0);
	test_empty_ctor<::jpl::vector<::std::string, 5>>(5);
}

template<class T>
void test_default_ctor(::size_t size) {
	T vec{ size };
	CHECK(vec.size() == size);
	CHECK(vec.size_bytes() == (size * sizeof(typename T::value_type)));
	CHECK(vec.empty() == (size == 0));
}

TEST_CASE("default ctor") {
	test_default_ctor<::jpl::vector<int>>(0);
	test_default_ctor<::jpl::vector<int>>(5);
	test_default_ctor<::jpl::vector<int, 2>>(0);
	test_default_ctor<::jpl::vector<int, 2>>(5);
	test_default_ctor<::jpl::vector<int, 5>>(0);
	test_default_ctor<::jpl::vector<int, 5>>(5);
	test_default_ctor<::jpl::vector<::std::string>>(0);
	test_default_ctor<::jpl::vector<::std::string>>(5);
	test_default_ctor<::jpl::vector<::std::string, 2>>(0);
	test_default_ctor<::jpl::vector<::std::string, 2>>(5);
	test_default_ctor<::jpl::vector<::std::string, 5>>(0);
	test_default_ctor<::jpl::vector<::std::string, 5>>(5);
}

template<class T>
void test_capacity_ctor() {
	T vec{ ::jpl::capacity, 10 };
	CHECK(vec.size() == 0);
	CHECK(vec.capacity() == 10);
	CHECK(vec.empty() == true);
}

TEST_CASE("capacity ctor") {
	test_capacity_ctor<::jpl::vector<int>>();
	test_capacity_ctor<::jpl::vector<int, 5>>();
	test_capacity_ctor<::jpl::vector<::std::string>>();
	test_capacity_ctor<::jpl::vector<::std::string, 5>>();
}

template<class V, class T>
void test_list_ctor(T a1, T a2, T a3) {
	V vec{ ::jpl::list, a1, a2, a3 };
	CHECK(vec.size() == 3);
	CHECK(vec.capacity() == ::std::max(V::sbo_size, 3lu));
	CHECK(vec.empty() == false);
	std::vector correct{ a1, a2, a3 };
	CHECK(::std::equal(vec.begin(), vec.end(), correct.begin(), correct.end()));
}

TEST_CASE("list ctor") {
	test_list_ctor<::jpl::vector<int>>(1, 2, 3);
	test_list_ctor<::jpl::vector<int, 5>>(1, 2, 3);
	test_list_ctor<::jpl::vector<int, 2>>(1, 2, 3);
	test_list_ctor<::jpl::vector<::std::string>>("1", "2", "3");
	test_list_ctor<::jpl::vector<::std::string, 5>>("1", "2", "3");
	test_list_ctor<::jpl::vector<::std::string, 2>>("1", "2", "3");
}

template<class T, int sbo>
void test_range_ctor(const ::std::vector<T>& copy_vec) {
	::jpl::vector<T, sbo> vec1{ copy_vec };
	CHECK(vec1.size() == copy_vec.size());
	CHECK(::std::equal(::std::cbegin(vec1), ::std::cend(vec1), ::std::cbegin(copy_vec), ::std::cend(copy_vec)));
	CHECK(vec1.empty() == false);

	::std::vector<T> move_vec{ copy_vec };
	::jpl::vector<T, sbo> vec2{ ::std::move(move_vec) };
	CHECK(vec2.size() == copy_vec.size());
	CHECK(::std::equal(::std::cbegin(vec2), ::std::cend(vec2), ::std::cbegin(copy_vec), ::std::cend(copy_vec)));
	CHECK(vec2.empty() == false);
}

TEST_CASE("range ctor") {
	test_range_ctor<int, 0>({ 1, 2, 3 });
	test_range_ctor<int, 2>({ 1, 2, 3 });
	test_range_ctor<int, 5>({ 1, 2, 3 });
	test_range_ctor<::std::string, 0>({ "1", "2", "3" });
	test_range_ctor<::std::string, 2>({ "1", "2", "3" });
	test_range_ctor<::std::string, 5>({ "1", "2", "3" });
}

template<class T, int sbo, int sbo2>
void test_vector_ctor(const ::jpl::vector<T, sbo2>& copy_vec) {
	::jpl::vector<T, sbo> vec1{ copy_vec };
	CHECK(vec1.size() == copy_vec.size());
	CHECK(::std::equal(::std::cbegin(vec1), ::std::cend(vec1), ::std::cbegin(copy_vec), ::std::cend(copy_vec)));
	CHECK(vec1.empty() == false);

	auto move_vec = copy_vec;
	::jpl::vector<T, sbo> vec2{ ::std::move(move_vec) };
	CHECK(vec2.size() == copy_vec.size());
	CHECK(::std::equal(::std::cbegin(vec2), ::std::cend(vec2), ::std::cbegin(copy_vec), ::std::cend(copy_vec)));
	CHECK(vec2.empty() == false);
}

TEST_CASE("other vector ctor") {
	::jpl::vector<int, 0> vec_int_0{ ::jpl::list, 1, 2, 3 };
	::jpl::vector<int, 2> vec_int_2{ ::jpl::list, 1, 2, 3 };
	::jpl::vector<int, 5> vec_int_5{ ::jpl::list, 1, 2, 3 };
	::jpl::vector<::std::string, 0> vec_string_0{ ::jpl::list, "1", "2", "3" };
	::jpl::vector<::std::string, 2> vec_string_2{ ::jpl::list, "1", "2", "3" };
	::jpl::vector<::std::string, 5> vec_string_5{ ::jpl::list, "1", "2", "3" };
	test_vector_ctor<int, 0, 0>(vec_int_0);
	test_vector_ctor<int, 0, 2>(vec_int_2);
	test_vector_ctor<int, 0, 5>(vec_int_5);
	test_vector_ctor<int, 2, 0>(vec_int_0);
	test_vector_ctor<int, 2, 2>(vec_int_2);
	test_vector_ctor<int, 2, 5>(vec_int_5);
	test_vector_ctor<int, 5, 0>(vec_int_0);
	test_vector_ctor<int, 5, 2>(vec_int_2);
	test_vector_ctor<int, 5, 5>(vec_int_5);
}

template<class V, class T>
void test_insert(T a1, T a2, T a3, T a4, T a5, T a6) {
	// Insert at beginning, end, and somewhere in between.
	// Do all with l-value and r-value.
	V vec{ ::jpl::list, a1, a2, a3 };
	::std::vector<T> expected{ a1, a2, a3 };
	[[maybe_unused]] auto it1 = vec.insert(vec.begin(), a4);
	[[maybe_unused]] auto it2 = expected.insert(expected.begin(), a4);
	CHECK(*it1 == *it2);
	it1 = vec.insert(vec.begin(), T{a4});
	it2 = expected.insert(expected.begin(), T{a4});
	CHECK(*it1 == *it2);
	it1 = vec.insert(vec.end(), a5);
	it2 = expected.insert(expected.end(), a5);
	CHECK(*it1 == *it2);
	it1 = vec.insert(vec.end(), T{a5});
	it2 = expected.insert(expected.end(), T{a5});
	CHECK(*it1 == *it2);
	it1 = vec.insert(vec.begin() + 2, a6);
	it2 = expected.insert(expected.begin() + 2, a6);
	CHECK(*it1 == *it2);
	it1 = vec.insert(vec.begin() + 2, T{a6});
	it2 = expected.insert(expected.begin() + 2, T{a6});
	CHECK(*it1 == *it2);
	CHECK(::std::equal(vec.cbegin(), vec.cend(), expected.cbegin(), expected.cend()));
}

TEST_CASE("insert") {
	test_insert<::jpl::vector<int>>(1, 2, 3, 4, 5, 6);
	test_insert<::jpl::vector<int, 5>>(1, 2, 3, 4, 5, 6);
	test_insert<::jpl::vector<::std::string>>("1", "2", "3", "4", "5", "6");
	test_insert<::jpl::vector<::std::string, 5>>("1", "2", "3", "4", "5", "6");
}

template<class V, class T, class Gen>
void test_predicate(Gen&& generator, ::std::vector<T>&& expected1, ::std::vector<T>&& expected2) {
	V vec{ 3, generator };
	CHECK(vec.size() == 3);
	CHECK(::std::equal(vec.cbegin(), vec.cend(), expected1.cbegin(), expected1.cend()));
	vec.resize(6, generator);
	CHECK(vec.size() == 6);
	CHECK(::std::equal(vec.cbegin(), vec.cend(), expected2.cbegin(), expected2.cend()));
	vec.resize(3, generator);
	CHECK(vec.size() == 3);
	CHECK(::std::equal(vec.cbegin(), vec.cend(), expected1.cbegin(), expected1.cend()));
	vec.resize(3, generator);
	CHECK(vec.size() == 3);
	CHECK(::std::equal(vec.cbegin(), vec.cend(), expected1.cbegin(), expected1.cend()));
}

TEST_CASE("predicate constructor and resize | no index") {
	int call_count{ 0 };
	auto generator = [&call_count](){ return ++call_count; };
	test_predicate<::jpl::vector<int>, int>(generator, { 1, 2, 3 }, { 1, 2, 3, 4, 5, 6 });
	CHECK(call_count == 6); call_count = 0;
	test_predicate<::jpl::vector<int, 5>, int>(generator, { 1, 2, 3 }, { 1, 2, 3, 4, 5, 6 });
	auto str_generator = [&call_count](){ return ::std::to_string(++call_count); };
	CHECK(call_count == 6); call_count = 0;
	test_predicate<::jpl::vector<::std::string>, ::std::string>(str_generator, { "1", "2", "3" }, { "1", "2", "3", "4", "5", "6" });
	CHECK(call_count == 6); call_count = 0;
	test_predicate<::jpl::vector<::std::string, 5>, ::std::string>(str_generator, { "1", "2", "3" }, { "1", "2", "3", "4", "5", "6" });
	CHECK(call_count == 6);
}

TEST_CASE("predicate constructor and resize 2 | with index") {
	auto generator = [](int i){ return i + 1; };
	test_predicate<::jpl::vector<int>, int>(generator, { 1, 2, 3 }, { 1, 2, 3, 4, 5, 6 });
	test_predicate<::jpl::vector<int, 5>, int>(generator, { 1, 2, 3 }, { 1, 2, 3, 4, 5, 6 });
	auto str_generator = [](int i){ return ::std::to_string(i + 1); };
	test_predicate<::jpl::vector<::std::string>, ::std::string>(str_generator, { "1", "2", "3" }, { "1", "2", "3", "4", "5", "6" });
	test_predicate<::jpl::vector<::std::string, 5>, ::std::string>(str_generator, { "1", "2", "3" }, { "1", "2", "3", "4", "5", "6" });
}

template<class V>
void test_resize() {
	V vec;
	vec.resize(10);
	CHECK(vec.size() == 10);
	CHECK(vec.capacity() == 10);
	if constexpr (!std::is_trivial_v<typename V::value_type>) {
		std::vector<typename V::value_type> vec2(10);
		CHECK(std::equal(vec.begin(), vec.end(), vec2.begin()));
	}
	vec.resize(2);
	CHECK(vec.size() == 2);
	CHECK(vec.capacity() == 10);
	if constexpr (!std::is_trivial_v<typename V::value_type>) {
		std::vector<typename V::value_type> vec2(2);
		CHECK(std::equal(vec.begin(), vec.end(), vec2.begin()));
	}
}

template<class V, class T>
void test_resize_with_val(T val) {
	V vec;
	vec.resize(10, val);
	CHECK(vec.size() == 10);
	CHECK(vec.capacity() == 10);
	std::vector<T> expected(10, val);
	CHECK(std::equal(vec.begin(), vec.end(), expected.begin()));
	vec.resize(2, val);
	CHECK(vec.size() == 2);
	CHECK(vec.capacity() == 10);
	CHECK(std::equal(vec.begin(), vec.end(), expected.begin()));
}

TEST_CASE("resize") {
	SUBCASE("default") {
		test_resize<jpl::vector<int>>();
		test_resize<jpl::vector<int, 5>>();
		test_resize<jpl::vector<std::string>>();
		test_resize<jpl::vector<std::string, 5>>();
	}
	SUBCASE("with val") {
		test_resize_with_val<jpl::vector<int>, int>(5);
		test_resize_with_val<jpl::vector<int, 5>, int>(5);
		test_resize_with_val<jpl::vector<std::string>, std::string>("Hello");
		test_resize_with_val<jpl::vector<std::string, 5>, std::string>("Hello");
	}
}

struct C {
	int64_t ctor{ 0 };
	int64_t dtor{ 0 };
	int64_t move{ 0 };
};
bool operator==(const C& l, const C& r) noexcept {
	return (l.ctor == r.ctor)
	    && (l.dtor == r.dtor)
	    && (l.move == r.move);
}
template<C& count>
struct test {
	test() {
		count.ctor++;
	}
	~test() {
		count.dtor++;
	}
	test(test&&) {
		count.move++;
	}
};

static C count;
template<int sbo_size>
void test_count() {
	count = { 0, 0, 0 };
	::jpl::vector<test<count>, sbo_size> vec(5);
	CHECK(count == C{ .ctor =  5, .dtor =  0, .move = 0 });
	vec.resize(10);
	CHECK(count == C{ .ctor = 10, .dtor =  5, .move = 5 });
	vec.resize(5);
	CHECK(count == C{ .ctor = 10, .dtor = 10, .move = 5 });
}

TEST_CASE("object management") {
	test_count<0>();
	test_count<5>();
}

template<class V, class T>
void test_move_copy_ctor(int capacity, T a1, T a2, T a3) {
	V vec{ ::jpl::list, a1, a2, a3 };
	CHECK(vec.size() == 3);
	CHECK(vec.capacity() == ::std::max(capacity, 3));
	::std::vector<T> expected{ a1, a2, a3 };

	SUBCASE("ctor from_copy") {
		V from_copy = vec;
		CHECK(::std::equal(from_copy.cbegin(), from_copy.cend(), expected.cbegin(), expected.cend()));
		CHECK(from_copy.size() == 3);
		CHECK(from_copy.capacity() == ::std::max(capacity, 3));
	}

	SUBCASE("ctor from move") {
		V from_move = ::std::move(vec);
		CHECK(::std::equal(from_move.cbegin(), from_move.cend(), expected.cbegin(), expected.cend()));
		CHECK(from_move.size() == 3);
		CHECK(from_move.capacity() == ::std::max(capacity, 3));
	}

	SUBCASE("ctor from copy (empty)") {
		V empty;
		V copy_from_empty = empty;
		CHECK(copy_from_empty.size() == 0);
		CHECK(copy_from_empty.empty());
		CHECK(copy_from_empty.capacity() == V::sbo_size);
	}

	SUBCASE("ctor from move (empty)") {
		V empty;
		V move_from_empty = std::move(empty);
		CHECK(move_from_empty.size() == 0);
		CHECK(move_from_empty.empty());
		CHECK(move_from_empty.capacity() == V::sbo_size);
	}
}

TEST_CASE("move/copy ctor") {
	test_move_copy_ctor<::jpl::vector<int>, int>(3, 1, 2, 3);
	test_move_copy_ctor<::jpl::vector<int, 5>, int>(5, 1, 2, 3);
	test_move_copy_ctor<::jpl::vector<int, 2>, int>(2, 1, 2, 3);
	test_move_copy_ctor<::jpl::vector<::std::string>, ::std::string>(3, "1", "2", "3");
	test_move_copy_ctor<::jpl::vector<::std::string, 5>, ::std::string>(5, "1", "2", "3");
	test_move_copy_ctor<::jpl::vector<::std::string, 2>, ::std::string>(2, "1", "2", "3");
}

template<class V, class T>
void test_move_copy_assign(int capacity, T a1, T a2, T a3) {
	V vec{ ::jpl::list, a1, a2, a3 };
	CHECK(vec.size() == 3);
	CHECK(vec.capacity() == ::std::max(capacity, 3));
	::std::vector<T> expected{ a1, a2, a3 };

	V from_copy;
	from_copy = vec;
	CHECK(::std::equal(from_copy.cbegin(), from_copy.cend(), expected.cbegin(), expected.cend()));
	CHECK(from_copy.size() == 3);
	CHECK(from_copy.capacity() == ::std::max(capacity, 3));

	V from_copy2{ ::jpl::list, a3, a2, a1, a1, a2, a3 };
	from_copy2 = vec;
	CHECK(::std::equal(from_copy2.cbegin(), from_copy2.cend(), expected.cbegin(), expected.cend()));
	CHECK(from_copy2.size() == 3);
	CHECK(from_copy2.capacity() == ::std::max(capacity, 6));

	V from_copy3{ ::jpl::list, a1, a2, a3 };
	from_copy3.reserve(::jpl::sic, 10);
	from_copy3 = vec;
	CHECK(::std::equal(from_copy3.cbegin(), from_copy3.cend(), expected.cbegin(), expected.cend()));
	CHECK(from_copy3.size() == 3);
	CHECK(from_copy3.capacity() == ::std::max(capacity, 10));
	
	V from_move;
	from_move = ::std::move(V{vec});
	CHECK(::std::equal(from_move.cbegin(), from_move.cend(), expected.cbegin(), expected.cend()));
	CHECK(from_move.size() == 3);
	CHECK(from_move.capacity() == ::std::max(capacity, 3));

	V from_move2{ ::jpl::list, a3, a2, a1, a1, a2, a3 };
	from_move2 = ::std::move(V{vec});
	CHECK(::std::equal(from_move2.cbegin(), from_move2.cend(), expected.cbegin(), expected.cend()));
	CHECK(from_move2.size() == 3);
	// Move-assign will dealloc current buffer and take buffer from the moved-from vector,
	// unless the moved-from vector uses SBO.
	CHECK(from_move2.capacity() == (capacity > 3 ? 6 : ::std::max(capacity, 3)));

	V from_move3{ ::jpl::list, a1, a2, a3 };
	from_move3.reserve(::jpl::sic, 10);
	from_move3 = ::std::move(V{vec});
	CHECK(::std::equal(from_move3.cbegin(), from_move3.cend(), expected.cbegin(), expected.cend()));
	CHECK(from_move3.size() == 3);
	CHECK(from_move3.capacity() == (capacity > 3 ? 10 : ::std::max(capacity, 3)));
}

TEST_CASE("move/copy assign") {
	test_move_copy_assign<::jpl::vector<int>, int>(3, 1, 2, 3);
	test_move_copy_assign<::jpl::vector<int, 5>, int>(5, 1, 2, 3);
	test_move_copy_assign<::jpl::vector<int, 2>, int>(2, 1, 2, 3);
	test_move_copy_assign<::jpl::vector<::std::string>, ::std::string>(3, "1", "2", "3");
	test_move_copy_assign<::jpl::vector<::std::string, 5>, ::std::string>(5, "1", "2", "3");
	test_move_copy_assign<::jpl::vector<::std::string, 2>, ::std::string>(2, "1", "2", "3");
}

TEST_CASE("append") {
	::jpl::vector<int, 10> vec{ ::jpl::list, 1, 2, 3, 4, 5 };
	::jpl::vector<int> vec2{ ::jpl::list, 6, 7, 8, 9, 10 };
	vec.append(vec2);
	::std::vector<int> expected{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	CHECK(::std::equal(vec.cbegin(), vec.cend(), expected.cbegin(), expected.cend()));
	CHECK(vec.size() == 10);
}

TEST_CASE("operator +") {
	::jpl::vector<::std::string, 10> vec{ ::jpl::list, "1", "2", "3", "4" };
	::jpl::vector<::std::string> vec2{ ::jpl::list, "5", "6", "7", "8" };
	auto vec3 = vec + vec2;
	::std::vector<::std::string> expected{ "1", "2", "3", "4", "5", "6", "7", "8" };
	CHECK(::std::equal(vec3.cbegin(), vec3.cend(), expected.cbegin(), expected.cend()));
	CHECK(vec3.size() == 8);
	CHECK(vec3.capacity() == 8);
}

TEST_CASE("reverse iterator") {
	::jpl::vector<int> vec{ ::jpl::list, 1, 2, 3 };
	::std::vector<int> reversed{ 3, 2, 1 };
	CHECK(::std::equal(vec.rbegin(), vec.rend(), reversed.cbegin(), reversed.cend()));
	CHECK(::std::equal(vec.crbegin(), vec.crend(), reversed.cbegin(), reversed.cend()));

	const ::jpl::vector<int> cvec{ ::jpl::list, 1, 2, 3 };
	CHECK(::std::equal(cvec.rbegin(), cvec.rend(), reversed.cbegin(), reversed.cend()));
}

// TODO: add more cases
TEST_CASE("swap") {
	::jpl::vector<int> vec1{ ::jpl::list, 1, 2, 3 };
	::jpl::vector<int> vec2{ ::jpl::list, 4, 5, 6 };
	::std::vector<int> orig_vec1{ 1, 2, 3 };
	::std::vector<int> orig_vec2{ 4, 5, 6 };
	vec1.swap(vec2);
	CHECK(::std::equal(vec1.begin(), vec1.end(), orig_vec2.cbegin(), orig_vec2.cend()));
	CHECK(::std::equal(vec2.begin(), vec2.end(), orig_vec1.cbegin(), orig_vec1.cend()));
}

TEST_CASE("erase") {
	::jpl::vector<int> vec1{ ::jpl::list, 1, 2, 3 };
	vec1.erase(vec1.begin() + 1);
	::std::vector<int> expected1{ 1, 3 };
	CHECK(::std::equal(vec1.begin(), vec1.end(), expected1.cbegin(), expected1.cend()));

	::jpl::vector<::std::string> vec2{ ::jpl::list, "1", "2", "3" };
	vec2.erase(vec2.begin() + 1);
	::std::vector<::std::string> expected2{ "1", "3" };
	CHECK(::std::equal(vec2.begin(), vec2.end(), expected2.cbegin(), expected2.cend()));
}

TEST_CASE("erase range") {
	::jpl::vector<int> vec1{ ::jpl::list, 1, 2, 3, 4, 5 };
	vec1.erase(vec1.begin() + 1, vec1.begin() + 3);
	::std::vector<int> expected1{ 1, 4, 5 };
	CHECK(::std::equal(vec1.begin(), vec1.end(), expected1.cbegin(), expected1.cend()));

	::jpl::vector<::std::string> vec2{ ::jpl::list, "1", "2", "3", "4", "5" };
	vec2.erase(vec2.begin() + 1, vec2.begin() + 3);
	::std::vector<::std::string> expected2{ "1", "4", "5" };
	CHECK(::std::equal(vec2.begin(), vec2.end(), expected2.cbegin(), expected2.cend()));
}

TEST_CASE("shrink_to_fit") {
	SUBCASE("trivial no sbo") {
		::jpl::vector<int> vec     { 10, [](::size_t i){ return i + 1; } };
		::jpl::vector<int> expected{  5, [](::size_t i){ return i + 1; } };
		vec.resize(5);
		vec.shrink_to_fit();
		CHECK(::std::equal(vec.begin(), vec.end(), expected.begin(), expected.end()));
		vec.clear();
		vec.shrink_to_fit();
		CHECK(vec.empty());
		CHECK(vec.size() == 0);
		CHECK(vec.capacity() == decltype(vec)::sbo_size);
	}
	SUBCASE("trivial sbo to sbo") {
		::jpl::vector<int, 10> vec { 10, [](::size_t i){ return i + 1; } };
		::jpl::vector<int> expected{  5, [](::size_t i){ return i + 1; } };
		vec.resize(5);
		vec.shrink_to_fit();
		CHECK(::std::equal(vec.begin(), vec.end(), expected.begin(), expected.end()));
		vec.clear();
		vec.shrink_to_fit();
		CHECK(vec.empty());
		CHECK(vec.size() == 0);
		CHECK(vec.capacity() == decltype(vec)::sbo_size);
	}
	SUBCASE("trivial no sbo to sbo") {
		::jpl::vector<int, 5> vec  { 10, [](::size_t i){ return i + 1; } };
		::jpl::vector<int> expected{  5, [](::size_t i){ return i + 1; } };
		vec.resize(5);
		vec.shrink_to_fit();
		CHECK(::std::equal(vec.begin(), vec.end(), expected.begin(), expected.end()));
		vec.clear();
		vec.shrink_to_fit();
		CHECK(vec.empty());
		CHECK(vec.size() == 0);
		CHECK(vec.capacity() == decltype(vec)::sbo_size);
	}
	SUBCASE("non-trivial no sbo") {
		::jpl::vector<std::string> vec     { 10, [](::size_t i){ return std::to_string(i + 1); } };
		::jpl::vector<std::string> expected{  5, [](::size_t i){ return std::to_string(i + 1); } };
		vec.resize(5);
		vec.shrink_to_fit();
		CHECK(::std::equal(vec.begin(), vec.end(), expected.begin(), expected.end()));
		vec.clear();
		vec.shrink_to_fit();
		CHECK(vec.empty());
		CHECK(vec.size() == 0);
		CHECK(vec.capacity() == decltype(vec)::sbo_size);
	}
	SUBCASE("non-trivial sbo to sbo") {
		::jpl::vector<std::string, 10> vec { 10, [](::size_t i){ return std::to_string(i + 1); } };
		::jpl::vector<std::string> expected{  5, [](::size_t i){ return std::to_string(i + 1); } };
		vec.resize(5);
		vec.shrink_to_fit();
		CHECK(::std::equal(vec.begin(), vec.end(), expected.begin(), expected.end()));
		vec.clear();
		vec.shrink_to_fit();
		CHECK(vec.empty());
		CHECK(vec.size() == 0);
		CHECK(vec.capacity() == decltype(vec)::sbo_size);
	}
	SUBCASE("non-trivial no sbo to sbo") {
		::jpl::vector<std::string, 5> vec  { 10, [](::size_t i){ return std::to_string(i + 1); } };
		::jpl::vector<std::string> expected{  5, [](::size_t i){ return std::to_string(i + 1); } };
		vec.resize(5);
		vec.shrink_to_fit();
		CHECK(::std::equal(vec.begin(), vec.end(), expected.begin(), expected.end()));
		vec.clear();
		vec.shrink_to_fit();
		CHECK(vec.empty());
		CHECK(vec.size() == 0);
		CHECK(vec.capacity() == decltype(vec)::sbo_size);
	}
}
