messages = ["I have a new idea", "I don't think so.", "Please trust me.", "No you're wrong"]
invalid_keywords = ["have", "you"]

class TrieNode:
    def __init__(self):
        self.children = [None] * 128
        self.end = set()

def func(messages, invalid_keywords):
    root = TrieNode()
    for i, msg in enumerate(messages):
        for word in msg.strip().split():
            node = root
            for c in word:
                if node.children[ord(c)] is None:
                    node.children[ord(c)] = TrieNode()
                node = node.children[ord(c)]
            node.end.add(i)
        
