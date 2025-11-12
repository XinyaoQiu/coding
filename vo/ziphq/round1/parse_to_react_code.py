import re

def parse_email_to_react(message: str) -> str:
    lines = message.split('\n')
    out, buf = [], []
    mode = None  # "p" for paragraph, "q" for quote

    def flush():
        nonlocal buf, mode
        if not mode:
            return
        content = '<Break />'.join(buf)
        tag = 'Paragraph' if mode == 'p' else 'Quote'
        out.append(f'<{tag}>{content}</{tag}>')
        buf.clear()
        mode = None

    for line in lines:
        if line.startswith('>'):  # quote line
            if mode == 'p': flush()
            mode = 'q'
            buf.append(line[1:])
            continue

        if line.strip() == '':  # blank line => end current block
            flush()
            continue

        if mode == 'q': flush()
        mode = 'p'
        buf.append(line)

    flush()
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
print(parse_email_to_react(message))
