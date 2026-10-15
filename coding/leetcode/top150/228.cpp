#include <vector>
#include <string>
using namespace std;

class Solution {
  public:
    vector<string> summaryRanges(vector<int>& nums) {
        vector<string> res;
        if (nums.empty()) return res;
        int start = nums[0], end = nums[0];
        for (int i = 1; i < nums.size(); i++) {
            if (nums[i] != end + 1) {
                if (start == end) res.emplace_back(to_string(start));
                else res.emplace_back(to_string(start) + "->" + to_string(end));
                start = end = nums[i];
            } else {
                end = nums[i];
            }
        }
        if (start == end) res.emplace_back(to_string(start));
        else res.emplace_back(to_string(start) + "->" + to_string(end));
        return res;
    }
};