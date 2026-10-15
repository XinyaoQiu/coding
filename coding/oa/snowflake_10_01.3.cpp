#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

// 订单结构体
struct Order {
    int start;   // 开始时间
    int duration; // 持续时间
    int volume;  // 订单量
    int end;     // 结束时间，计算为 start + duration
};

bool compareByEnd(const Order &a, const Order &b) {
    return a.end < b.end;
}

int findNonOverlappingOrder(const vector<Order> &orders, int current) {
    int low = 0, high = current - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (orders[mid].end <= orders[current].start) {
            if (orders[mid + 1].end <= orders[current].start) {
                low = mid + 1;
            } else {
                return mid;
            }
        } else {
            high = mid - 1;
        }
    }
    return -1;
}

int maxOrderVolume(vector<int> &start, vector<int> &duration, vector<int> &volume) {
    int n = start.size();
    vector<Order> orders(n);

    for (int i = 0; i < n; i++) {
        orders[i].start = start[i];
        orders[i].duration = duration[i];
        orders[i].volume = volume[i];
        orders[i].end = start[i] + duration[i];
    }

    sort(orders.begin(), orders.end(), compareByEnd);

    vector<int> dp(n, 0);
    dp[0] = orders[0].volume;
    for (int i = 1; i < n; i++) {
        int includeCurrent = orders[i].volume;

        int lastNonOverlap = findNonOverlappingOrder(orders, i);
        if (lastNonOverlap != -1) {
            includeCurrent += dp[lastNonOverlap];
        }
        dp[i] = max(dp[i - 1], includeCurrent);
    }

    return dp[n - 1];
}

int main() {
    vector<int> start = {10, 5, 15, 18, 30};
    vector<int> duration = {30, 12, 20, 35, 35};
    vector<int> volume = {50, 51, 20, 25, 10};
    cout << "Maximum order volume: " << maxOrderVolume(start, duration, volume) << endl;

    return 0;
}
