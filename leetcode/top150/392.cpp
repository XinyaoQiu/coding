#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    bool isSubsequence(string s, string t) {
        vector<int> count_t(26);
        for (char c : t) {
            count_t[c - 'a']++;
        }
        for (char c : s) {
            count_t[c - 'a']--;
            if (count_t[c - 'a'] < 0) {
                return false;
            }
        }
        return true;
    }
};