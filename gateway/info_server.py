"""
ESP LAN JSON Receiver (Flask Server)

This script starts a simple HTTP server that listens for JSON POST requests from ESP devices
on the local network. It prints received data and responds with a confirmation.

Usage:
    1. Make sure your ESP devices are on the same LAN as this server.
    2. Run the server:
         python info_server.py
    3. ESP devices should send POST requests with JSON payload to:
         http://10.0.0.2:8000/
"""

from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route('/', methods=['POST'])
def receive_info():
    data = request.get_json()
    if not data:
        return jsonify({"status": "error", "reason": "no JSON received"}), 400

    # Print the received JSON to verify
    print("Received JSON from ESP:")
    print(data)

    return jsonify({"status": "ok"}), 200

if __name__ == "__main__":
    # Listen on the fixed IP 10.255.255.254 so ESPs on the LAN can reach it
    app.run(host="10.0.0.2", port=8000)
