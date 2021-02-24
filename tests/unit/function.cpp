#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>
#include <jpl/function.hpp>
#include <fmt/core.h>

bool free_function() {
	return true;
}

TEST_CASE("<bool()> free function with template type deduction") {
	jpl::function func{ free_function };
	CHECK(func());
}

TEST_CASE("<void()> lambda no capture") {
	static bool called{ false };
	jpl::function<void()> func{ []{ called = true; } };
	func();
	CHECK(called);
	called = false;
	const auto cfunc = std::move(func);
	cfunc();
	CHECK(called);
}

TEST_CASE("<int()> lambda no capture") {
	static bool called{ false };
	jpl::function<int()> func{ []{ called = true; return 5; } };
	CHECK(func() == 5);
	CHECK(called);
}

TEST_CASE("<int(int, int)> lambda no capture") {
	jpl::function<int(int, int)> func{ [](int val1, int val2){ return val1 + val2; } };
	CHECK(func(2, 3) == 5);
}

TEST_CASE("<std::string()> lambda no capture") {
	static bool called{ false };
	jpl::function<std::string()> func{ []() -> std::string { called = true; return "5"; } };
	CHECK(func() == "5");
	CHECK(called);
}

TEST_CASE("<std::string(std::string, std::string)> lambda no capture") {
	jpl::function<std::string(std::string, std::string)> func{ [](std::string str1, std::string str2) -> std::string {
		return str1 + str2;
	} };
	CHECK(func("Hel", "lo") == "Hello");
}

TEST_CASE("<void()> lambda with capture") {
	bool called{ false };
	jpl::function<void()> func{ [&]{ called = true; } };
	func();
	CHECK(called);
}

TEST_CASE("<int()> lambda with capture") {
	bool called{ false };
	int val{ 5 };
	jpl::function<int()> func{ [&, val]{ called = true; return val; } };
	CHECK(func() == val);
	CHECK(called);
}

TEST_CASE("<int(int, int)> lambda with capture") {
	bool called{ false };
	int val{ 5 };
	jpl::function<int(int, int)> func{ [&, val](int val1, int val2) -> int {
		called = (val1 + val2 == val);
		return val1 + val2;
	} };
	CHECK(func(2, 3) == val);
	CHECK(called);
}

TEST_CASE("<std::string()> lambda with capture") {
	bool called{ false };
	std::string val{ "5" };
	jpl::function<std::string()> func{ [&, val]() -> std::string { called = true; return val; } };
	CHECK(func() == val);
	CHECK(called);
}

TEST_CASE("<std::string(std::string, std::string)> lambda with capture") {
	bool called{ false };
	std::string val{ "Hello" };
	jpl::function<std::string(std::string, std::string)> func{ [&, val](std::string str1, std::string str2) -> std::string {
		called = ((str1 + str2) == val);
		return str1 + str2;
	} };
	CHECK(func("Hel", "lo") == val);
	CHECK(called);
}

struct TestObject {
	int method(int val2) {
		return val1 + val2;
	}

	int val1 = 2;
	bool access_me = true;
};

TEST_CASE("object member pointers") {
	TestObject object;
	jpl::function<bool(TestObject&)> accessor{ &TestObject::access_me };
	CHECK(accessor(object));
	jpl::function<int(TestObject&, int)> method{ &TestObject::method };
	CHECK(method(object, 3) == 5);
}

struct TestResults {
	int ctor   = 0;
	int dtor   = 0;
	int move_c = 0;
	int move_a = 0;
	int copy_c = 0;
	int copy_a = 0;
	void print() {
		fmt::print("ctor:   {}\tdtor:   {}\nmove_c: {}\tmove_a: {}\ncopy_c: {}\tcopy_a: {}\n", ctor, dtor, move_c, move_a, copy_c, copy_a);
	}
};

template<TestResults* res>
struct TestManage {
	TestManage() { res->ctor++; }
	~TestManage() { res->dtor++; }
	TestManage(TestManage&&) { res->move_c++; }
	TestManage& operator=(TestManage&&) { res->move_a++; return *this; }
	TestManage(const TestManage&) { res->copy_c++; }
	TestManage& operator=(const TestManage&) { res->copy_a++; return *this; }

	bool operator()() {
		return true;
	}
};

TEST_CASE("stored object management") {
	static TestResults res;
	TestManage<&res> test; // ctor 1
	auto func = new jpl::function<bool()>{ test }; // copy_c 1
	*func = std::move(test); // move_c 1, dtor 1
	delete func; // dtor 2

	CHECK(res.move_c == 1);
	CHECK(res.copy_c == 1);
	CHECK(res.dtor == 2);
}

TEST_CASE("swap") {
	jpl::function<bool()> ttf{ []{ return true; } };
	jpl::function<bool()> ftt{ []{ return false; } };
	CHECK(ttf());
	CHECK(!ftt());
	std::swap(ttf, ftt);
	CHECK(!ttf());
	CHECK(ftt());
}

TEST_CASE("move") {
	std::string test{ "test" };
	std::vector<jpl::function<bool()>> vec;
	for (int i{ 0 }; i != 5; ++i)
		vec.push_back([str = test](){ return str == "test"; });
	for (auto& func : vec)
		CHECK(func() == true);
}

TEST_CASE("move") {
	std::string test{ "test" };
	std::vector<jpl::function<std::string()>> vec;
	for (int i{ 0 }; i != 5; ++i)
		vec.push_back([str = test](){ return str; });
	for (auto& func : vec)
		CHECK(func() == "test");
}

TEST_CASE("move") {
	std::vector<jpl::function<std::string()>> vec;
	for (int i{ 0 }; i != 5; ++i)
		vec.push_back([](){ return "test"; });
	for (auto& func : vec)
		CHECK(func() == "test");
}

TEST_CASE("move") {
	std::vector<jpl::function<bool()>> vec;
	for (int i{ 0 }; i != 5; ++i)
		vec.push_back([](){ return true; });
	for (auto& func : vec)
		CHECK(func() == true);
}
