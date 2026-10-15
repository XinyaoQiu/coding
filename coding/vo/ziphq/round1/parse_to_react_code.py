import re

def parse_email_to_react(message: str) -> str:
    lines = message.split('\n')
    out, buf = [], []
    mode = None
    def flush(buf, mode):
        if not mode:
            return
        content = '<Break />'.join(buf)
        tag = 'Paragraph' if mode == 'p' else 'Quote'
        out.append(f"<{tag}>{content}</{tag}>")
        buf.clear()
        mode = None
    for line in lines:
        if line == '':
            flush(buf, mode)
        elif line.startswith('>'):
            mode = 'q'
            buf.append(line[1:])
        else:
            mode = 'p'
            buf.append(line)
    flush(buf, mode)
    return '\n'.join(out)


# demo
message = (
    "Please tag @Bob Jones for approval!\n\n"
    "Here's the max amount we were allotted:\n\n"
    ">Max spend: $5000\n"
    ">Max spend per user: $12\n"
    ">Target spend: $3000\n\n"
    "Do you think it will be ok?\nOtherwise let's consider a new vendor."
)
"""
<Paragraph>Please tag @Bob Jones for approval!</Paragraph>
<Paragraph>Here's the max amount we were allotted:</Paragraph>
<Quote>Max spend: $5000<Break />Max spend per user: $12<Break />Target spend: $3000</Quote>
<Paragraph>Do you think it will be ok?<Break />Otherwise let's consider a new vendor.</Paragraph>
"""
print(parse_email_to_react(message))
