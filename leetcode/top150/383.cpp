#include <string>
#include <vector>
using namespace std;

class Solution {
  public:
    bool canConstruct(string ransomNote, string magazine) {
        vector<int> count_ransomNote(26);
        vector<int> count_magazine(26);
        for (char c : magazine) {
            count_magazine[c - 'a']++;
        }
        for (char c : ransomNote) {
            count_ransomNote[c - 'a']++;
            if (count_ransomNote[c - 'a'] > count_magazine[c - 'a']) {
                return false;
            }
        }
        return true;
    }
};