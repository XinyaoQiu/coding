#include <string>
#include <unordered_map>
using namespace std;

class Solution {
  public:
    int lengthOfLongestSubstring(string s) {
        unordered_map<int, int> m;
		int res = 0;
		for (int i = 0, j = 0; j < s.size(); j++) {
			i = max(i, m[s[j]]);
			res = max(res, j - i + 1);
			m[s[j]] = j + 1;
		}
		return res;
    }
};