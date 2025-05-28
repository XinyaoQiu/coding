#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

class Solution {
  public:
    bool isIsomorphic(string s, string t) {
        unordered_map<char, char> m_s;
        unordered_map<char, char> m_t;
        for (int i = 0; i < s.size(); ++i) {
            if ((m_s.count(s[i]) && m_s[s[i]] != t[i]) || (m_t.count(t[i]) && m_t[t[i]] != s[i])) {
                return false;
            }
            m_s[s[i]] = t[i];
            m_t[t[i]] = s[i];
        }
        return true;
    }
};