#include <string>
using namespace std;

class Solution {
public:
    int lengthOfLongestSubstring(string s) {
      int maxlength = 0;
      int index1 = 0, index2 = 0;
      int ascii[10000] = {0};
      while (index2 < s.length()) {
        while (ascii[s[index2]] == 0 && index2<s.length()) {
          ascii[s[index2]] += 1;
          index2 += 1;
          maxlength =
              ((index2 - index1) > maxlength) ? (index2 - index1) : maxlength;
        }
        if ( index2 == s.length()) {
          break;
        }
        ascii[s[index2]] = 2;
        index2+=1;
        while (ascii[s[index1]] != 2) {
          index1++;
        }
        ascii[s[index2]] = 1;
        }
    }
};