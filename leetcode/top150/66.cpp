#include <vector>
#include <algorithm>
using namespace std;

class Solution {
public:
    vector<int> plusOne(vector<int>& digits) {
        vector<int> ans(digits.begin(), digits.end());
        reverse(ans.begin(), ans.end());
        int check = 1;
        for (int i = 0; i < ans.size(); ++i) {
            check += ans[i];
            ans[i] = check % 10;
            check /= 10;
            if (check == 0) {
                break;
            }
        }
        if (check == 1) {
            ans.push_back(1);
        }
        reverse(ans.begin(), ans.end());
        return ans;
    }
};