#include <vector>
#include <queue>
using namespace std;

class Solution {
    vector<vector<int>> dirs = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    void dfs(vector<vector<int>>& grid, int x, int y, queue<pair<int, int>>& q) {
        int n = grid.size();
        if (x < 0 || x >= n || y < 0 || y >= n) {
            return;
        }
        if (grid[x][y] == 1) {
            grid[x][y] = 2;
            q.push({x, y});
            for (auto& d : dirs) {
                int nx = x + d[0], ny = y + d[1];
                dfs(grid, nx, ny, q);
            }
        }
    }
public:
    int shortestBridge(vector<vector<int>>& grid) {
        queue<pair<int, int>> q;
        int n = grid.size();
        bool found = false;
        for (int x = 0; x < n && !found; ++x) {
            for (int y = 0; y < n && !found; ++y) {
                if (grid[x][y] == 1) {
                    found = true;
                    dfs(grid, x, y, q);
                }
            }
        }
        int step = 0;
        while (!q.empty()) {
            int sz = q.size();
            while (sz--) {
                int x = q.front().first, y = q.front().second;
                q.pop();
                for (auto& d : dirs) {
                    int nx = x + d[0], ny = y + d[1];
                    if (nx >= 0 && nx < n && ny >= 0 && ny < n) {
                        if (grid[nx][ny] == 1) {
                            return step;
                        }
                        if (grid[nx][ny] == 0) {
                            grid[nx][ny] = -1;
                            q.push({nx, ny});
                        }
                    }
                }
            }
            ++step;
        }
        return -1;
    }
};