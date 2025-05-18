#include <vector>
#include <string>
using namespace std;

class Solution {
  public:
    int lengthOfLastWord(string s) {
        int n = s.size();
        int ans = 0;
        for (int i = n - 1; i >= 0; i--) {
            if (isalpha(s[i])) {
                while (i >= 0 && isalpha(s[i])) {
                    ans++;
                    i--;
                }
                break;
            }
        }
        return ans;
    }
};
