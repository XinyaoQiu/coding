#include <string>
#include <vector>
using namespace std;

class Solution {
    vector<int> nums;
    vector<char> ops;

    void preprocess(string& s) {
        int pos = s.find(' ');
        while (pos != -1) {
            s.replace(pos, 1, "");
            pos = s.find(' ');
        }
        for (int i = 0; i < s.size(); i++) {
            if ((s[i] == '+' || s[i] == '-') && (i == 0 || s[i - 1] == '(')) {
                s.insert(i, 1, '0');
            }
        }
    }
    void calc_once() {
        int b = nums.back();
        nums.pop_back();
        int a = nums.back();
        nums.pop_back();
        char op = ops.back();
        ops.pop_back();
        if (op == '+')
            nums.push_back(a + b);
        if (op == '-')
            nums.push_back(a - b);
    }

  public:
    int calculate(string s) {
        preprocess(s);
        for (int i = 0; i < s.size(); i++) {
            if (isdigit(s[i])) {
                int num = 0;
                while (isdigit(s[i])) {
                    num = num * 10 + (s[i++] - '0');
                }
                i--;
                nums.push_back(num);
            } else if (s[i] == '(') {
                ops.push_back(s[i]);
            } else if (s[i] == ')') {
                while (ops.back() != '(') {
                    calc_once();
                }
                ops.pop_back();
            } else if (s[i] == '+' || s[i] == '-') {
                while (!(ops.empty() || ops.back() == '(')) {
                    calc_once();
                }
                ops.push_back(s[i]);
            }
        }
        while (!ops.empty()) {
            calc_once();
        }
        return nums[0];
    }
};