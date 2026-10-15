#include <iostream>
#include <vector>
#include <set>
#include <climits>
using namespace std;

int solution(vector<int>& heights, int viewingGap) {
    int ans = INT_MAX;
    multiset<int> ms;
    for (int i = viewingGap; i < heights.size(); ++i) {
        ms.insert(heights[i - viewingGap]);
        auto it = ms.lower_bound(heights[i]);
        if (it != ms.end()) {
            ans = min(ans, abs(*it - heights[i]));
        } else if (it != ms.begin()) {
            --it;
            ans = min(ans, abs(*it - heights[i]));
        }
    }
    return ans;
}

int main() {
    vector<int> heights({3, 10, 5, 8});
    cout << solution(heights, 1) << endl;
}