#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    bool canConstruct(string ransomNote, string magazine) {
        vector<int> count(26);
        for (char c : magazine) {
            ++count[c - 'a'];
        }
        for (char c : ransomNote) {
            --count[c - 'a'];
            if (count[c - 'a'] < 0) {
                return false;
            }
        }
        return true;
    }
};