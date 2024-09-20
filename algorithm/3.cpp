#include <string>
#include <vector>
#include <iostream>
#include <unordered_set>
using namespace std;


class Solution {
public:
    int lengthOfLongestSubstring(string s) {
      if (!s.length()) return 0;
      int count[1000] = {0};
      int i = 0, j = 1, max = 1;
      while (j < s.length()) {
        count[s[i]] = 1;
        while (!count[s[j]] && j < s.length()) {
          count[s[j]] = 1;
          j++;
        }
        max = std::max(j - i, max);
        count[s[i]] = 0;
        i++;
        if (j == i) j++;
      }
      return max;
    }
};

int main() {
  Solution solution;
  string s = "pwwkew";
  cout << solution.lengthOfLongestSubstring(s) << endl;
  return 0;
}