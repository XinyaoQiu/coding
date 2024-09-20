#include <iostream>
#include <vector>

using namespace std;

void heapInsert(vector<int>& arr, int index) {
    while (arr[index] > arr[(index - 1) / 2]) {
        swap(arr[index], arr[(index - 1) / 2]);
        index = (index - 1) / 2;
    }
}


void heapify(vector<int>& arr, int index, int heapSize) {
    int left = index * 2 + 1;
    while (left < heapSize) {
        int largest = left + 1 < heapSize && arr[left] < arr[left + 1] ? left + 1 : left;
        largest = arr[index] < arr[largest] ? largest : index;
        if (index == largest) {
            break;
        }
        swap(arr[largest], arr[index]);
        index = largest;
        left = index * 2 + 1;
    }
}

int main() {
    vector<int> heap({4, 2, 3, 6, 9, 8, 0, 1, 4, 7});
    int heapSize = 0;
    for (auto& i : heap) {
        heapInsert(heap, heapSize++);
    }
    while (heapSize > 0) {
        swap(arr[0], arr[--heapSize]);
        heapify(arr, 0, heapSize);
    }
    for (auto& i : heap) {
        cout << i << " ";
    }
    cout << endl;
}