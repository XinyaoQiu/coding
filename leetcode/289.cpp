// game-of-life

#include "leetcode_base.h"

class Solution {
public:
    void gameOfLife(vector<vector<int>>& board) {
        int m = board.size(), n = board[0].size();
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                int count = 0;
                for (int k = max(0, i - 1); k <= min(m - 1, i + 1); k++) {
                    for (int l = max(0, j - 1); l <= min(n - 1, j + 1); l++) {
                        if (k != i || l != j) {
                            count += board[k][l] % 2;
                        }
                    }
                }
                if (board[i][j] == 1) {
                    if (count < 2 || count > 3) {
                        board[i][j] = 3;
                    }
                } else {
                    if (count == 3) {
                        board[i][j] = 2;
                    }
                }
            }
        }
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                if (board[i][j] == 2) {
                    board[i][j] = 1;
                } else if (board[i][j] == 3) {
                    board[i][j] = 0;
                }
            }
        }      
    }
};