#include <set>
#include <vector>
using namespace std;

class MedianFinder {
    multiset<int> nums;
    multiset<int>::iterator left, right;

  public:
    MedianFinder() {}

    void addNum(int num) {
        int sz = nums.size();
        nums.insert(num);
        if (sz == 0) {
            left = right = nums.begin();
        } else if (sz % 2 == 1) {
            if (num < *left) {
                left--;
            } else {
                right++;
            }
        } else if (sz % 2 == 0) {
            if (num < *left) {
                right--;
            } else if (num >= *right) {
                left++;
            } else {
                left++;
                right--;
            }
        }
    }

    double findMedian() {
        return (*left + *right) / 2.0;
    }
};

/**
 * Your MedianFinder object will be instantiated and called as such:
 * MedianFinder* obj = new MedianFinder();
 * obj->addNum(num);
 * double param_2 = obj->findMedian();
 */