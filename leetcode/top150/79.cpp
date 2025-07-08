#include <string>
#include <vector>
using namespace std;

class Solution {
    vector<vector<int>> visited;
    const int DIR[4][4] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    bool dfs(vector<vector<char>>& board, string& word, int i, int j, int index) {
        if (index == word.size()) {
            return true;
        }
        int m = board.size(), n = board[0].size();
        if (i < 0 || i >= m || j < 0 || j >= n || visited[i][j] || board[i][j] != word[index]) {
            return false;
        }
        visited[i][j] = 1;
        for (int k = 0; k < 4; ++k) {
            if (dfs(board, word, i + DIR[k][0], j + DIR[k][1], index + 1)) {
                return true;
            }
        }
        visited[i][j] = 0;
        return false;
    }
public:
    bool exist(vector<vector<char>>& board, string word) {
        int m = board.size(), n = board[0].size();
        visited = vector<vector<int>>(m, vector<int>(n));
        for (int i = 0; i < m; ++i) {
            for (int j = 0; j < n; ++j) {
                if (dfs(board, word, i, j, 0)) {
                    return true;
                }
            }
        }
        return false;
    }
};