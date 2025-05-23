#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

class Solution {
  public:
    vector<int> findSubstring(string s, vector<string>& words) {
        int wl = words[0].size(), s_size = s.size(), w_size = words.size();
        vector<int> ans;
        for (int i = 0; i < wl && i + w_size * wl <= s_size; ++i) {
            unordered_map<string, int> count;
            for (int j = 0; j < w_size; ++j) {
                ++count[s.substr(i + j * wl, wl)];
            }
            for (string& word : words) {
                if (--count[word] == 0) {
                    count.erase(word);
                }
            }
            if (count.empty()) {
                ans.emplace_back(i);
            }
            for (int j = i; j + w_size * wl <= s_size; j += wl) {
                string in = s.substr(j + w_size * wl , wl);
                string out = s.substr(j, wl);
                if (++count[in] == 0) {
                    count.erase(in);
                }
                if (--count[out] == 0) {
                    count.erase(out);
                }
                if (count.empty()) {
                    ans.emplace_back(j + wl);
                }
            }
        }
        return ans;
    }
};