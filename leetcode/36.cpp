// valid-sudoku

#include "leetcode_base.h"

class Solution {
public:
    bool isValidSudoku(vector<vector<char>>& board) {
        for (auto& v : board) {
            unordered_set<char> s;
            for (auto& c : v) {
                if (c != '.') {
                    if (s.find(c) != s.end()) {
                        return false;
                    }
                    s.insert(c);
                }
            }
        }
        for (int i = 0; i < 9; i++) {   
            unordered_set<char> s;
            for (int j = 0; j < 9; j++) {
                if (board[j][i] != '.') {
                    if (s.find(board[j][i]) != s.end()) {
                        return false;
                    }
                    s.insert(board[j][i]);
                }
            }
        }
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                unordered_set<char> s;
                for (int k = 0; k < 3; k++) {
                    for (int l = 0; l < 3; l++) {
                        if (board[i * 3 + k][j * 3 + l] != '.') {
                            if (s.find(board[i * 3 + k][j * 3 + l]) != s.end()) {
                                return false;
                            }
                            s.insert(board[i * 3 + k][j * 3 + l]);
                        }
                    }
                }
            }
        }
        return true;
    }   
};
