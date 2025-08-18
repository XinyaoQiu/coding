#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
using namespace std;

void swapRows(vector<vector<int>>& matrix, int r1, int r2) {
    swap(matrix[r1], matrix[r2]);
}

void swapColumns(vector<vector<int>>& matrix, int c1, int c2) {
    for (auto& v : matrix) {
        swap(v[c1], v[c2]);
    }
}

void reverseRow(vector<vector<int>>& matrix, int r) {
    reverse(matrix[r].begin(), matrix[r].end());
}

void reverseColumn(vector<vector<int>>& matrix, int c) {
    vector<int> col;
    for (auto& v : matrix) {
        col.push_back(v[c]);
    }
    reverse(col.begin(), col.end());
    for (int i = 0; i < matrix.size(); ++i) {
        matrix[i][c] = col[i];
    }
}

void rotate90Clockwise(vector<vector<int>>& matrix) {
    int m = matrix.size(), n = matrix[0].size();
    vector<vector<int>> temp(n, vector<int>(m));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            temp[i][j] = matrix[m - 1 - j][i];
        }
    }
    matrix = temp;
}

void solution(vector<vector<int>>& matrix, vector<string>& commands) {
    for (string& command : commands) {
        istringstream iss(command);
        string op;
        iss >> op;
        if (op == "swapRows") {
            int r1, r2;
            iss >> r1 >> r2;
            swapRows(matrix, r1, r2);
        } else if (op == "swapColumns") {
            int c1, c2;
            iss >> c1 >> c2;
            swapColumns(matrix, c1, c2);
        } else if (op == "reverseRow") {
            int r;
            iss >> r;
            reverseRow(matrix, r);
        } else if (op == "reverseColumn") {
            int c;
            iss >> c;
            reverseColumn(matrix, c);
        } else if (op == "rotate90Clockwise") {
            rotate90Clockwise(matrix);
        }
    }
}

int main() {
    vector<vector<int>> matrix({{1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 11, 12}});
    vector<string> commands({"rotate90Clockwise"});
    solution(matrix, commands);
    for (int i = 0; i < matrix.size(); ++i) {
        for (int j = 0; j < matrix[0].size(); ++j) {
            cout << matrix[i][j] << " ";
        }
        cout << endl;
    }
}