//#include <iostream>
#include <stdio.h>
//using namespace std;
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include "usb.h"
#include <errno.h>

#define CMD_UPDATE_APROM	0x000000A0
#define CMD_UPDATE_CONFIG	0x000000A1
#define CMD_READ_CONFIG		0x000000A2
#define CMD_ERASE_ALL		0x000000A3
#define CMD_SYNC_PACKNO		0x000000A4
#define CMD_GET_FWVER		0x000000A6
#define CMD_APROM_SIZE		0x000000AA
#define CMD_RUN_APROM		0x000000AB
#define CMD_RUN_LDROM		0x000000AC
#define CMD_RESET			0x000000AD

#define CMD_GET_DEVICEID	0x000000B1

#define CMD_PROGRAM_WOERASE 	0x000000C2
#define CMD_PROGRAM_WERASE 	 	0x000000C3
#define CMD_READ_CHECKSUM 	 	0x000000C8
#define CMD_WRITE_CHECKSUM 	 	0x000000C9
#define CMD_GET_FLASHMODE 	 	0x000000CA

#define APROM_MODE	1
#define LDROM_MODE	2

#define BOOL  unsigned char
#define PAGE_SIZE                      0x00000200     /* Page size */

#define PACKET_SIZE	64//32
#define FILE_BUFFER	128
#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif
unsigned char rcvbuf[PACKET_SIZE];
unsigned char sendbuf[PACKET_SIZE];
unsigned char aprom_buf[512];
unsigned int send_flag = FALSE;
unsigned int recv_flag = FALSE;
unsigned int g_packno = 1;
unsigned short gcksum;

unsigned short Checksum(unsigned char *buf, unsigned int len);
void WordsCpy(void *dest, void *src, unsigned int size);
BOOL CmdSyncPackno(void);
BOOL CmdGetCheckSum(int flag, int start, int len, unsigned short *cksum);
BOOL CmdGetDeviceID(unsigned int *devid);
BOOL CmdGetConfig( unsigned int *config);
BOOL CmdUpdateAprom(char *filename);

#define dbg_printf printf
#define inpw(addr)            (*(unsigned int *)(addr))



struct usb_device *usbio_probe()
{
	struct usb_bus *busses, *bus;
	usb_init();
	usb_find_busses();
	usb_find_devices();
	busses = usb_get_busses();
	for (bus = busses; bus; bus = bus->next) {
 	
		struct usb_device *dev;
 	
		for (dev = bus->devices; dev; dev = dev->next) {
			struct usb_device_descriptor *desc;
			desc = &(dev->descriptor);
			printf("Vendor/Product ID: %04x:%04x\n",
				desc->idVendor,
				desc->idProduct);
			if ((desc->idVendor == (0x0416)) && (desc->idProduct == (0xa317))) 
			{
				return dev;
			}
		}
	}
	return NULL;
}

usb_dev_handle *udev;
int main(int argc, char *argv[])
{	
	clock_t start_time, end_time;
	float total_time = 0;
	start_time = clock(); /* mircosecond */
	char szdata[64];
	struct usb_device *dev;
	struct usb_device_descriptor *desc;
	
	int r = 1, i = 0;
  	
	dev = usbio_probe();
	desc = &(dev->descriptor);
	if (dev == NULL) {
		printf("USB IO Card not found.\n");
		return -1;
	}
	//Open USB port
	udev = usb_open(dev);
	r = usb_detach_kernel_driver_np(udev, 0);
	printf("usb_detach_kernel_driver_np: ret %d\n", r);
	r = usb_set_configuration(udev, dev->descriptor.bNumConfigurations);
	if (r < 0) {
		fprintf(stderr, "libusb_set_configuration error %d\n", r);
		return -1;
	}
	
	// sudo chmod o+w /dev/bus/usb/001/003 for linux
	r = usb_claim_interface(udev, 0);
	if (r < 0) {
		fprintf(stderr, "libusb_claim_interface error %d\n", r);
		return -1;
	}
	printf("Successfully claimed interface\n");
	
	if (CmdUpdateAprom(argv[1]) == TRUE)
	{
		printf("Process=%.2f \r", 100.0); // Print progress information
		printf("programmer pass\n\r");	  // Print success message for programming
	}
	else
	{
		printf("programmer flase\n\r"); // Print error message for programming failure
	}	
	usb_close(udev);
	end_time = clock();
/* CLOCKS_PER_SEC is defined at time.h */
	total_time = (float)(end_time - start_time) / CLOCKS_PER_SEC;

	printf("Time : %f sec \n", total_time);
}



void WordsCpy(void *dest, void *src, unsigned int size)
{
	unsigned char *pu8Src, *pu8Dest;
	unsigned int i;
    
	pu8Dest = (unsigned char *)dest;
	pu8Src  = (unsigned char *)src;
    
	for (i = 0; i < size; i++)
		pu8Dest[i] = pu8Src[i]; 
}

unsigned short Checksum(unsigned char *buf, int len)
{
	int i;
	unsigned short c;

	for (c = 0, i = 0; i < len; i++) {
		c += buf[i];
	}
	return (c);
}

