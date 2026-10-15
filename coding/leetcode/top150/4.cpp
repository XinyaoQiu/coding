#include <vector>
#include <climits>
using namespace std;

// [1, 3, 5, 7(left)(right), 9]
// [2, 4, 6(j), 8, 10, 11]
// 1, 2, 3, 4, 5, [6], 7, 8, 9, 10, 11
// i + j = (m + n) / 2
// nums1(i): i++; possible->impossible
// nums2(j): j--==i++; impossible->possible
// only one (i, j) that nums1 possible & nums2 possible
// so this *only one* must be the last one that nums1(i) possible
// nums1: p p p p p i i i i 
// nums2: i i i i i p p p p 

class Solution {
public:
    double findMedianSortedArrays(vector<int>& nums1, vector<int>& nums2) {
        if (nums1.size() > nums2.size()) {
            swap(nums1, nums2);
        }
        int m = nums1.size(), n = nums2.size();
        int left = 0, right = m;
        while (left <= right) {
            int i = (left + right) / 2, j = (m + n) / 2 - i;
            if (i == 0 || nums1[i - 1] <= nums2[j]) {
                left = i + 1;
            } else {
                right = i - 1;
            }
        }
        int i = right, j = (m + n) / 2 - i;
        int a1 = i > 0 ? nums1[i - 1] : INT_MIN;
        int a2 = j > 0 ? nums2[j - 1] : INT_MIN;
        int b1 = i < m ? nums1[i] : INT_MAX;
        int b2 = j < n ? nums2[j] : INT_MAX;
        if ((m + n) % 2 == 1) {
            return min(b1, b2);
        }
        return (max(a1, a2) + min(b1, b2)) / 2.0;
    }
};


