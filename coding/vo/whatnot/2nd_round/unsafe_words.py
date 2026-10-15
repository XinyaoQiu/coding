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

unsafe_words = set()
for word in UNSAFE_WORDS:
    unsafe_words.add(word.lower())
result = []
for msg in messages:
    for word in msg['text'].strip().split():
        if word.lower() in unsafe_words:
            result.append(msg['text'])
print(result)
# O(k * n * m)