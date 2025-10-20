def wordWrap(words, maxLen):
    if len(words) == 0 or len(words[0]) > maxLen:
        return ""
    result = words[0]
    for word in words[1:]:
        if len(result) + len(word) + 1 > maxLen:
            break
        result += "-" + word
    return result

print(wordWrap(["I", "will", "have", "dinner", "now."], 12))

def reflowAndJustify(lines, maxLen):
    ans = []
    words = (" ".join(lines)).split()
    i = 0
    while i < len(words):
        curr = [words[i]]
        curr_len = len(curr[0])
        i += 1
        while i < len(words) and curr_len + len(words[i]) + 1 <= maxLen:
            curr.append(words[i])
            curr_len += len(words[i]) + 1
            i += 1
        if len(curr) == 1:
            ans.append(curr[0])
        else:
            remain = maxLen - curr_len
            k, b = remain // (len(curr) - 1), remain % (len(curr) - 1)
            left = ('-' * (k + 2)).join(curr[:b])
            mid = '-' if b > 0 else ''
            right = ('-' * (k + 1)).join(curr[b:])
            ans.append(left + mid + right)
    return ans
        
        

lines = [ "The day began as still as the",
          "night abruptly lighted with",
          "brilliant flame" ]

print(reflowAndJustify(lines, 24))
    