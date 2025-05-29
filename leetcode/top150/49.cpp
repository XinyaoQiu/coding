#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <numeric>
using namespace std;

// c++20

class Solution {
    struct ArrayHash {
        size_t operator()(const array<int, 26>& arr) const {
            return accumulate(arr.begin(), arr.end(), 0u, [&](size_t acc, int num) {
                return (acc << 1) ^ hash<int>{}(num);
            });
        }
    };
  public:
    vector<vector<string>> groupAnagrams(vector<string>& strs) {
        vector<vector<string>> ans;
        unordered_map<array<int, 26>, vector<string>, ArrayHash> m;
        for (string& s : strs) {
            array<int, 26> arr{};
            for (char c : s) {
                ++arr[c - 'a'];
            }
            m[arr].emplace_back(s);
        }
        for (auto& [k, v] : m) {
            ans.emplace_back(v);
        }
        return ans;
    }
};