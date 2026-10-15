#include <vector>
using namespace std;

class Solution {
    using matrix1st = vector<int>;
    using matrix2nd = vector<vector<int>>;
    using matrix3rd = vector<vector<vector<int>>>;
  public:
    bool isValidSudoku(vector<vector<char>>& board) {
        matrix2nd rows(9, matrix1st(9));
        matrix2nd columns(9, matrix1st(9));
        matrix3rd subs(3, matrix2nd(3, matrix1st(9)));
        for (int i = 0; i < 9; ++i) {
            for (int j = 0; j < 9; ++j) {
                if (board[i][j] != '.') {
                    int index = board[i][j] - '1';
                    if (rows[i][index] == 1 || columns[j][index] == 1 || subs[i / 3][j / 3][index] == 1) {
                        return false;
                    }    
                    rows[i][index] = 1;
                    columns[j][index] = 1;
                    subs[i / 3][j / 3][index] = 1;
                }
            }
        }      
        return true;
    }
};