#include <algorithm>
#include <vector>
using namespace std;

class Solution {
  public:
    vector<vector<int>> merge(vector<vector<int>>& intervals) {
        sort(intervals.begin(), intervals.end(), [](const vector<int>& a, const vector<int>& b) {
            return a[0] < b[0];
        });
        vector<vector<int>> ans;
        for (auto& v : intervals) {
            if (ans.empty() || v[0] > ans.back()[1]) {
                ans.emplace_back(v);
            } else {
                ans.back()[1] = max(ans.back()[1], v[1]);
            }
        }
        return ans;
    }
};