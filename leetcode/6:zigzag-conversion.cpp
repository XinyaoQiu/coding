#include "leetcode_base.h"

class Solution {
  public:
    pair<int, int> next_ij(int i, int j, int n) {
        if (0 <= i <= n - 2 && j % (n - 1) == 0) {
            return {i + 1, j};
        } else if (i == n - 1 && j % (n - 1) == 0) {
            return {i - 1, j + 1};
        }
    }

    string convert(string s, int numRows) {
        if (numRows == 1) {
            return s;
        }
        deque<char> chars;
        for (char c : s) {
            chars.push_back(c);
        }
        vector<vector<char>> matrix(s.length() + 1,
                                    vector<char>(s.length() + 1, ' '));
        int i = 0, j = 0;
        while (!chars.empty()) {
            matrix[i][j] = chars.front();
            chars.pop_front();
            if (0 <= i <= numRows - 2 && j % (numRows - 1) == 0) {
                i++;
            } else if (i == numRows - 1 && j % (numRows - 1) == 0) {
                i--;
                j++;
            }
        }
    }
};