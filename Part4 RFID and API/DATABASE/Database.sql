CREATE DATABASE IF NOT EXISTS esp32_access_db;
USE esp32_access_db;

-- ตารางเก็บ MAC Address และห้องเรียน
CREATE TABLE classrooms (
    id INT AUTO_INCREMENT PRIMARY KEY,
    mac_address VARCHAR(17) UNIQUE NOT NULL,
    room_name VARCHAR(10) NOT NULL
);

-- ตารางเก็บข้อมูลผู้ใช้ (UID และชื่อ)
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    uid VARCHAR(20) UNIQUE NOT NULL,
    owner_uid VARCHAR(50) NOT NULL
);

-- ตารางบันทึกการเข้าห้องเรียน
CREATE TABLE attendance_logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    uid VARCHAR(20) NOT NULL,
    owner_uid VARCHAR(50) NOT NULL,
    room_name VARCHAR(10) NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (uid) REFERENCES users(uid)
);