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
    # Listen on all interfaces (0.0.0.0) so ESPs on the LAN can reach it
    app.run(host="0.0.0.0", port=8000)
