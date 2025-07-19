#include <vector>
#include <queue>
#include <algorithm>
using namespace std;

class Solution {
public:
    int findMaximizedCapital(int k, int w, vector<int>& profits, vector<int>& capital) {
        vector<pair<int, int>> capitals;
        int n = profits.size();
        for (int i = 0; i < n; ++i) {
            capitals.emplace_back(capital[i], profits[i]);
        }
        sort(capitals.begin(), capitals.end());
        priority_queue<int, vector<int>, less<int>> pq;
        int i = 0;
        while (k-- > 0) {
            while (i < n && capitals[i].first <= w) {
                pq.emplace(capitals[i++].second);
            }
            if (pq.empty()) {
                break;
            }
            w += pq.top();
            pq.pop();
        }
        return w;
    }
};