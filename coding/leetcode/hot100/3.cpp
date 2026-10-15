#include <vector>
#include <string>
using namespace std;

class Solution {
public:
    int lengthOfLongestSubstring(string s) {
        vector<int> m(128);
        int i = 0, ans = 0;
        for (int j = 0; j < s.size(); ++j) {
            i = max(i, m[s[j]]);
            m[s[j]] = j + 1;
            ans = max(ans, j - i + 1);
        }
        return ans;
    }
};