#include "leetcode_base.h"

class Solution {
public:
    string convert(string s, int numRows) {
        int num = numRows * 2 - 2;
        if (num == 0) {
            return s;
        }
        string res;
        for (int i = 0; i < numRows; i++) {
            for (int j = 0; j + i < s.length(); j += num) {
                res += s[j + i];
                if (i != 0 && i != numRows - 1 && j + num - i < s.length()) {
                    res += s[j + num - i];
                }
            }
        }
        return res;
    }
};