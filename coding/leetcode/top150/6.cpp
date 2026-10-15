#include <string>
#include <vector>
using namespace std;

/*
    "PAYPALISHIRING", numRows = 3
    [0, 4, 8, 12], i = 0; j = [0, 1, 2, 3]
    [1, 3, 5, 7, 9, 11, 13], i = 1; j = [0, 1]
    [2, 6, 10]

    "PAYPALISHIRING", numRows = 4
    [0, 6, 12]
    [1, 5, 7, 11]
    [2, 4, 8, 10]
*/

class Solution {
  public:
    string convert(string s, int numRows) {
        int len = s.size();
        int n = 2 * numRows - 2;
        if (n == 0) return s;
        string ans;
        for (int i = 0; i < numRows; i++) {
            for (int j = 0; j + i < len; j += n) {
                ans += s[j + i];
                if (i > 0 && i < numRows - 1 && j + n - i < len) {
                    ans += s[j + n - i];
                }
            }
        }
        return ans;
    }
};