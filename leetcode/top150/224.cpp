#include <algorithm>
#include <string>
#include <vector>
using namespace std;

class Solution {
    void preprocess(string& s) {
        s.erase(remove_if(s.begin(), s.end(), ::isspace), s.end());
        for (int i = 0; i < s.size(); i++) {
            if ((s[i] == '+' || s[i] == '-') && (i == 0 || s[i - 1] == '(')) {
                s.insert(i, 1, '0');
            }
        }
    }
    void calc(vector<int>& nums, vector<char>& ops) {
        char op = ops.back();
        ops.pop_back();
        int b = nums.back();
        nums.pop_back();
        int a = nums.back();
        nums.pop_back();
        if (op == '+')
            nums.emplace_back(a + b);
        if (op == '-')
            nums.emplace_back(a - b);
    }

  public:
    int calculate(string s) {
        preprocess(s);
        vector<int> nums;
        vector<char> ops;
        for (int i = 0; i < s.size(); i++) {
            if (isdigit(s[i])) {
                int num = 0;
                while (isdigit(s[i])) {
                    num = num * 10 + (s[i++] - '0');
                }
                i--;
                nums.emplace_back(num);
            } else if (s[i] == '+' || s[i] == '-') {
                while (!ops.empty() && ops.back() != '(') {
                    calc(nums, ops);
                }
                ops.emplace_back(s[i]);
            } else if (s[i] == ')') {
                while (ops.back() != '(') {
                    calc(nums, ops);
                }
                ops.pop_back();
            } else {
                ops.emplace_back(s[i]);
            }
        }
        while (!ops.empty()) {
            calc(nums, ops);
        }
        return nums[0];
    }
};