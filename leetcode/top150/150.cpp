#include <vector>
#include <string>
using namespace std;

class Solution {
public:
    int evalRPN(vector<string>& tokens) {
        vector<int> stack;
        for (string& t : tokens) {
            if (t == "+" || t == "-" || t == "*" || t == "/") {
                int b = stack.back(); stack.pop_back();
                int a = stack.back(); stack.pop_back();
                if (t == "+") {
                    stack.push_back(a + b);
                } else if (t == "-") {
                    stack.push_back(a - b);
                } else if (t == "*") {
                    stack.push_back(a * b);
                } else {
                    stack.push_back(a / b);
                }
            } else {
                stack.push_back(stoi(t));
            }
        }
        return stack.back();
    }
};