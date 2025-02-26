from machine import Pin, SPI
import mfrc522

# Initialize SPI and RFID reader
sck = 18
mosi = 23
miso = 19
rst = 4
cs = 5

rdr = mfrc522.MFRC522(sck, mosi, miso, rst, cs)

print("Place card near the reader")

try:
    while True:
        (stat, tag_type) = rdr.request(rdr.REQIDL)
        
        if stat == rdr.OK:
            (stat, raw_uid) = rdr.anticoll()
            
            if stat == rdr.OK:
                print("Card detected")
                print("UID:", [hex(i) for i in raw_uid])
                
except KeyboardInterrupt:
    print("Bye")