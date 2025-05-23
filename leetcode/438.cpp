#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    vector<int> findAnagrams(string s, string p) {
        int sl = s.size(), pl = p.size();
        vector<int> ans;
        vector<int> count(26);
        if (sl < pl) {
            return {};
        }
        for (int i = 0; i < pl; ++i) {
            ++count[s[i] - 'a'];
            --count[p[i] - 'a'];
        }
        int diff = 0;
        for (int x : count) {
            if (x != 0) {
                ++diff;
            }
        }
        if (diff == 0) {
            ans.emplace_back(0);
        }
        for (int i = 0; i + pl < sl; ++i) {
            int in = s[i + pl] - 'a';
            int out = s[i] - 'a';
            if (count[in] == 0) {
                ++diff;
            } else if (count[in] == -1) {
                --diff;
            }
            ++count[in];
            if (count[out] == 0) {
                ++diff;
            } else if (count[out] == 1) {
                --diff;
            }
            --count[out];
            if (diff == 0) {
                ans.emplace_back(i + 1);
            }
        }
        return ans;
    }
};