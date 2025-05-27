#include <vector>
using namespace std;

class Solution {
  public:
    void gameOfLife(vector<vector<int>>& board) {
        int m = board.size(), n = board[0].size();
        for (int i = 0; i < m; ++i) {
            for (int j = 0; j < n; ++j) {
                int count = 0;
                for (int k = -1; k <= 1; ++k) {
                    for (int l = -1; l <= 1; ++l) {
                        if (!(k == 0 && l == 0)) {
                            int r = i + k, c = j + l;
                            if (r >= 0 && r < m && c >= 0 && c < n && board[r][c] >= 1) {
                                ++count;
                            }
                        }
                    }
                }
                board[i][j] = board[i][j] == 1 ? count + 1 : -count;
            }
        }
        for (int i = 0; i < m; ++i) {
            for (int j = 0; j < n; ++j) {
                if (board[i][j] >= 0) {
                    int count = board[i][j] - 1;
                    if (count < 2 || count > 3) {
                        board[i][j] = 0;
                    } else {
                        board[i][j] = 1;
                    }
                } else {
                    int count = -board[i][j];
                    if (count == 3) {
                        board[i][j] = 1;
                    } else {
                        board[i][j] = 0;
                    }
                }
            }
        }
    }
};