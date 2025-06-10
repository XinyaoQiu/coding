#include <vector>
using namespace std;

class Solution {
  public:
    void merge(vector<int>& nums1, int m, vector<int>& nums2, int n) {
        int p1 = m - 1, p2 = n - 1, i = m + n - 1;
        while (p1 >= 0 && p2 >= 0) {
            nums1[i--] = nums1[p1] <= nums2[p2] ? nums2[p2--] : nums1[p1--];
        }
        while (p2 >= 0) {
            nums1[i--] = nums2[p2--];
        }
    } 
};