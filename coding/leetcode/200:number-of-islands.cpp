#include "leetcode_base.h"

class Solution {
  public:
    void infect(vector<vector<char>>& grid, int i, int j, int m, int n) {
        if (i < 0 || i >= m || j < 0 || j >= n ||
            grid[i][j] == '0' || grid[i][j] == '2') {
            return;
        }
        grid[i][j] = '2';
        infect(grid, i - 1, j, m, n);
        infect(grid, i + 1, j, m, n);
        infect(grid, i, j - 1, m, n);
        infect(grid, i, j + 1, m, n);
    }

    int numIslands(vector<vector<char>>& grid) {
        int m = grid.size();
        int n = grid[0].size();
        int result = 0;
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                if (grid[i][j] == '1') {
                    infect(grid, i, j, m, n);
                    result++;
                }
            }
        }
        return result;
    }
};