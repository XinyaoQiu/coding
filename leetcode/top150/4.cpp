#include <vector>
using namespace std;

// [1, 3, 5, 7(left)(i=3), 9(right)]
// [2, 4, 6(j=2), 8, 10, 11]
// 1, 2, 3, 4, 5, [6], 7, 8, 9, 10, 11
// i + j = (m + n) / 2

// [2(i=0)]
// [1, 3(j=1)]

class Solution {
public:
    double findMedianSortedArrays(vector<int>& nums1, vector<int>& nums2) {
        if (nums1.size() > nums2.size()) {
            swap(nums1, nums2);
        }
        int m = nums1.size(), n = nums2.size();
        int left = 0, right = m;
        while (left <= right) {
            int i = (left + right) / 2;
            int j = (m + n) / 2 - i;
            if ((i == 0 || j == n || nums1[i - 1] <= nums2[j])) {
                left = i + 1;
            } else {
                right = i - 1;
            }
        }
        int i = right, j = (m + n) / 2 - i;
        int aMax1 = i > 0 ? nums1[i - 1] : INT_MIN;
        int aMax2 = j > 0 ? nums2[j - 1] : INT_MIN;
        int bMin1 = i < m ? nums1[i] : INT_MAX;
        int bMin2 = j < n ? nums2[j] : INT_MAX;
        if ((m + n) % 2 == 1) {
            return min(bMin1, bMin2);
        }
        return (max(aMax1, aMax2) + min(bMin1, bMin2)) / 2.0;
    }
};

