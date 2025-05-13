#include <vector>
#include <string>
using namespace std;

class Solution {
  public:
    vector<string> fullJustify(vector<string>& words, int maxWidth) {
        vector<string> res;
        int i = 0;
        while (i < words.size()) {
            int j = i + 1;
            int len = words[i].size();
            while (j < words.size() && len + words[j].size() + 1 <= maxWidth) {
                len += words[j].size() + 1;
                j++;
            }
            string s = words[i];
            if (j == words.size()) {
                for (int k = i + 1; k < j; k++) {
                    s += " " + words[k];
                }
            } else {
                int numInternal = j - i - 1;
                int remain = maxWidth - len + numInternal;
                for (int k = i + 1; k < j; k++) {
                    s.append(remain / numInternal + (k - i <= remain % numInternal), ' ');
                    s += words[k];
                }
            }
            s.append(maxWidth - s.size(), ' ');
            res.emplace_back(s);
            i = j;
        }
        return res;
    }
};