#include <string>
#include <vector>
using namespace std;

// KMP

class Solution {
  public:
    int strStr(string haystack, string needle) {
        int n = haystack.size(), m = needle.size();
        vector<int> nexts(m);
        for (int i = 0, j = 1; j < m; j++) {
            while (i > 0 && needle[i] != needle[j]) {
                i = nexts[i - 1];
            }
            if (needle[i] == needle[j]) {
                i++;
            }
            nexts[j] = i;
        }
        for (int i = 0, j = 0; i < n; i++) {
            while (j > 0 && haystack[i] != needle[j]) {
                j = nexts[j - 1];
            }
            if (haystack[i] == needle[j]) {
                j++;
            }
            if (j == m) {
                return i - j + 1;
            }
        }
        return -1;
    }
};