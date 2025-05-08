#include <string>
using namespace std;

class Solution {
  public:
    bool isPalindrome(string s) {
        int L = 0, R = s.size() - 1;
        while (L < R) {
            while (!isalnum(s[L]) && L < R) {
                L++;
            }
            while (!isalnum(s[R]) && L < R) {
                R--;
            }
            if (tolower(s[L++]) != tolower(s[R--])) {
                return false;
            }
        }
        return true;
    }
};