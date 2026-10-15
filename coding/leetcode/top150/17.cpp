#include <vector>
#include <string>
using namespace std;

class Solution {
    const string MAPPING[8] = {"abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz"};
    vector<string> ans;
    string path;
    void dfs(string& digits, int i) {
        if (i == digits.size()) {
            if (path != "") {
                ans.emplace_back(path);
            }
            return;
        }
        for (char it : MAPPING[digits[i] - '2']) {
            path.push_back(it);
            dfs(digits, i + 1);
            path.pop_back();
        }
    }
public:
    vector<string> letterCombinations(string digits) {
        dfs(digits, 0);
        return ans;
    }
};
