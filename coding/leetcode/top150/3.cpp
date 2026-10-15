#include <string>
#include <unordered_map>
using namespace std;

class Solution {
  public:
    int lengthOfLongestSubstring(string s) {
        unordered_map<int, int> m;
		int i = 0, ans = 0;
		for (int j = 0; j < s.size(); ++j) {
			i = max(i, m[s[j]]);
			m[s[j]] = j + 1;
			ans = max(ans, j - i + 1);
		}
		return ans;
    }
};
