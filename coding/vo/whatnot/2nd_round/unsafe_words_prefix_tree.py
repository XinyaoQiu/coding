from collections import defaultdict

UNSAFE_WORDS = [
'Nintendo',
'Microsoft',
'SONY',
'usb-C',
]

messages = [
{
    "username": 'tom',
    "text": 'i am enjoying the nintendo switch',
},
{
    "username": 'jerry',
    "text": 'i prefer the wii',
},
{
    "username": 'tom',
    "text": 'my best friend is sonya',
},
{
    "username": 'tom',
    "text": 'do you have a usb charger',
},
{
    "username": 'jerry',
    "text": 'no but i do have a USB-C charger',
},
]
# "dafNintendo-doo"
# "Nintendo"
# " Nintendo fdsa"

class TrieNode:
    def __init__(self):
        self.children = defaultdict(TrieNode)
        self.end = 0

root = TrieNode()
for word in UNSAFE_WORDS:
    node = root
    for c in word.lower():
        node = node.children[c]
    node.end += 1
ans = []
for msg in messages:
    for word in msg['text'].strip().lower().split():
        node = root
        for c in word:
            node = node.children[c]
        if node.end > 0:
            ans.append(msg['text'])
            break
print(ans)