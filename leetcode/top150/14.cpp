#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    string longestCommonPrefix(vector<string>& strs) {
        int len = 0, n = strs.size();
        while (1) {
            for (int i = 0; i < n; i++) {
                if (len == strs[i].size() || strs[i][len] != strs[0][len]) {
                    return strs[0].substr(0, len);
                }
            }
            len++;
        }
    }
};
