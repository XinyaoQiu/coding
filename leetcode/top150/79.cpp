#include <string>
#include <vector>
using namespace std;

class Solution {
    string path;
    vector<vector<int>> visited;
    const int DIR[4][4] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    bool dfs(vector<vector<char>>& board, string& word, int i, int j) {
        if (path == word) {
            return true;
        }
        if (path != "" && path.back() != word[path.size() - 1]) {
            return false;
        }
        int m = board.size(), n = board[0].size();
        for (int k = 0; k < 4; ++k) {
            i += DIR[k][0];
            j += DIR[k][1];
            if (i > 0 && i < m - 1 && j > 0 && j < n - 1 && !visited[i][j]) {
                path.push_back(board[i][j]);
                visited[i][j] = 1;
                if (dfs(board, word, i, j)) {
                    return true;
                }
                visited[i][j] = 0;
                path.pop_back();
            }
            i -= DIR[k][0];
            j -= DIR[k][1];
        }
        return false;
    }
public:
    bool exist(vector<vector<char>>& board, string word) {
        int m = board.size(), n = board[0].size();
        visited = vector<vector<int>>(m, vector<int>(n));
        for (int i = 0; i < m; ++i) {
            for (int j = 0; j < n; ++j) {
                path.push_back(board[i][j]);
                visited[i][j] = 1;
                if (dfs(board, word, i, j)) {
                    return true;
                }
                visited[i][j] = 0;
                path.pop_back();
            }
        }
        return false;;
    }
};