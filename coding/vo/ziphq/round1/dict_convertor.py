from typing import *

def is_valid(s: str) -> bool:
    return all([c.isdigit() for c in s])

def convert(s: str) -> str:
    return ''.join([chr(ord(c) + 1) for c in s])


def func1(mp: Dict[str, str]) -> Dict:
    valid = all([is_valid(v) for v in mp.values()])
    if valid:
        new_mp = {}
        for k, v in mp.items():
            new_mp[k] = convert(v)
        return new_mp
    return mp

mp = {'a': '123', 'b': '234', 'c': '1ab'}
print(func1(mp))
mp = {'a': '123', 'b': '234', 'c': '345'}
print(func1(mp))

def func2(mp: Dict[str, Dict]) -> Dict:
    memo = {}
    def check_valid(mp):
        for v in mp.values():
            if isinstance(v, str):
                if not is_valid(v):
                    return False
            elif isinstance(v, dict):
                if not check_valid(v):
                    return False
        return True     
    if not check_valid(mp):
        return mp
    def convert_mp(mp):
        new_mp = {}
        for k, v in mp.items():
            if isinstance(v, str):
                if v not in memo:
                    memo[v] = convert(v)
                new_mp[k] = memo[v]
            elif isinstance(v, dict):
                new_mp[k] = convert_mp(v)
        return new_mp
    new_mp = convert_mp(mp)
    print(f"memo = {memo}")
    return new_mp
                
mp = {'a': '123', 'b': {'d': '123', 'e': {'f': '123'}}, 'c': '123'}
print(func2(mp))
mp = {'a': '124', 'b': {'d': '45a6', 'e': {'f': '154'}}, 'c': '137'}
print(func2(mp))