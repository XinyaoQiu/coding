#include <vector>
using namespace std;

class Solution {
    int lower_bound(vector<int>& arr, int target) {
        int L = 0, R = arr.size() - 1;
        while (L <= R) {
            int M = L + (R - L) / 2;
            if (arr[M] < target) {
                L = M + 1;
            } else {
                R = M - 1;
            }
        }
        return L;
    }
  public:
    int minimumOperations(vector<int>& nums) {
        vector<int> arr;
        for (int x : nums) {
            if (arr.empty() || x >= arr.back()) {
                arr.emplace_back(x);
            } else {
                arr[lower_bound(arr, x + 1)] = x;
            }
        }
        return nums.size() - arr.size();
    }
};