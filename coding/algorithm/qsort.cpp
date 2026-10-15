#include <iostream>
#include <vector>
#include <tuple>
#include <sstream>

using namespace std;

pair<int, int> partition(vector<int>& arr, int L, int R) {
    srand(time(0));
    int p = rand() % (R - L) + L;
    swap(arr[p], arr[R]);
    p = R--;
    int i = L;
    while (L <= R) {
        if (arr[i] < arr[p]) {
            swap(arr[i++], arr[L++]);
        } else if (arr[i] > arr[p]) {
            swap(arr[i], arr[R--]);
        } else {
            i++;
        }
    }
    swap(arr[i++], arr[p]);
    return { L - 1, i};
}

void quicksort(vector<int>& arr, int L, int R) {
    if (L >= R) return;
    int left, right;
    tie(left, right) = partition(arr, L, R);
    quicksort(arr, L, left);
    quicksort(arr, right, R);
}


int main() {
    string line;
    getline(cin, line);
    istringstream iss(line);
    int num;
    vector<int> arr;
    while (iss >> num) {
        arr.push_back(num);
    }
    quicksort(arr, 0, arr.size() - 1);
    for (auto& i : arr) {
        cout << i << " ";
    }
    cout << endl;
}