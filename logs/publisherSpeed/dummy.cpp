#include <string>
#include <functional>
struct Msg {
    long   i;
    int    j;
    short  k;
    double d;
};

int dummy(const Msg& m) {
    return m.j;
}

void dummy() {
}

void DoTimedTest(const std::string& name, size_t n)
{
}
