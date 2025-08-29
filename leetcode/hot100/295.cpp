#include <queue>
using namespace std;

class MedianFinder {
    priority_queue<int> smaller_pq;
    priority_queue<int, vector<int>, greater<int>> larger_pq;
public:
    MedianFinder() {
    }
    
    void addNum(int num) {
        if (smaller_pq.empty()) {
            smaller_pq.push(num);
        } else if (larger_pq.empty()) {
            larger_pq.push(num);
        } else if (smaller_pq.size() > larger_pq.size()) {
            if (num < smaller_pq.top()) {
                larger_pq.push(smaller_pq.top());
                smaller_pq.pop();
                smaller_pq.push(num);
            } else {
                larger_pq.push(num);
            }
        } else {
            if (num > larger_pq.top()) {
                smaller_pq.push(larger_pq.top());
                larger_pq.pop();
                larger_pq.push(num);
            } else {
                smaller_pq.push(num);
            }
        }
    }
    
    double findMedian() {
        if (smaller_pq.empty() && larger_pq.empty()) {
            return 0.0;
        }
        if (smaller_pq.size() > larger_pq.size()) {
            return smaller_pq.top();
        }
        return (smaller_pq.top() + larger_pq.top()) / 2.0;
    }
};

/**
 * Your MedianFinder object will be instantiated and called as such:
 * MedianFinder* obj = new MedianFinder();
 * obj->addNum(num);
 * double param_2 = obj->findMedian();
 */