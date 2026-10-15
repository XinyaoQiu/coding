#include <iostream>
#include <vector>
#include <utility>
#include <queue>
using namespace std;

class Solution {
    pair<int, int> id2rc(int id, int n) {
        int r = (id - 1) / n, c = (id - 1) % n;
        if (r % 2 == 1) {
            c = n - 1 - c;
        }
        return {n - 1 - r, c};
    }
public:
    int snakesAndLadders(vector<vector<int>>& board) {
        int n = board.size();
        vector<int> visited(n * n + 1);
        queue<pair<int, int>> q;
        q.emplace(1, 0);
        while (!q.empty()) {
            auto [id, step] = q.front();
            q.pop();
            for (int i = 1; i <= 6; ++i) {
                int nxt = id + i;
                if (nxt > n * n) {
                    break;
                }
                auto [row, col] = id2rc(nxt, n);
                if (board[row][col] != -1) {
                    nxt = board[row][col];
                }
                if (nxt == n * n) {
                    return step + 1;
                }
                if (!visited[nxt]) {
                    visited[nxt] = true;
                    q.emplace(nxt, step + 1);
                }
            }
        }
        return -1;
    }
};