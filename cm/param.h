// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(PARAM_H_20090415)
#define PARAM_H_20090415

#define MAGIC					0x720F
#define MAP_VERSION_V1		0x01
#define MAP_VERSION_V2		0x02

#define WB_INFO_BLOCK_OFFSET	0x400
#define WB_BOARD_BLOCK_OFFSET	0x440
#define WB_RFCAL_BLOCK_OFFSET	0x470


struct wb_cfg_header {
	unsigned short	magic;		// 0x720F
	unsigned char	version;
	unsigned char	crc;
} __attribute__((packed));

#define WB_INFO_BLOCK_OFFSET_V2			0x400
#define WB_BOARD_BLOCK_OFFSET_V2		0x4A0
#define WB_RFCAL_BLOCK_OFFSET_V2		0x4E0
#define WB_TEMP_BLOCK1_OFFSET_V2		0x6F0
#define WB_TEMP_BLOCK2_OFFSET_V2		0xB70
#define WB_WIDE_SCAN_BLOCK_OFFSET_V2	0xFA0
#define WB_OMA_DMT_BLOCK_OFFSET_V2		0x1500

#define WB_DEV_CERT_BLOCK_OFFSET_V2		0x2500
#define WB_CERT1_UP_BLOCK_OFFSET_V2		0x3500
#define WB_CERT1_LO_BLOCK_OFFSET_V2		0x3D00
#define WB_VEND_BLOCK_OFFSET_V2			0x5000
#define WB_CERT2_BLOCK_OFFSET_V2		0x5400
#define WB_EAP_BLOCK_OFFSET_V2			0x5C00
#define WB_CERT3_BLOCK_OFFSET_V2		0x8400

#define WB_OMA_DMT_BLOCK_SIZE			0x1000
#define WB_MAX_CERT_BLOCK_SIZE			WB_CERT3_BLOCK_SIZE
#define WB_DEV_CERT_BLOCK_SIZE			0x1000
#define WB_CERT1_UP_BLOCK_SIZE			0x0800
#define WB_CERT1_LO_BLOCK_SIZE			0x0800
#define WB_VEND_BLOCK_SIZE				0x0400
#define WB_CERT2_BLOCK_SIZE				0x0800
#define WB_EAPP_BLOCK_SIZE				0x0800
#define WB_CERT3_BLOCK_SIZE				0x5000

/*
 * struct wb_config_block_v2 defines the WiBro configuration parameters
 * stored in either EEPROM or NOR Flash.
 */
struct wb_info_block_v2 {
	unsigned short	magic;		// 0x720F
	unsigned char	version;	// 2
	unsigned char	crc;

	unsigned char	mac_address[6];
	
	unsigned char	band_sel_code;
	
	unsigned char	fa_list_size;	// number of FA list
	unsigned int	fa_list[16];		// frequency in kHz
	unsigned char 	IMEI[8];
	unsigned char	PID[12];			// Product ID
	unsigned char	ATR_lock;	// ATR lock/unlock
					// 0: default (KT lock0
					// 1: Add service provider here
					// 0xFF : unlock
	unsigned short	freq_bitmap;	// Frequency bitmap
	unsigned char	BS_vendor;		// For BS-vendor specific setting
	unsigned short	USB_VID;
	unsigned short	USB_PID;
	unsigned char	eeprom_boot_delay;
	unsigned char	reserved[39];
	char            Serial[16];     // Product serial number (especially for USB serial number);
} __attribute__((packed));


/* struct wb_board_config_block defines the board specific
 * configurations.
 */
struct wb_board_config_block_v2 {
	unsigned short	magic;		// 0x720F
	unsigned char	version;	// 2
	unsigned char	crc;		// computed over the following bytes

	unsigned int	rf_ctrl_polarity0;
	unsigned int	rf_ctrl_polarity1;
	
	char 			tx_dynamic_corr_fact;	// Tx dynamic correction factor
	char	mt_ofs1;	// for manual tuning
	char 	mt_ofs2;

	char	uicc_level_trans;   // For internal UICC, level translator is controlled by this setting
	/* Ethan2007.03.19 - Add 16-bytes for LED control */
	char			reserved[12];
	unsigned short	leds[8];

	char			reserved1[20];
} __attribute__((packed));


#define CALTBL_NUM	16

/*
 * struct wb_rf_calibr_block defines the WiBro RF calibration
 * information stored in either EEPROM or NOR Flash.
 */
struct wb_rf_calibr_block_v2 {
	unsigned short	magic;		// 0x720F
	unsigned char	version;	// 2
	unsigned char	crc;		// computed over the following bytes