BOOL SendData(void)
{


	gcksum = Checksum(sendbuf, PACKET_SIZE);

	usb_interrupt_write(udev, 0x02, (char*)sendbuf, PACKET_SIZE, 10000);//send

	return TRUE;
}

BOOL RcvData(void)
{
	BOOL Result;
	unsigned short lcksum, i;
	unsigned char *pBuf;
	usb_interrupt_read(udev, 0x81, (char*)rcvbuf, PACKET_SIZE, 10000);//get

	
	pBuf = rcvbuf;
	WordsCpy(&lcksum, pBuf, 2);
	pBuf += 4;

	if (inpw(pBuf) != g_packno)
	{
		dbg_printf("g_packno=%d rcv %d\n", g_packno, inpw(pBuf));
		Result = FALSE;
	}
	else
	{
		if (lcksum != gcksum)
		{
			dbg_printf("gcksum=%x lcksum=%x\n", gcksum, lcksum);
			Result = FALSE;
		}
		g_packno++;
		Result = TRUE;
	}
	return Result;
}

BOOL CmdSyncPackno(void)
{
	BOOL Result;
	unsigned long cmdData;
	
	//sync send&recv packno
	memset(sendbuf, 0, PACKET_SIZE);
	cmdData = CMD_SYNC_PACKNO;//CMD_UPDATE_APROM
	WordsCpy(sendbuf + 0, &cmdData, 4);
	WordsCpy(sendbuf + 4, &g_packno, 4);
	WordsCpy(sendbuf + 8, &g_packno, 4);
	g_packno++;
	
	SendData();
	Result = RcvData();
	
	return Result;
}

BOOL CmdFWVersion(unsigned int *fwver)
{
	BOOL Result;
	unsigned long cmdData;
	unsigned int lfwver;
	
	//sync send&recv packno
	memset(sendbuf, 0, PACKET_SIZE);
	cmdData = CMD_GET_FWVER;
	WordsCpy(sendbuf + 0, &cmdData, 4);
	WordsCpy(sendbuf + 4, &g_packno, 4);
	g_packno++;
	
	SendData();


	Result = RcvData();
	if (Result)
	{
		WordsCpy(&lfwver, rcvbuf + 8, 4);
		*fwver = lfwver;
	}
	
	return Result;
}


BOOL CmdGetDeviceID(unsigned int *devid)
{
	BOOL Result;
	unsigned long cmdData;
	unsigned int ldevid;
	
	//sync send&recv packno
	memset(sendbuf, 0, PACKET_SIZE);
	cmdData = CMD_GET_DEVICEID;
	WordsCpy(sendbuf + 0, &cmdData, 4);
	WordsCpy(sendbuf + 4, &g_packno, 4);
	g_packno++;
	
	SendData();
	Result = RcvData();
	if (Result)
	{
		WordsCpy(&ldevid, rcvbuf + 8, 4);
		*devid = ldevid;
	}
	
	return Result;
}

BOOL CmdGetConfig(unsigned int *config)
{
	BOOL Result;
	unsigned long cmdData;
	unsigned int lconfig[2];
	
	//sync send&recv packno
	memset(sendbuf, 0, PACKET_SIZE);
	cmdData = CMD_READ_CONFIG;
	WordsCpy(sendbuf + 0, &cmdData, 4);
	WordsCpy(sendbuf + 4, &g_packno, 4);
	g_packno++;
	
	SendData();


	Result = RcvData();
	if (Result)
	{
		WordsCpy(&lconfig[0], rcvbuf + 8, 4);
		WordsCpy(&lconfig[1], rcvbuf + 12, 4);
		config[0] = lconfig[0];
		config[1] = lconfig[1];
	}
	
	return Result;
}

//uint32_t def_config[2] = {0xFFFFFF7F, 0x0001F000};
//CmdUpdateConfig(FALSE, def_config)
BOOL CmdUpdateConfig(unsigned int *conf)
{
	BOOL Result;
	unsigned long cmdData;
	
	//sync send&recv packno
	memset(sendbuf, 0, PACKET_SIZE);
	cmdData = CMD_UPDATE_CONFIG;
	WordsCpy(sendbuf + 0, &cmdData, 4);
	WordsCpy(sendbuf + 4, &g_packno, 4);
	WordsCpy(sendbuf + 8, conf, 8);
	g_packno++;
	
	SendData();
	Result = RcvData();
	
	return Result;
}

