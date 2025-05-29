#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
using namespace std;

class Solution {
  public:
    vector<vector<string>> groupAnagrams(vector<string>& strs) {
        unordered_map<string, vector<string>> m;
        for (string& str : strs) {
            string sorted_str = str;
            sort(sorted_str.begin(), sorted_str.end());
            m[sorted_str].emplace_back(str);
        }
        vector<vector<string>> ans;
        for (auto& [k, v] : m) {
            ans.emplace_back(v);
        }
        return ans;
    }
};