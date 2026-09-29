#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifndef sleep
#define sleep(sec) Sleep((DWORD)((sec) * 1000))
#endif
#ifndef usleep
#define usleep(usec) Sleep((DWORD)(((usec) + 999) / 1000))
#endif
#else
#include <unistd.h>
#endif

#if defined(__has_include)
  #if __has_include(<libusb-1.0/libusb.h>)
    #include <libusb-1.0/libusb.h>
  #else
    #include <libusb.h>
  #endif
#else
  #include <libusb.h>
#endif

#define USBD_VID            0x0416
#define USBD_PID            0x3F00

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

#ifdef _WIN32
// In Windows, BOOL, TRUE, and FALSE are defined in <windows.h>
#else
#define BOOL  unsigned char
#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif
#endif
#define PAGE_SIZE                      0x00000200     /* Page size */

#define PACKET_SIZE	64//32
#define FILE_BUFFER	128
unsigned char rcvbuf[PACKET_SIZE];
unsigned char sendbuf[PACKET_SIZE];
unsigned char aprom_buf[512];
unsigned int send_flag = FALSE;
unsigned int recv_flag = FALSE;
unsigned int g_packno = 1;
unsigned short gcksum;

unsigned short Checksum(unsigned char *buf, unsigned int len);
void WordsCpy(void *dest, void *src, unsigned int size);
BOOL SendData(void);
BOOL RcvData(void);
BOOL CmdSyncPackno(void);
BOOL CmdGetCheckSum(int flag, int start, int len, unsigned short *cksum);
BOOL CmdGetDeviceID(unsigned int *devid);
BOOL CmdGetConfig( unsigned int *config);
BOOL CmdRunCmd(unsigned int cmd, unsigned int *data);
BOOL CmdUpdateAprom(char *filename);

#define dbg_printf printf
static inline unsigned int inpw(const void *addr)
{
	unsigned int val = 0;
	memcpy(&val, addr, sizeof(val));
	return val;
}



static libusb_context *g_ctx = NULL;
static libusb_device_handle *udev = NULL;

libusb_device_handle *usbio_probe(libusb_context *ctx, unsigned short target_vid, unsigned short target_pid)
{
	libusb_device **devs = NULL;
	ssize_t cnt = libusb_get_device_list(ctx, &devs);
	if (cnt < 0) {
		fprintf(stderr, "libusb_get_device_list error %d\n", (int)cnt);
		return NULL;
	}

	libusb_device *found_dev = NULL;
	for (ssize_t i = 0; i < cnt; i++) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(devs[i], &desc);
		if (r < 0) continue;
		printf("Vendor/Product ID: %04x:%04x\n",
			desc.idVendor,
			desc.idProduct);
		if ((desc.idVendor == target_vid) && (desc.idProduct == target_pid || (target_pid == USBD_PID && desc.idProduct == 0xa317))) {
			found_dev = devs[i];
			break;
		}
	}

	libusb_device_handle *handle = NULL;
	if (found_dev != NULL) {
		int r = libusb_open(found_dev, &handle);
		if (r != 0) {
			fprintf(stderr, "libusb_open error %d (%s)\n", r, libusb_error_name(r));
			handle = NULL;
		}
	}

	libusb_free_device_list(devs, 1);
	return handle;
}

int main(int argc, char *argv[])
{	
	setvbuf(stdout, NULL, _IONBF, 0);
	clock_t start_time, end_time;
	float total_time = 0;
	int r = 1;
	unsigned short target_vid = USBD_VID;
	unsigned short target_pid = USBD_PID;
	const char *bin_filename = NULL;
	BOOL jump_aprom = FALSE;
	int pos_arg = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--jumpAP") == 0) {
			jump_aprom = TRUE;
		} else {
			if (pos_arg == 0) {
				bin_filename = argv[i];
				pos_arg++;
			} else if (pos_arg == 1) {
				target_pid = (unsigned short)strtoul(argv[i], NULL, 16);
				pos_arg++;
			} else if (pos_arg == 2) {
				target_vid = (unsigned short)strtoul(argv[i], NULL, 16);
				pos_arg++;
			}
		}
	}

	if (bin_filename == NULL) {
		printf("Usage: %s <firmware_file.bin> [--jumpAP] [PID_HEX] [VID_HEX]\n", argv[0]);
		printf("Options:\n");
		printf("  --jumpAP       Jump to APROM and reboot MCU after successful programming\n");
		printf("Default Target: VID=0x%04X, PID=0x%04X\n", USBD_VID, USBD_PID);
		return -1;
	}

	printf("Target device VID: 0x%04X, PID: 0x%04X\n", target_vid, target_pid);
	if (jump_aprom) {
		printf("Option --jumpAP enabled: Will jump to APROM after programming.\n");
	}

	start_time = clock(); /* microsecond / clock ticks */

	r = libusb_init(&g_ctx);
	if (r < 0) {
		fprintf(stderr, "libusb_init error %d (%s)\n", r, libusb_error_name(r));
		return -1;
	}

	udev = usbio_probe(g_ctx, target_vid, target_pid);
	if (udev == NULL) {
		printf("USB IO Card not found.\n");
		libusb_exit(g_ctx);
		return -1;
	}

