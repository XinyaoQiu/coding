#include <vector>
using namespace std;

class Solution {
    void binaryInsert(vector<int>& arr, int val) {
        int L = 0, R = arr.size() - 1;
        while (L <= R) {
            int M = L + (R - L) / 2;
            if (arr[M] >= val) {
                R = M - 1;
            } else {
                L = M + 1;
            }
        }
        arr[L] = val;
    }
public:
    int lengthOfLIS(vector<int>& nums) {
        vector<int> arr;
        for (int n : nums) {
            if (arr.empty() || n > arr.back()) {
                arr.push_back(n);
            } else {
                binaryInsert(arr, n);
            }
        }
        return arr.size();
    }
};