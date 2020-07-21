import requests
import io

with io.open('url.txt') as f:
    url = f.read().strip()

def send(msg):
    return requests.post(url, msg).text

with io.open('send.txt', 'rt') as fin:
    msg = fin.read().strip()
reply = send(msg)
with io.open('alien.txt', 'wt') as fout:
    fout.write(reply)
