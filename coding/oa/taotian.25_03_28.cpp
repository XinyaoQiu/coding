#include <iostream>

class A {
public:
    A() {
        std::cout << "A";
    }

    virtual ~A() {
        std::cout << "~A";
    }
};

class B : public A {
public:
    B() {
        std::cout << "B";
    }

    ~B() {
        std::cout << "~B";
    }
};

int main() {
    B* b = new B[2];
    delete[] b;
    return 0;
}