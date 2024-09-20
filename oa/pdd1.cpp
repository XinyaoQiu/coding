#include <iostream>
#include <vector>
#include <sstream>

using namespace std;

int process(vector<char>& arr, int L, int R) {
    if (L > R) {
        return 0;
    }
    vector<char> pdd({'P', 'D', 'D'});
    int result = 0;
    for (auto i =  L; i <= R; i++) {
        result += abs(arr[i] - pdd[(i - L) % 3]);
    }
    return result;
}

int main() {
    int N;
    cin >> N;
    char c;
    vector<char> arr;
    for (auto i = 0; i < N; i++) {
        cin >> c;
        arr.push_back(c);
    }
    if (N <= 2) {
        cout << 0 << " " << 0 << endl;
        return 0;
    }
    cout << N / 3 << " ";
    if (N % 3 == 0) {
        cout << process(arr, 0, N - 1) << endl;
    } else if (N % 3 == 1) {
        int result = INT32_MAX;
        for (auto i = 0; i < N; i += 3) {
            result = min(result, process(arr, 0, i - 1) + process(arr, i + 1, N - 1));
        }
        cout << result << endl;
    } else {
        int result = INT32_MAX;
        for (auto i = 0; i < N; i += 3) {
            for (auto j = i + 1; j < N; j += 3) {
                result = min(result, process(arr, 0, i - 1) + process(arr, i + 1,
                             j - 1) + process(arr, j + 1, N - 1));
            }
        }
        cout << result << endl;
    }

}