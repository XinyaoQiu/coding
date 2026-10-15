#include <vector>
#include <algorithm>
using namespace std;

// [[1, 6], [2, 8], [7, 12], [10, 16]]
// y = 6, 12

// [[1, 2], [3, 4], [5, 6], [7, 8]]
// y = 2, 4, 6, 8

class Solution {
  public:
    int findMinArrowShots(vector<vector<int>>& points) {
        sort(points.begin(), points.end(), [](const vector<int>& a, const vector<int>& b) {
            return a[0] < b[0];
        });
        int n = points.size();
        int start = points[0][0], end = points[0][1];
        int ans = 1;
        for (auto& v : points) {
            if (v[0] <= end) {
                start = v[0];
                end = min(end, v[1]);
            } else {
                start = v[0];
                end = v[1];
                ++ans;
            }
        }
        return ans;
    }
};