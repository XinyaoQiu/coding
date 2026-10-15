#include "leetcode_base.h"

class Solution {
public:
    string longestCommonPrefix(vector<string>& strs) {
        if (strs.size() == 1) {
            return strs[0];
        }
        int n = strs.size();
        sort(strs.begin(), strs.end());
        string smallest = strs[0];
        string largest = strs[n - 1];
        string result = "";
        for (int i = 0; i < min(smallest.length(), largest.length()); i++) {
            if (smallest[i] == largest[i]) {
                result += smallest[i];
            } else {
                return result;
            }
        }
        return result;
    }
};