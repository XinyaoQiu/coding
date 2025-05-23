#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

class Solution {
  public:
    vector<int> findSubstring(string s, vector<string>& words) {
        int wl = words[0].size(), s_size = s.size(), w_size = words.size();
        unordered_map<string, int> count;
        vector<int> ans;
        for (int i = 0; i < wl; ++i) {
            if (i + w_size * wl >= s_size) {
                break;
            }
            for (int j = 0; j < w_size; ++j) {
                ++count[s.substr(i + j * wl, wl)];
                --count[words[j]];
            }
            int diff = 0;
            for (auto& [k, v] : count) {
                if (v != 0) {
                    ++diff;
                }
            }
            if (diff == 0) {
                ans.emplace_back(0);
            }
            for (int j = i; j + w_size * wl < s_size; j += wl) {
                string in_word = s.substr(j + w_size * wl, wl);
                string out_word = s.substr(j, wl);
                if (count[out_word] == 0) {
                    ++diff;
                } else if (count[out_word] == 1) {
                    --diff;
                }
                --count[out_word];
                if (count[in_word] == 0) {
                    ++diff;
                } else if (count[in_word] == -1) {
                    --diff;
                }
                ++count[in_word];
                if (diff == 0) {
                    ans.emplace_back(j + wl);
                }
            }
        }
        return ans;
    }
};