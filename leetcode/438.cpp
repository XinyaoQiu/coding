#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    vector<int> findAnagrams(string s, string p) {
        int sl = s.size(), pl = p.size();
        if (sl < pl) {
            return {};
        }
        vector<int> ans;
        vector<int> count(26);
        for (int i = 0; i < pl; ++i) {
            ++count[s[i] - 'a'];
            --count[p[i] - 'a'];
        }
        int diff = 0;
        for (int i = 0; i < 26; ++i) {
            if (count[i] != 0) {
                ++diff;
            }
        }
        if (diff == 0) {
            ans.push_back(0);
        }
        for (int i = 0; i + pl < sl; ++i) {
            if (count[s[i] - 'a'] == 0) {
                ++diff;
            } else if (count[s[i] - 'a'] == 1) {
                --diff;
            }
            --count[s[i] - 'a'];
            if (count[s[i + pl] - 'a'] == 0) {
                ++diff;
            } else if (count[s[i + pl] - 'a'] == -1) {
                --diff;
            }
            ++count[s[i + pl] - 'a'];
            if (diff == 0) {
                ans.push_back(i + 1);
            }
        }
        return ans;
    }
};