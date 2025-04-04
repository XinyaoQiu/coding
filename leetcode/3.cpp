// longest-substring-without-repeating-characters
#include "leetcode_base.h"

class Solution {
public:
    int lengthOfLongestSubstring(string s) {
        if (s.length() == 0) {
            return 0;
        }
        unordered_map<char, int> map;
        int result = 0;
        int left = 0;
        for (int right = 0; right < s.length(); right++) {
            if (!map.count(s[right]) || map[s[right]] < left) {
                map[s[right]] = right;
                result = max(result, right - left + 1);
            } else {
                left = map[s[right]] + 1;
                map[s[right]] = right;
            }
        }
        return result;
    }
};