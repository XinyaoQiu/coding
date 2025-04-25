#include <vector>
using namespace std;

class Solution {
  public:
    int majorityElement(vector<int>& nums) {
        int cur = 0, count = 0;
        for (int x : nums) {
            if (count == 0 || cur == x) {
                cur = x;
                count++;
            } else {
                count--;
            }
        }
        return cur;
    }
};