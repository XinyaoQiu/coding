"""
parse_json('{"hello": "world"}') -> {"hello": "world"}
parse_json('{"he\\"llo": "world"}') -> {'he"llo': "world"}

write the function parse_json to support the examples
"""

def parse_json(s):
    def parse(i):
        d = {}
        buf = []
        in_str = False
        key = None
        while i < len(s):
            if in_str:
                if s[i] == '\\':
                    buf.append(s[i + 1])
                    i += 2
                elif s[i] == '"':
                    in_str = False
                    token = "".join(buf)
                    buf = []
                    if key is None:
                        key = token
                    else:
                        d[key] = token
                        key = None
                    i += 1
                else:
                    buf.append(s[i])
                    i += 1
            else:
                if s[i] == '"':
                    in_str = True
                    i += 1
                elif s[i] == '{':
                    d[key], i = parse(i + 1)
                    key = None
                elif s[i] == '}':
                    return d, i + 1
                else:
                    i += 1
        return d, i

    d, _ = parse(s.index('{') + 1)
    return d

print(parse_json('{"hello": "world", "wor,d": {"hel\\"lo", "world"}}'))