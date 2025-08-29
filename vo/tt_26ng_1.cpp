#include <iostream>
#include <vector>
using namespace std;

/*
input N
input = 1
output = [[1]]

input = 2
output = [[1, 2],
          [4, 3]]

input = 3
output = [[ 1, 2, 3],
          [ 8, 9, 4],
          [ 7, 6, 5]]

input = 4,
output = [[ 1,   2,  3, 4],
          [ 12, 13, 14, 5],
          [ 11, 16, 15, 6],
          [ 10,  9,  8, 7]]"

input = 5,
output = [[ 1  2  3  4  5 ]
           [16 17 18 19 6 ]
           [15 24 25 20 7 ]
           [14 23 22 21 8 ]
           [13 12 11 10 9 ]]
*/

vector<vector<int>> printMatrix(int n) {
    vector<vector<int>> ans(n, vector<int>(n));
    int top = 0, bottom = n - 1, left = 0, right = n - 1;
    int count = 1;
    while (top <= bottom && left <= right) {
        for (int i = left; i <= right; ++i) {
            ans[top][i] = count++;
        }
        for (int i = top + 1; i <= bottom; ++i) {
            ans[i][right] = count++;
        }
        if (top != bottom) {
            for (int i = right - 1; i >= left; --i) {
                ans[bottom][i] = count++;
            }
        }
        if (left != right) {
            for (int i = bottom - 1; i >= top + 1; --i) {
                ans[i][left] = count++;
            }
        }
        top++; bottom--; left++; right--;
    }
    return ans;
}

void printVec(vector<vector<int>>& v) {
    for (int i = 0; i < v.size(); ++i) {
        for (int j = 0; j < v[0].size(); ++j) {
            cout << v[i][j] << " ";
        }
        cout << endl;
    }
}

int main() {
    vector<vector<int>> case1 = printMatrix(1);
    vector<vector<int>> case2 = printMatrix(2);
    vector<vector<int>> case3 = printMatrix(3);
    vector<vector<int>> case4 = printMatrix(4);
    vector<vector<int>> case5 = printMatrix(5);
    vector<vector<int>> case6 = printMatrix(6);
    printVec(case1);
    printVec(case2);
    printVec(case3);
    printVec(case4);
    printVec(case5);
    printVec(case6);
}