#include <string>
#include <vector>
using namespace std;

class Solution {
    bool verify(vector<int>& s_count, vector<int>& t_count) {
        for (int i = 'a'; i <= 'z'; ++i) {
            if (s_count[i] < t_count[i]) {
                return false;
            }
        }
        for (int i = 'A'; i <= 'Z'; ++i) {
            if (s_count[i] < t_count[i]) {
                return false;
            }
        }
        return true;
    }

  public:
    string minWindow(string s, string t) {
        int sl = s.size(), tl = t.size();
        int ans_start = -1, ans_len = sl + 1;
        vector<int> s_count(128), t_count(128);
        for (char c : t) {
            ++t_count[c];
        }
        for (int i = 0, j = 0; j < sl; ++j) {
            ++s_count[s[j]];
            while (i <= j && verify(s_count, t_count)) {
                if (j - i + 1 < ans_len) {
                    ans_start = i;
                    ans_len = j - i + 1;
                }
                --s_count[s[i++]];
            }
        }
        return ans_start >= 0 ? s.substr(ans_start, ans_len) : "";
    }
};