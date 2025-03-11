from flask import Flask, send_from_directory
from flask_sock import Sock
import json
import mysql.connector

app = Flask(__name__)
sock = Sock(app)

# เก็บ WebSocket connections
connected_clients = {}

# ฟังก์ชันเชื่อมต่อฐานข้อมูล MySQL
def get_db_connection():
    return mysql.connector.connect(
        host="localhost",
        user="root",
        password="1234",
        database="esp32_access_db"
    )

@app.route('/')
def index():
    return send_from_directory('.', 'index.html')

@sock.route('/ws')
def websocket(ws):
    db = get_db_connection()
    cursor = db.cursor()

    try:
        # สร้าง session_id สำหรับ client
        session_id = str(id(ws))  
        connected_clients[session_id] = ws  # เก็บ WebSocket instance

        while True:
            data = ws.receive()
            if not data:
                break  # ถ้าไม่มีข้อมูลให้หยุดลูป

            try:
                payload = json.loads(data)
                command = payload.get('command')
                room_name = payload.get('room_name')
                uid = payload.get('uid')
                mac_address = payload.get('mac_address')
                status = payload.get('status')

                if command == 'open_room' and room_name:
                    cursor.execute("SELECT ss_id FROM classrooms WHERE room_name = %s", (room_name,))
                    result = cursor.fetchone()
                    if result:
                        session_id = result[0]
                        if session_id in connected_clients:
                            connected_clients[session_id].send(json.dumps({"status": "open"}))
                        else:
                            ws.send(json.dumps({"status": "error", "message": "Session not found"}))
                        print(f"Room {room_name} opened for session {session_id}")
                    else:
                        ws.send(json.dumps({"status": "error", "message": "Room not found"}))

                elif mac_address and not uid:
                    # บันทึก Session ID
                    cursor.execute("UPDATE classrooms SET ss_id = %s WHERE mac_address = %s", (session_id, mac_address))
                    db.commit()
                    ws.send(json.dumps({"status": "success", "mac_address": mac_address, "sid": session_id}))
                    print("Mac Address : ", mac_address, "Session ID : ", session_id)

                elif uid and mac_address:
                    print(f"Received: UID={uid}, MAC={mac_address}, Status={status}")

                    # ดึงชื่อห้องจาก MAC Address
                    cursor.execute("SELECT room_name FROM classrooms WHERE mac_address = %s", (mac_address,))
                    result = cursor.fetchone()

                    if result:
                        room_name = result[0]

                        if status:
                            cursor.execute("INSERT INTO attendance_logs (uid, room_name) VALUES (%s, %s)", (uid, room_name))
                            db.commit()
                            ws.send(json.dumps({"status": "success"}))
                        else:
                            cursor.execute("SELECT owner_uid FROM users WHERE uid = %s", (uid,))
                            user_result = cursor.fetchone()

                            if user_result:
                                owner = user_result[0]
                                cursor.execute("INSERT INTO attendance_logs (uid, room_name) VALUES (%s, %s)", (uid, room_name))
                                db.commit()
                                ws.send(json.dumps({"status": "success", "uid": uid, "owner": owner}))
                            else:
                                ws.send(json.dumps({"status": "error", "message": "UID not found"}))
                    else:
                        ws.send(json.dumps({"status": "error", "message": "MAC address not found"}))
                else:
                    ws.send(json.dumps({"status": "error", "message": "Invalid data"}))

            except json.JSONDecodeError:
                ws.send(json.dumps({"status": "error", "message": "Invalid JSON"}))

    except Exception as e:
        print(f"Error: {e}")
    finally:
        # ลบ session ออกจาก dictionary เมื่อ client disconnect
        connected_clients.pop(session_id, None)
        cursor.close()
        db.close()

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