	/* Common fields */
	char			valid;
	char			dcc_offset_i;
	char			dcc_offset_q;
	char			rx_i_imb;
	char			rx_q_imb;
	unsigned short	pags_th;
	unsigned char	pags_gpio;
	char 			max_tpwr;
	char			c_gs_par;
	char			reserved[2];

	/* Per channel data */
	struct {
		unsigned 	valid: 8;	// Bit0: Cal, Bit1: DCC-OFS, Bit2: DCC-VAL
		unsigned 	freq: 24;		// kHz
		char		rssi_ofs0[3];
		char		rssi_ofs1[3];
		unsigned char tc_rssi;		// Temp value measured while RSSI cal
		char		cinr_ofs[2];	// CINR ofs for main/diversity
		unsigned char tpwr_valid;	// power_ofs is applied when this flas is 1
		unsigned char tc_tpwr;		// Temp value measured while TX power cal
		char		power_ofs[10];	// for 10-point tx power cal
		char		gs_par1;
		char		gs_par2;
		char		gs_th;
		char		mt_ofs1;
		char		mt_ofs2;
		char		reserved[2];
	} __attribute__((packed)) cal_tbl[CALTBL_NUM];
} __attribute__((packed));

/*
 * struct wb_temp_block_v2 defines the WiBro RF calibration
 * information stored in either EEPROM or NOR Flash.
 */
struct wb_temp_block_v2 {
	unsigned short	magic;		// 0x720F
	unsigned char	version;	// 2
	unsigned char	crc;		// computed over the following bytes

	/* Common fields */
	char			valid;
	unsigned char	ofs23_adc_idx[47];	// 43-point thermistor value per temperature for Tx power temperature compensation
	char			reserved[1];
	unsigned char 	rssi_adc_idx[11];	// 11-point thermistor value per temperature for RSSI temperature compensation
	char 			reserved1[16];
	char			rssi_cf[3][11];
	char			reserved2[15];

	/* Per channel data */
	struct {
		unsigned 	valid: 8;	// Bit0: Cal, Bit1: DCC-OFS, Bit2: DCC-VAL
		unsigned 	freq: 24;		// kHz
		char		ofs23_cf[47];
		char		reserved[1];
		char cpara_cf[9][7];
		char		reserved1[13];
	} __attribute__((packed)) temp_tbl[CALTBL_NUM];
} __attribute__((packed));

struct wb_temp_common_block_v2 {
	unsigned short	magic;		// 0x720F
	unsigned char	version;	// 2
	unsigned char	crc;		// computed over the following bytes

	/* Common fields */
	char			valid;
	unsigned char	ofs23_adc_idx[47];	// 43-point thermistor value per temperature for Tx power temperature compensation
	char			reserved[1];
	unsigned char 	rssi_adc_idx[11];	// 11-point thermistor value per temperature for RSSI temperature compensation
	char 			reserved1[16];
	char			rssi_cf[3][11];
	char			reserved2[15];
	char			fa[0];
} __attribute__((packed));

#define MAX_WIDE_SCAN_CHAN_PLAN		4

/* Channel Plan for Wide Scan */

struct wide_cp {
	unsigned int	valid:8;
	unsigned int	first_freq:24;
	unsigned int	bw:8;
	unsigned int	last_freq:24;
	unsigned int	freq_step:16;
	unsigned int	dummy:16;
} __attribute__((packed));

/*
 * struct wb_wide_scan_block defines the board specific
 * configurations.
 */
struct wb_wide_scan_block_v2 {
	unsigned short	magic;		// 0x720F
	unsigned char	version;	// 2
	unsigned char	crc;		// computed over the following bytes

	struct wide_cp	cp[MAX_WIDE_SCAN_CHAN_PLAN];
} __attribute__((packed));


#define MAX_OMA_DATA_SIZE	(0x1000 - 8)
struct wb_oma_block_v2 {
	unsigned short	magic;		// 0x720F
	unsigned char	version;	// 2
	unsigned char	crc;		// computed over the following bytes

	unsigned int	length;
	unsigned char	c_tree[MAX_OMA_DATA_SIZE];	// Compressed OMA Tree image
} __attribute__((packed));

struct wb_temp_fa_block_v2 {
	unsigned 	valid: 8;	// Bit0: Cal, Bit1: DCC-OFS, Bit2: DCC-VAL
	unsigned 	freq: 24;		// kHz
	char		ofs23_cf[47];
	char		reserved[1];
	char 		cpara_cf[9][7];
	char		reserved1[13];
} __attribute__((packed));

struct wb_temp_block1_v2 {
	struct wb_temp_common_block_v2 common;
	struct wb_temp_fa_block_v2 fa[8];
} __attribute__((packed));

struct wb_temp_block2_v2 {
	struct wb_temp_fa_block_v2 fa[8];
} __attribute__((packed));

#endif
