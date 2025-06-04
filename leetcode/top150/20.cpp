#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    bool isValid(string s) {
        vector<char> stack;
        for (char c : s) {
            if (stack.empty()) {
                stack.push_back(c);
            } else {
                bool type1 = stack.back() == '(' && c == ')';
                bool type2 = stack.back() == '[' && c == ']';
                bool type3 = stack.back() == '{' && c == '}';
                if (type1 || type2 || type3) {
                    stack.pop_back();
                } else {
                    stack.push_back(c);
                }
            }
        }
        return stack.empty();
    }
};