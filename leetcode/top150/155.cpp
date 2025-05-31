#include <string>
#include <vector>
using namespace std;

class MinStack {
    vector<pair<int, int>> stack;
public:
    MinStack() {}
    
    void push(int val) {
        if (stack.empty()) {
            stack.push_back({val, val});
        } else {
            int currMin = stack.back().second;
            stack.push_back({val, min(val, currMin)});
        }
    }
    
    void pop() {
        stack.pop_back();
    }
    
    int top() {
        return stack.back().first;
    }
    
    int getMin() {
        return stack.back().second;
    }
};

/**
 * Your MinStack object will be instantiated and called as such:
 * MinStack* obj = new MinStack();
 * obj->push(val);
 * obj->pop();
 * int param_3 = obj->top();
 * int param_4 = obj->getMin();
 */