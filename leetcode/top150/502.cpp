#include <vector>
#include <algorithm>
#include <queue>
using namespace std;

/*
    [3, 2, 1]
*/

class Solution {
public:
    int findMaximizedCapital(int k, int w, vector<int>& profits, vector<int>& capital) {
        priority_queue<int, vector<int>, less<int>> pq;
        vector<pair<int, int>> capitals;
        int n = profits.size();
        for (int i = 0; i < n; i++) {
            capitals.emplace_back(capital[i], profits[i]);
        }
        sort(capitals.begin(), capitals.end());
        int i = 0;
        while (k-- > 0) {
            while (i < n && capitals[i].first <= w) {
                pq.push(capitals[i++].second);
            }
            if (pq.empty()) break;
            w += pq.top();
            pq.pop();
        }
        return w;
    }
};