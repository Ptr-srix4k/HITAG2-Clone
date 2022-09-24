import binascii

file = open("EEPROM.hex", "rb")

id_code = "D35B9E6D"
first_sniff = ""
second_sniff = ""

for x in range(4):
    byte = file.read(1)
    byte_st = binascii.hexlify(byte)
    byte_st = byte_st.decode('utf-8')
    first_sniff = byte_st + first_sniff 

for x in range(4):
    byte = file.read(1)
    byte_st = binascii.hexlify(byte)
    byte_st = byte_st.decode('utf-8')
    first_sniff = byte_st + first_sniff  
    
for x in range(4):
    byte = file.read(1)
    byte_st = binascii.hexlify(byte)
    byte_st = byte_st.decode('utf-8')
    second_sniff = byte_st + second_sniff 

for x in range(4):
    byte = file.read(1)
    byte_st = binascii.hexlify(byte)
    byte_st = byte_st.decode('utf-8')
    second_sniff = byte_st + second_sniff  
 
cmd = "./ht2crack5 " + id_code + " " + first_sniff[0:8] + " " + first_sniff[8:16] + " " + second_sniff[0:8] + " " + second_sniff[8:16]

print () 
print ("First  sniff = " + first_sniff) 
print ("Second sniff = " + second_sniff) 
print () 
print ("Command to use :") 
print () 
print (cmd)
print () 

file.close()
