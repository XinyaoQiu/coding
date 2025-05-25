#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    string minWindow(string s, string t) {
        int sl = s.size(), tl = t.size();
        int ans_start = -1, ans_len = sl + 1, diff = 0;
        vector<int> count(128); // t-count > s-count
        for (char c : t) {
            if (++count[c] == 1) {
                ++diff;
            }
        }
        for (int i = 0, j = 0; j < sl; ++j) {
            if (--count[s[j]] == 0) {
                --diff;
            }
            while (i <= j && diff == 0) {
                if (j - i + 1 < ans_len) {
                    ans_start = i;
                    ans_len = j - i + 1;
                }
                if (++count[s[i++]] == 1) {
                    ++diff;
                }
            }
        }
        return ans_start >= 0 ? s.substr(ans_start, ans_len) : "";
    }
};