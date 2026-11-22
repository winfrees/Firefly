"""Tiny S-expression reader/writer for KiCad files."""
import re

_tok = re.compile(r'\s*(?:(\()|(\))|("(?:[^"\\]|\\.)*")|([^\s()"]+))')


class Sym(str):
    """Bare atom (unquoted)."""


def parse(text):
    stack, cur = [], []
    pos = 0
    n = len(text)
    while pos < n:
        m = _tok.match(text, pos)
        if not m:
            if text[pos:].strip() == "":
                break
            raise ValueError("bad token at %d" % pos)
        pos = m.end()
        o, c, q, a = m.groups()
        if o:
            stack.append(cur)
            cur = []
        elif c:
            done = cur
            cur = stack.pop()
            cur.append(done)
        elif q is not None:
            cur.append(q[1:-1].replace('\\"', '"').replace("\\\\", "\\"))
        elif a is not None:
            cur.append(Sym(a))
    return cur[0] if len(cur) == 1 else cur


def dump(x, indent=0):
    sp = "  " * indent
    if isinstance(x, list):
        simple = all(not isinstance(e, list) for e in x)
        if x and not isinstance(x[0], list):
            x = [Sym(x[0])] + list(x[1:])
        if simple:
            return sp + "(" + " ".join(atom(e) for e in x) + ")"
        head = []
        rest = []
        for i, e in enumerate(x):
            if isinstance(e, list):
                rest = x[i:]
                break
            head.append(e)
        out = sp + "(" + " ".join(atom(e) for e in head)
        for e in rest:
            if isinstance(e, list):
                out += "\n" + dump(e, indent + 1)
            else:
                out += " " + atom(e)
        return out + ")"
    return sp + atom(x)


def atom(e):
    if isinstance(e, Sym):
        return str(e)
    if isinstance(e, bool):
        return "yes" if e else "no"
    if isinstance(e, (int, float)):
        return fmt(e)
    s = str(e).replace("\\", "\\\\").replace('"', '\\"')
    return '"' + s + '"'


def fmt(v):
    if isinstance(v, int):
        return str(v)
    s = ("%.4f" % v).rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def find(node, key):
    return [e for e in node if isinstance(e, list) and e and e[0] == key]


def first(node, key):
    r = find(node, key)
    return r[0] if r else None
