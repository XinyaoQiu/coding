#include <vector>
#include <unordered_map>
#include <random>
using namespace std;

class RandomizedSet {
    vector<int> v;
    unordered_map<int, int> m;
  public:
    RandomizedSet() {}

    bool insert(int val) {
        if (m.count(val)) return false;
        v.push_back(val);
        m[val] = v.size() - 1;
        return true;
    }

    bool remove(int val) {
        if (!m.count(val)) return false;
        int size = v.size();
        m[v[size - 1]] = m[val];
        swap(v[m[val]], v[size - 1]);
        v.pop_back();
        m.erase(val);
        return true;
    }

    int getRandom() {
        int size = v.size();
        return v[rand() % size];
    }
};

/**
 * Your RandomizedSet object will be instantiated and called as such:
 * RandomizedSet* obj = new RandomizedSet();
 * bool param_1 = obj->insert(val);
 * bool param_2 = obj->remove(val);
 * int param_3 = obj->getRandom();
 */