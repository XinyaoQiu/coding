#include <vector>
#include <string>
#include <unordered_set>
#include <utility>
using namespace std;

class Solution {
    struct TrieNode {
        bool isEnd;
        vector<TrieNode*> children;
        TrieNode() : isEnd(false), children(26) {}
    };
    TrieNode* root;
    unordered_set<string> st;
    vector<vector<int>> visited;
    string word;
    void dfs(vector<vector<char>>& board, TrieNode* root, int i, int j) {
        word.push_back(board[i][j]);
        visited[i][j] = 1;
        if (root->isEnd) {
            st.insert(word);
        }
        if (i > 0 && root->children[board[i - 1][j] - 'a'] && !visited[i - 1][j]) {
            dfs(board, root->children[board[i - 1][j] - 'a'], i - 1, j);
        }
        if (i < board.size() - 1 && root->children[board[i + 1][j] - 'a'] && !visited[i + 1][j]) {
            dfs(board, root->children[board[i + 1][j] - 'a'], i + 1, j);
        }
        if (j > 0 && root->children[board[i][j - 1] - 'a'] && !visited[i][j - 1]) {
            dfs(board, root->children[board[i][j - 1] - 'a'], i, j - 1);
        }
        if (j < board[0].size() - 1 && root->children[board[i][j + 1] - 'a'] && !visited[i][j + 1]) {
            dfs(board, root->children[board[i][j + 1] - 'a'], i, j + 1);
        }
        visited[i][j] = 0;
        word.pop_back();
    }
public:
    vector<string> findWords(vector<vector<char>>& board, vector<string>& words) {
        root = new TrieNode();
        int m = board.size(), n = board[0].size();
        visited = vector<vector<int>>(m, vector<int>(n));
        for (string& s : words) {
            TrieNode* cur = root;
            for (char it : s) {
                if (!cur->children[it - 'a']) {
                    cur->children[it - 'a'] = new TrieNode();
                }
                cur = cur->children[it - 'a'];
            }
            cur->isEnd = true;
        }
        for (int i = 0; i < board.size(); ++i) {
            for (int j = 0; j < board[0].size(); ++j) {
                if (root->children[board[i][j] - 'a']) {
                    dfs(board, root->children[board[i][j] - 'a'], i, j);
                }
            }
        }
        vector<string> ans(st.begin(), st.end());
        return ans;
    }
};