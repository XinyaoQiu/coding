#include <vector>
using namespace std;

class Solution {
  public:
    vector<vector<int>> insert(vector<vector<int>>& intervals,
                               vector<int>& newInterval) {
        vector<vector<int>> ans;
        for (auto& v : intervals) {
            if (newInterval[0] < v[0]) {
                if (ans.empty() || newInterval[0] > ans.back()[1]) {
                    ans.emplace_back(newInterval);
                } else {
                    ans.back()[1] = max(ans.back()[1], newInterval[1]);
                }
            }
            if (ans.empty() || v[0] > ans.back()[1]) {
                ans.emplace_back(v);
            } else {
                ans.back()[1] = max(ans.back()[1], v[1]);
            }
        }
        if (ans.empty() || newInterval[0] >= ans.back()[0]) {
            if (ans.empty() || newInterval[0] > ans.back()[1]) {
                ans.emplace_back(newInterval);
            } else {
                ans.back()[1] = max(ans.back()[1], newInterval[1]);
            }
        }
        return ans;
    }
};