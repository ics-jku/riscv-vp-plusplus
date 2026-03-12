#include <iostream>
#include <list>
#include <map>
#include <vector>

struct A {
	int a;
	int b;

	A(int a, int b) : a(a), b(b) {}

	virtual int foo() = 0;

	virtual int bar() {
		return a + b;
	}
};

struct B : public A {
	using A::A;

	virtual int foo() override {
		return a - b;
	}
};

int main(void) {
	std::cout << "Hello World\n";
	B x(5, 2);
	auto a = x.foo();
	auto b = x.bar();
	std::cout << a << "\n";
	std::cout << b << "\n";
	std::vector<int> v;
	v.push_back(1);
	v.push_back(2);
	for (auto e : v) std::cout << e << std::endl;
	return 0;
}
