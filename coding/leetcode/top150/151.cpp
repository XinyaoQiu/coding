#include <string>
using namespace std;

class Solution {
    void reverse(string& s, int L, int R) {
        while (L < R) {
            swap(s[L++], s[R--]);
        }
    }
  public:
    string reverseWords(string s) {
        reverse(s, 0, s.size() - 1);
        int i = 0;
        while (i < s.size()) {
            if (isalnum(s[i])) {
                int r = i;
                while (r < s.size() && isalnum(s[r])) {
                    r++;
                }
                reverse(s, i, r - 1);
                i = r;
            }
            if (isspace(s[i])) {
                int r = i;
                while (r < s.size() && isspace(s[r])) {
                    r++;
                }
                if (i == 0 || r == s.size()) {
                    s.replace(i, r - i, "");
                } else {
                    s.replace(i, r - i - 1, "");
                    i++;
                }
            }
        }
        return s;
    }
    
};