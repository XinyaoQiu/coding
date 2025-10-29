
class SnapshotSetIterator:
    def __init__(self, arr=[]):
        self.arr = arr
        self.it = 0

    def has_next(self):
        return self.it < len(self.arr)

    def next(self):
        if not self.has_next():
            return -1
        ret = self.arr[self.it]
        self.it += 1
        return ret

class SnapshotSet:
    def __init__(self, arr=[]):
        self.mp = {}
        for x in arr:
            self.mp[x] = 0
    
    def add(self, x):
        self.mp[x] = 0
    
    def remove(self, x):
        if x in self.mp:
            del self.mp[x]
    
    def iterator(self):
        return SnapshotSetIterator(list(self.mp.keys()))

ss = SnapshotSet([1, 2, 3])
ss.add(4)
iter1 = ss.iterator()
print(iter1.next())
print(iter1.next())
ss.remove(3)
print(iter1.next())
iter2 = ss.iterator()
ss.add(6)
print(iter2.next())