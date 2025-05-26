#include <vector>
using namespace std;

class Solution {
  public:
    vector<int> spiralOrder(vector<vector<int>>& matrix) {
        int top = 0, bottom = matrix.size() - 1, left = 0, right = matrix[0].size() - 1;
        vector<int> ans;
        while (top <= bottom && left <= right) {
            for (int i = left; i <= right; ++i) {
                ans.emplace_back(matrix[top][i]);
            }
            for (int i = top + 1; i <= bottom; ++i) {
                ans.emplace_back(matrix[i][right]);
            }
            if (top < bottom && left < right) {
                for (int i = right - 1; i >= left; --i) {
                    ans.emplace_back(matrix[bottom][i]);
                }
                for (int i = bottom - 1; i > top; --i) {
                    ans.emplace_back(matrix[i][left]);
                }
            }
            ++top; --bottom; ++left; --right;
        }
        return ans;
    }
};