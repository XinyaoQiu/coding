#include <string>
#include <vector>
#include <unordered_set>
using namespace std;

class Solution {
public:
    string minRemoveToMakeValid(string s) {
        vector<int> stk;
        string ans;
        unordered_set<int> to_delete;
        for (int i = 0; i < s.size(); ++i) {
            if (stk.empty() && s[i] == ')') {
                to_delete.insert(i);
            } else if (s[i] == '(') {
                stk.push_back(i);
            } else if (s[i] == ')') {
                stk.pop_back();
            }
        }
        for (int index : stk) {
            to_delete.insert(index);
        }
        for (int i = 0; i < s.size(); ++i) {
            if (!to_delete.count(i)) {
                ans.push_back(s[i]);
            }
        }
        return ans;
    }
};