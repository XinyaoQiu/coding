#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    vector<int> findAnagrams(string s, string p) {
        int sl = s.size(), pl = p.size();
        if (sl < pl)
            return {};
        vector<int> res;
        vector<int> s_count(26);
        vector<int> p_count(26);
        for (int i = 0; i < pl; ++i) {
            ++s_count[s[i] - 'a'];
            ++p_count[p[i] - 'a'];
        }
        if (s_count == p_count) {
            res.push_back(0);
        }
        for (int i = 0; i + pl < sl; ++i) {
            int in = s[i + pl] - 'a', out = s[i] - 'a';
            ++s_count[in];
            --s_count[out];
            if (s_count[in] == p_count[in] && s_count[out] == p_count[out] &&
                s_count == p_count) {
                res.push_back(i + 1);
            }
        }
        return res;
    }
};