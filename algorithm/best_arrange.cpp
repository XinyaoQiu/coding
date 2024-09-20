#include <algorithm>
#include <iostream>
#include <vector>

using namespace std;

struct Program {
    int start;
    int end;

    Program(int start_, int end_) : start(start_), end(end_) {}
};

int main() {
    vector<Program*> arr;
    arr.push_back(new Program(6, 10));
    arr.push_back(new Program(7, 8));
    arr.push_back(new Program(11, 12));
    sort(arr.begin(), arr.end(),
         [](Program* p1, Program* p2) { return p1->end < p2->end; });
    int timePoint = 6;
    int result = 0;
    for (Program* p : arr) {
        if (p->start >= timePoint) {
            result++;
            timePoint = p->end;
        }
    }
    return result;
}   