//for the commands
//CMD_RUN_APROM
//CMD_RUN_LDROM
//CMD_RESET
//CMD_ERASE_ALL
//CMD_GET_FLASHMODE
//CMD_WRITE_CHECKSUM
BOOL CmdRunCmd(unsigned int cmd, unsigned int *data)
{
	BOOL Result;
	unsigned int cmdData, i;
	
	//sync send&recv packno
	memset(sendbuf, 0, PACKET_SIZE);
	cmdData = cmd;
	WordsCpy(sendbuf + 0, &cmdData, 4);
	WordsCpy(sendbuf + 4, &g_packno, 4);
	if (cmd == CMD_WRITE_CHECKSUM)
	{
		WordsCpy(sendbuf + 8, &data[0], 4);
		WordsCpy(sendbuf + 12, &data[1], 4);
	}
	g_packno++;
	
	SendData();
	if ((cmd == CMD_ERASE_ALL) || (cmd == CMD_GET_FLASHMODE) 
			|| (cmd == CMD_WRITE_CHECKSUM))
	{
		Result = RcvData();
		if (Result)
		{
			if (cmd == CMD_GET_FLASHMODE)
			{
				WordsCpy(&cmdData, rcvbuf + 8, 4);
				*data = cmdData;
			}
		}
		
	}
	else if ((cmd == CMD_RUN_APROM) || (cmd == CMD_RUN_LDROM)
		|| (cmd == CMD_RESET))
	{
		sleep(500);
	}
	return Result;
}
unsigned int file_totallen;
unsigned int file_checksum;

//the ISP flow, show to update the APROM in target chip 
BOOL CmdUpdateAprom(char *filename)
{
	BOOL Result=TRUE;
	unsigned int devid, config[2], i, mode, j;
	unsigned long cmdData, startaddr;
	unsigned short get_cksum;
	unsigned char Buff[256];
	unsigned int s1;
	FILE *fp;		//taget bin file pointer	

	g_packno = 1;
	
	//synchronize packet number with ISP. 
	Result = CmdSyncPackno();
	if (Result == FALSE)
	{
		dbg_printf("send Sync Packno cmd fail\n");
		goto out1;
	}
	
	//This command is used to get boot selection (BS) bit. 
	//If boot selection is APROM, the mode of returned is equal to 1,
	//Otherwise, if boot selection is LDROM, the mode of returned is equal to 2. 
#if 0
	Result = CmdRunCmd(CMD_GET_FLASHMODE, &mode);
	if (mode != LDROM_MODE)
	{
		dbg_printf("fail\n");
		goto out1;
	}
	else
	{
		dbg_printf("ok\n");
	}
#endif
	//get product ID 
	CmdGetDeviceID(&devid);
	printf("DeviceID: 0x%x\n", devid);
	
	//get config bit
	CmdGetConfig(config);
	dbg_printf("config0: 0x%x\n", config[0]);
	dbg_printf("config1: 0x%x\n", config[1]);
	printf("%s\n\r", filename);
	//open bin file for APROM
	//if ((fp = fopen("//home/jc/test.bin", "rb")) == NULL)
	//sudo chmod 777 ./test.bin 
	if ((fp = fopen(filename, "rb")) == NULL)
	{
		printf("APROM FILE OPEN FALSE\n\r");
		Result = FALSE;
		goto out1;
	}	
	//get file size
	fseek(fp, 0, SEEK_END);
	file_totallen = ftell(fp);
	fseek(fp, 0, SEEK_SET);

    //first isp package
	memset(sendbuf, 0, PACKET_SIZE);
	cmdData = CMD_UPDATE_APROM;			//CMD_UPDATE_APROM Command
	WordsCpy(sendbuf + 0, &cmdData, 4);
	WordsCpy(sendbuf + 4, &g_packno, 4);
	g_packno++;
    
	//start address
	startaddr = 0;
	WordsCpy(sendbuf + 8, &startaddr, 4);
	WordsCpy(sendbuf + 12, &file_totallen, 4);
	
	fread(&sendbuf[16], sizeof(char), 48, fp);
	
	//send CMD_UPDATE_APROM
	SendData();
	printf("erase chip ...\n\r");
	//for erase time delay using, other bus need it.
	sleep(3);
	Result = RcvData();
	if (Result == FALSE)
		goto out1;
	
	//Send other BIN file data in ISP package
	for (i = 48; i < file_totallen; i = i + 56)
	{

	
		//dbg_printf("i=%d \n\r", i);
		printf("Process=%.2f%% \r", (float)((float)i / (float)file_totallen) * 100);
				//clear buffer
		for (j = 0; j < 64; j++)
		{
			sendbuf[j] = 0;
		}
		//WordsCpy(sendbuf+0, &cmdData, 4);
		WordsCpy(sendbuf + 4, &g_packno, 4);
		g_packno++;
		if ((file_totallen - i) > 56)
		{			
			//f_read(&file1, &sendbuf[8], 56, &s1);
			fread(&sendbuf[8], sizeof(char), 56, fp);
			//read check  package
			SendData();
			usleep(50000);
			Result = RcvData();
			if (Result == FALSE)
				goto out1;			
		}
		else
		{
			//f_read(&file1, &sendbuf[8], file_totallen - i, &s1);
			fread(&sendbuf[8], sizeof(char), file_totallen - i, fp);
			  //read target chip checksum
			SendData();
			usleep(50000);
			Result = RcvData();
			if (Result == FALSE)			
				goto out1;	
#if 0
			WordsCpy(&get_cksum, rcvbuf + 8, 2);
			if ((file_checksum & 0xffff) != get_cksum)	
			{			 
				Result = FALSE;
				goto out1;
			}
#endif
		}
	}

out1:
	return Result;
	
}
