all: AdrienReceiverDomo.cpp 
	g++ -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s -Wall -I../ -lrf24-bcm -lmosquitto AdrienReceiverDomo.cpp -o AdrienReceiverDomo -Wno-write-strings

clean: 
	$(RM) myprog


#sudo  ln -s /home/plessisa/AdrienReceiver/AdrienReceiverDomo /etc/init.d/
