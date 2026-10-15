#include <vector>
#include <iostream>
using namespace std;

int solution(vector<int>& a) {
    int ans = 0;
    for (int n : a) {
        int count = 0;
        while (n > 0) {
            if (n % 10 == 0) {
                ++count;
            }
            n /= 10;
        }
        if (count % 2 == 1) {
            ++ans;
        }
    }
    return ans;
}

int main() {
    vector<int> a({20, 11, 10, 10070, 7});
    cout << solution(a) << endl;
}