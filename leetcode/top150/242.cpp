#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    bool isAnagram(string s, string t) {
        vector<int> count(26);
        for (char c : s) {
            ++count[c - 'a'];
        }
        for (char c : s) {
            --count[c - 'a'];
            if (count[c - 'a'] < 0) {
                return false;
            }
        }
        return true;
    }
};