#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000102)
	libusb_set_auto_detach_kernel_driver(udev, 1);
#elif !defined(_WIN32)
	if (libusb_kernel_driver_active(udev, 0) == 1) {
		r = libusb_detach_kernel_driver(udev, 0);
		printf("libusb_detach_kernel_driver: ret %d\n", r);
	}
#endif

	int current_config = 0;
	libusb_get_configuration(udev, &current_config);
	if (current_config != 1) {
		libusb_set_configuration(udev, 1);
	}

	r = libusb_claim_interface(udev, 0);
	if (r < 0) {
		fprintf(stderr, "libusb_claim_interface error %d (%s)\n", r, libusb_error_name(r));
		libusb_close(udev);
		libusb_exit(g_ctx);
		return -1;
	}
	printf("Successfully claimed interface\n");

	if (CmdUpdateAprom((char *)bin_filename) == TRUE)
	{
		printf("Process=%.2f \r", 100.0); // Print progress information
		printf("programmer pass\n\r");	  // Print success message for programming
		if (jump_aprom) {
			printf("Jumping to APROM...\n\r");
			CmdRunCmd(CMD_RUN_APROM, NULL);
		}
	}
	else
	{
		printf("programmer false\n\r"); // Print error message for programming failure
	}	

	libusb_release_interface(udev, 0);
	libusb_close(udev);
	libusb_exit(g_ctx);

	end_time = clock();
/* CLOCKS_PER_SEC is defined at time.h */
	total_time = (float)(end_time - start_time) / CLOCKS_PER_SEC;

	printf("Time : %f sec \n", total_time);
	return 0;
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

unsigned short Checksum(unsigned char *buf, unsigned int len)
{
	unsigned int i;
	unsigned short c = 0;

	for (i = 0; i < len; i++) {
		c += buf[i];
	}
	return (c);
}

BOOL SendData(void)
{
	gcksum = Checksum(sendbuf, PACKET_SIZE);

	int transferred = 0;
	int r = libusb_interrupt_transfer(udev, 0x02, sendbuf, PACKET_SIZE, &transferred, 10000);
	if (r != 0) {
		dbg_printf("SendData interrupt transfer error: %d (%s)\n", r, libusb_error_name(r));
		return FALSE;
	}

	return TRUE;
}

BOOL RcvData(void)
{
	BOOL Result;
	unsigned short lcksum;
	unsigned char *pBuf;

	int transferred = 0;
	int r = libusb_interrupt_transfer(udev, 0x81, rcvbuf, PACKET_SIZE, &transferred, 15000);
	if (r != 0) {
		dbg_printf("RcvData interrupt transfer error: %d (%s)\n", r, libusb_error_name(r));
		return FALSE;
	}

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
	BOOL Result = TRUE;
	unsigned int cmdData;
	
	//sync send&recv packno
	memset(sendbuf, 0, PACKET_SIZE);
	cmdData = cmd;
	WordsCpy(sendbuf + 0, &cmdData, 4);
	WordsCpy(sendbuf + 4, &g_packno, 4);
	if (cmd == CMD_WRITE_CHECKSUM && data != NULL)
	{
		WordsCpy(sendbuf + 8, &data[0], 4);
		WordsCpy(sendbuf + 12, &data[1], 4);
	}
	g_packno++;
	
	if (!SendData()) {
		return FALSE;
	}

	if ((cmd == CMD_ERASE_ALL) || (cmd == CMD_GET_FLASHMODE) 
			|| (cmd == CMD_WRITE_CHECKSUM))
	{
		Result = RcvData();
		if (Result)
		{
			if (cmd == CMD_GET_FLASHMODE && data != NULL)
			{
				WordsCpy(&cmdData, rcvbuf + 8, 4);
				*data = cmdData;
			}
		}
	}
	else if ((cmd == CMD_RUN_APROM) || (cmd == CMD_RUN_LDROM)
		|| (cmd == CMD_RESET))
	{
#ifdef _WIN32
		Sleep(100);
#else
		usleep(100000);
#endif
		Result = TRUE;
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
	FILE *fp = NULL;		//taget bin file pointer	

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
	if (fp != NULL)
	{
		fclose(fp);
		fp = NULL;
	}
	return Result;
	
}
