// spiral-matrix
#include "leetcode_base.h"

class Solution {
  public:
    vector<int> spiralOrder(vector<vector<int>>& matrix) {
        int dir = 0;
        int m = matrix.size(), n = matrix[0].size();
        int i = 0, j = 0;
        int cnt = m * n;
        vector<int> res;
        while (1) {
            if (dir == 0) {
                if (j + 1 < n && matrix[i][j + 1] != INT32_MAX) {
                    res.push_back(matrix[i][j]);
                    matrix[i][j] = INT32_MAX;
                    j++;
                } else {
                    dir = (dir + 1) % 4;
                }
            } else if (dir == 1) {
                if (i + 1 < m && matrix[i + 1][j] != INT32_MAX) {
                    res.push_back(matrix[i][j]);
                    matrix[i][j] = INT32_MAX;
                    i++;
                } else {
                    dir = (dir + 1) % 4;
                }
            } else if (dir == 2) {
                if (j - 1 >= 0 && matrix[i][j - 1] != INT32_MAX) {
                    res.push_back(matrix[i][j]);
                    matrix[i][j] = INT32_MAX;
                    j--;
                } else {
                    dir = (dir + 1) % 4;
                }
            } else {
                if (i - 1 >= 0 && matrix[i - 1][j] != INT32_MAX) {
                    res.push_back(matrix[i][j]);
                    matrix[i][j] = INT32_MAX;
                    i--;
                } else {
                    dir = (dir + 1) % 4;
                }
            }
            if (res.size() == cnt - 1) {
                res.push_back(matrix[i][j]);
                break;
            }
        }
        return res;
    }
};