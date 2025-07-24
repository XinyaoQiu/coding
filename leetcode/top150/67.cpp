#include <string>
#include <algorithm>
using namespace std;

class Solution {
  public:
    string addBinary(string a, string b) {
        reverse(a.begin(), a.end());
        reverse(b.begin(), b.end());
        string ans;
        int check = 0;
        for (int i = 0; i < max(a.size(), b.size()); ++i) {
            if (i < a.size()) {
                check += a[i] - '0';
            }
            if (i < b.size()) {
                check += b[i] - '0';
            }
            ans += '0' + check % 2;
            check /= 2;
        }
        if (check == 1) {
            ans += '1';
        }
        reverse(ans.begin(), ans.end());
        return ans;
    }
};