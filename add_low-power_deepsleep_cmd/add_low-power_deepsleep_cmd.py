#import esptool 
import struct
import os,sys
import binascii

import string
import zlib

class ESPROM:

    # These are the currently known commands supported by the ROM
    ESP_FLASH_BEGIN = 0x02
    ESP_FLASH_DATA  = 0x03
    ESP_FLASH_END   = 0x04
    ESP_MEM_BEGIN   = 0x05
    ESP_MEM_END     = 0x06
    ESP_MEM_DATA    = 0x07
    ESP_SYNC        = 0x08
    ESP_WRITE_REG   = 0x09
    ESP_READ_REG    = 0x0a

    # Maximum block sized for RAM and Flash writes, respectively.
    ESP_RAM_BLOCK   = 0x1800
    ESP_FLASH_BLOCK = 0x400

    # Default baudrate used by the ROM. Don't know if it is possible to change.
    ESP_ROM_BAUD    = 115200

    # First byte of the application image
    ESP_IMAGE_MAGIC = 0xe9
    BIN_MAGIC_IROM   = 0xEA

    # Initial state for the checksum routine
    ESP_CHECKSUM_MAGIC = 0xef
    
    ESP_MAC = ""
    
    #download state
    ESP_DL_OK = 0x0
    ESP_DL_IDLE = 0x1
    ESP_DL_CONNECT_ERROR = 0x2
    ESP_DL_SYNC = 0x3
    ESP_DL_SYNC_ERROR = 0x4
    ESP_DL_ERASE = 0x5
    ESP_DL_ERASE_ERROR = 0x6
    ESP_DL_DOWNLOADING = 0x7
    ESP_DL_DOWNLOAD_ERROR = 0x8
    ESP_DL_FAIL = 0x9
    ESP_DL_FINISH = 0xA
    ESP_DL_STOP = 0xB
    

    def __init__(self, frame, port = 6, baudrate = 115200):
        self._COM = port
        self.ESP_ROM_BAUD = baudrate
        self.isOpen =  False
        self.stopFlg = False
        self.state = self.ESP_DL_IDLE
        self.process_num = 0
        self.MAC2 = 0
        self.MAC3 = 0
        self.MAC4 = 0
        self.MAC5 = 0
        self.frame = frame
        
        print "_COM: ",self._COM
        print "ESP_ROM_BAUD : ",self.ESP_ROM_BAUD
    @staticmethod
    def checksum(data, state = ESP_CHECKSUM_MAGIC):
        for b in data:
            state ^= ord(b)
        return state   

