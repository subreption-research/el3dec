import websocket

ws = websocket.WebSocket()
ws.connect("ws://localhost:8081")

telesamples = open('tests/fixtures/telemetry-samples.txt', 'r')
for line in telesamples.readlines():
    line = line.strip()
    ws.send(line)
    print(ws.recv())
