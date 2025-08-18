#include <iostream>
#include <vector>
#include <unordered_map>
#include <queue>
using namespace std;

int solution(vector<int>& readings) {
    int non_digit = 0;
    for (int n : readings) {
        if (n >= 10) {
            ++non_digit;
        }
    }
    while (non_digit > 0) {
        for (int i = 0; i < readings.size(); ++i) {
            int sum = 0, curr = readings[i];
            if (curr < 10) {
                continue;
            }
            while (curr > 0) {
                sum += curr % 10;
                curr /= 10;
            }
            if (sum < 10) {
                --non_digit;
            }
            readings[i] = sum;
        }
    }
    unordered_map<int, int> m;
    for (int n : readings) {
        m[n]++;
    }
    int max_freq = 0, ans = 0;
    for (auto& p : m) {
        int num = p.first, freq = p.second;
        if (freq > max_freq || (freq == max_freq && num > ans)) {
            max_freq = freq;
            ans = num;
        }
    }
    return ans;
}

int main() {
    vector<int> readings({123, 456, 789, 101});
    cout << solution(readings) << endl;
}