class ESPFirmwareImage:
    
    def __init__(self, filename = None, bin_type = None):
        self.segments = []
        self.entrypoint = 0
	self.byte2 = 0
	self.byte3 = 0
	self.tail = 0
	
	self.irom_segments = []
	self.irom_entrypoint = 0
	self.irom_user_byte2 = 0
	self.irom_user_byte3 = 0
	
	if bin_type == "flash.bin":
	    if filename is not None:
		print "test file in fwi: ",filename
		f = file(filename, 'rb')
		(magic, segments, self.byte2, self.byte3, self.entrypoint) = struct.unpack('<BBBBI', f.read(8))
		# some sanity check
		if magic != ESPROM.ESP_IMAGE_MAGIC or segments > 16:
		    raise Exception('Invalid firmware image;magic:%d'%magic)
		
		for i in xrange(segments):
		    (offset, size) = struct.unpack('<II', f.read(8))
		    if offset > 0x40200000 or offset < 0x3ffe0000 or size > 65536:
			raise Exception('Suspicious segment %x,%d' % (offset, size))
			#pass
		    self.segments.append((offset, size, f.read(size)))
    
		# Skip the padding. The checksum is stored in the last byte so that the
		# file is a multiple of 16 bytes.
		align = 15-(f.tell() % 16)
		f.seek(align, 1)
		self.checksum = ord(f.read(1))
		
	elif bin_type == "user.bin":
	    if filename is not None:
		print "test file in fwi: ",filename
		f = file(filename, 'rb')
		(magic, segments, self.irom_byte2, self.irom_byte3, self.irom_entrypoint) = struct.unpack('<BBBBI', f.read(8))
		
		# some sanity check
		if magic != ESPROM.BIN_MAGIC_IROM or segments > 16:
		    raise Exception('Invalid firmware image;magic:%d'%magic)
		
		#read irom block
		(offset, size) = struct.unpack('<II', f.read(8))
		if offset > 0x40200000:
		    raise Exception('Suspicious segment %x,%d' % (offset, size))
		self.irom_segments.append((offset, size, f.read(size)))
		
		#read falsh.bin block
		(magic, segments, self.byte2, self.byte3, self.entrypoint) = struct.unpack('<BBBBI', f.read(8))	
		
		for i in xrange(segments):
		    (offset, size) = struct.unpack('<II', f.read(8))
		    if (offset > 0x40200000 or offset < 0x3ffe0000 or size > 65536):
			raise Exception('Suspicious segment %x,%d' % (offset, size))
		    self.segments.append((offset, size, f.read(size)))
    
		# Skip the padding. The checksum is stored in the last byte so that the file is a multiple of 16 bytes.
		align = 15-(f.tell() % 16)
		f.seek(align, 1)
		self.checksum = ord(f.read(1))
	else:
	    print "error file name..."
	    

    def add_segment(self, filename, segment_list=[]):
	if "user" in filename:
	    len_list = len(segment_list)
	    #print "len_list",len_list
	    for i in range(len_list):
		(addr,data)=segment_list[len_list-1-i]
		self.segments = [(addr,len(data),data),] + self.segments
	    #new order Insert in the middle
	    self.segments = self.irom_segments + self.segments	

	elif "flash" in filename:
	    len_list = len(segment_list)
	    #print "len_list",len_list	    
	    for i in range(len_list):
		(addr,data)=segment_list[len_list-1-i]
		self.segments = [(addr,len(data),data),] + self.segments
	#print segments
	##for (addr,length,data) in self.segments:
	    ##print "addr:0x%08x ; len :0x%08x  %d kB"%(addr,length,length/1024)	

    def save(self, filename):
	if "flash" in filename:
	    f = file(filename, 'wb')
	    f.write(struct.pack('<BBBBI', ESPROM.ESP_IMAGE_MAGIC, len(self.segments), self.byte2, self.byte3, self.entrypoint))
    
	    checksum = ESPROM.ESP_CHECKSUM_MAGIC
	    for (offset, size, data) in self.segments:
		f.write(struct.pack('<II', offset, size))
		f.write(data)
		checksum = ESPROM.checksum(data, checksum)
    
	    align = 15-(f.tell() % 16)
	    f.seek(align, 1)
	    f.write(struct.pack('B', checksum))
	    f.close()
	    
	elif "user" in filename:
	    f = file(filename, 'wb')
	    #write irom.bin
	    f.write(struct.pack('<BBBBI', ESPROM.BIN_MAGIC_IROM, len(self.segments), self.irom_byte2, self.irom_byte3, self.irom_entrypoint))
	    (offset, size, data) = self.segments[0]
	    f.write(struct.pack('<II', offset, size))
	    f.write(data)	    
	    
	    #write flash.bin
	    f.write(struct.pack('<BBBBI', ESPROM.ESP_IMAGE_MAGIC, len(self.segments)-1, self.byte2, self.byte3, self.entrypoint))
	    checksum = ESPROM.ESP_CHECKSUM_MAGIC
	    count = 0
	    for (offset, size, data) in self.segments:
		if count == 0:
		    count = count + 1
		else:
		    f.write(struct.pack('<II', offset, size))
		    f.write(data)
		    checksum = ESPROM.checksum(data, checksum)
		
	    align = 15-(f.tell() % 16)
	    f.seek(align, 1)
	    f.write(struct.pack('B', checksum))
	    f.close()	 
	    
	    #get all bin crc byte
	    try: 
		blocksize = 1024 * 64 
		f = open(filename, 'rb')
		str = f.read(blocksize) 
		crc = 0 
		while(len(str) != 0): 
		    crc = binascii.crc32(str, crc) 
		    str = f.read(blocksize) 
		f.close() 
	    except: 
		print 'get file crc error!' 
		
	    all_bin_crc = crc
	    if all_bin_crc < 0:
		all_bin_crc = abs(all_bin_crc) - 1
	    else :
		all_bin_crc = abs(all_bin_crc) + 1
	    
	    #write crc 4 byte 
	    crc_check_data = chr((all_bin_crc & 0x000000FF))+chr((all_bin_crc & 0x0000FF00) >> 8)+chr((all_bin_crc & 0x00FF0000) >> 16)+chr((all_bin_crc & 0xFF000000) >> 24)
	    fp = open(filename,'ab')
	    if fp:
		fp.seek(0,2)
		fp.write(crc_check_data)
		fp.close()
	    else:
		print '%s write fail\n'%(file_name)	   
		    
	else:
	    print 'error file name '

    def add_all_crc(self, filename):
	all_bin_crc = getFileCRC(flash_bin_name)
	print all_bin_crc
	if all_bin_crc < 0:
	    all_bin_crc = abs(all_bin_crc) - 1
	else :
	    all_bin_crc = abs(all_bin_crc) + 1
	print all_bin_crc
	write_file(flash_bin_name,chr((all_bin_crc & 0x000000FF))+chr((all_bin_crc & 0x0000FF00) >> 8)+chr((all_bin_crc & 0x00FF0000) >> 16)+chr((all_bin_crc & 0xFF000000) >> 24))	
    
def add_instruction(bin_file='eagle.app.v6.flash.bin', bin_type="flash.bin", segment_list=[]):
    fw = ESPFirmwareImage(bin_file, bin_type) #eagle.app.v6.flash.bin
    #combine
    fw.add_segment(bin_file,segment_list)
	    
    new_name = bin_file[:-4]+'_low_power.bin'
    print "generated : "+new_name
    
    fw.save(new_name)   
    
    size_f1=os.path.getsize(bin_file)
    #print "size f1: ",size_f1
    size_f2=os.path.getsize(new_name)
    #print "size f2: ",size_f2
    if(size_f1>size_f2):
	f1=open(bin_file,'rb')
	f2=open(new_name,'ab')
	data_append = f1.read(size_f1)
	f2.write(data_append[size_f2:] )
	f1.close()
	f2.close()
    

def add_disable_rf():
    bin_file_name = ''
    try:
        bin_file_name = str(sys.argv[1])
	#bin_file_name = "./eagle.flash.bin"
        print "file name : ",bin_file_name
    except:
        print "bin path error"

    if not "user" in bin_file_name:
	try:
	    add_instruction(bin_file=bin_file_name, bin_type="flash.bin", 
		    segment_list=[(0x60000020,struct.pack('<i',0x0004001C)),
	                          (0x60000710,struct.pack('<i',0x00000000)),#disable rf 
	                          ])
	except:
	    print 'flash.bin add command error...' 
    else:
	try:
	    add_instruction(bin_file=bin_file_name, bin_type="user.bin", 
	            segment_list=[(0x60000020,struct.pack('<i',0x0004001C)),
				  (0x60000710,struct.pack('<i',0x00000000)),#disable rf 
				  ])    
	except:
	    print 'user.bin add command error...' 	
    
if __name__=="__main__":
    add_disable_rf()

