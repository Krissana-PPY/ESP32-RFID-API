from flask import Flask, request, jsonify
import mysql.connector
import requests

ESP32_IP = "http://172.20.10.2:80"  # Update with the correct IP of the ESP32

app = Flask(__name__)

def get_db_connection():
    return mysql.connector.connect(
        host="localhost",
        user="root",
        password="1234",
        database="esp32_access_db"
    )

@app.route('/api/mac', methods=['POST'])
def receive_mac_address():
    data = request.get_json()
    mac_address = data.get('mac_address')
    uid = data.get('uid')
    uid_found = data.get('uid_found')

    if mac_address and uid is not None:
        conn = get_db_connection()
        cursor = conn.cursor(dictionary=True)

        if uid_found:
            cursor.execute("SELECT room_name FROM classrooms WHERE mac_address = %s", (mac_address,))
            room = cursor.fetchone()
            if room:
                cursor.execute("SELECT owner_uid FROM users WHERE uid = %s", (uid,))
                user = cursor.fetchone()
                if user:
                    cursor.execute("INSERT INTO attendance_logs (uid, owner_uid, room_name) VALUES (%s, %s, %s)",
                                   (uid, user['owner_uid'], room['room_name']))
                    conn.commit()
                    cursor.close()
                    conn.close()
                    return jsonify({"message": "Attendance logged"}), 200
                else:
                    cursor.close()
                    conn.close()
                    return jsonify({"error": "UID not found"}), 404
            else:
                cursor.close()
                conn.close()
                return jsonify({"error": "Room not found"}), 404
        else:
            cursor.execute("SELECT owner_uid FROM users WHERE uid = %s", (uid,))
            user = cursor.fetchone()
            if user:
                cursor.execute("SELECT room_name FROM classrooms WHERE mac_address = %s", (mac_address,))
                room = cursor.fetchone()
                if room:
                    cursor.execute("INSERT INTO attendance_logs (uid, owner_uid, room_name) VALUES (%s, %s, %s)",
                                   (uid, user['owner_uid'], room['room_name']))
                    conn.commit()
                    cursor.close()
                    conn.close()
                    return jsonify({"owner_uid": user['owner_uid']}), 200
                else:
                    cursor.close()
                    conn.close()
                    return jsonify({"error": "Room not found"}), 404
            else:
                cursor.close()
                conn.close()
                return jsonify({"message": "No such UID found."}), 404
    else:
        return jsonify({"error": "MAC Address or UID not provided"}), 400

@app.route('/api/add', methods=['POST'])
def add_uid():
    data = request.get_json()
    if not data or 'uid' not in data or 'owner' not in data:
        return jsonify({"error": "Missing UID or owner"}), 400

    try:
        response = requests.post(f"{ESP32_IP}/api/add", data={"uid": data['uid'], "owner": data['owner']})
        return response.json(), response.status_code
    except requests.exceptions.RequestException as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/delete', methods=['POST'])
def delete_uid():
    data = request.get_json()
    if not data or 'uid' not in data:
        return jsonify({"error": "Missing UID"}), 400

    try:
        response = requests.post(f"{ESP32_IP}/api/delete", data={"uid": data['uid']})
        return response.json(), response.status_code
    except requests.exceptions.RequestException as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/request', methods=['GET'])
def request_uids():
    try:
        response = requests.get(f"{ESP32_IP}/api/request")
        return response.json(), response.status_code
    except requests.exceptions.RequestException as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/open', methods=['POST'])
def open_door():
    try:
        response = requests.post(f"{ESP32_IP}/api/open")
        return response.json(), response.status_code
    except requests.exceptions.RequestException as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
