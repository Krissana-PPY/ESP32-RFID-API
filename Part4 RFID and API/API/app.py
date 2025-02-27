from flask import Flask, request, jsonify
import mysql.connector

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
                    return jsonify({"uid": uid, "owner": user['owner_uid']}), 200
                else:
                    cursor.close()
                    conn.close()
                    return jsonify({"error": "Room not found"}), 404
            else:
                cursor.close()
                conn.close()
                return jsonify({"message": "No such UID found."}), 404  # แก้ข้อความให้ชัดเจนขึ้น
    else:
        return jsonify({"error": "MAC Address or UID not provided"}), 400


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
