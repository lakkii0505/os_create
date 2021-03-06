#include "bootpack.h"
FIFO8 mousefifo;

/* ps/2マウスからの割り込み */
void inthandler2c(int *esp){
    unsigned char data;
    io_out8(PIC1_OCW2, 0x64);
    io_out8(PIC0_OCW2, 0x62);
    data = io_in8(PORT_KEYDAT);
    fifo8_put(&mousefifo, data);
    return;
}

void enable_mouse(MOUSE_DEC *mdec){
	/* マウス有効 */
	wait_KBC_sendReady();
	io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
	wait_KBC_sendReady();
	io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
	mdec->phase = 0;
	return;
}

int mouse_decode(BOOTINFO *binfo, MOUSE_DEC *mdec, unsigned char data){
	char s[40];
	sprintf(s,"hoge");
	if( mdec->phase == 0 ){
		/* mouseの1byteを待機 */
		if(data == 0xfa){
			mdec->phase = 1;
		}
		return 0;
	}else if( mdec->phase == 1 ){
		if((data & 0xc8) == 0x08){
			mdec->buf[0] = data;
			mdec->phase = 2;
		}
		return 0;
	}else if( mdec->phase == 2 ){
		mdec->buf[1] = data;
		mdec->phase = 3;
		return 0;
	}else if( mdec->phase == 3 ){
		mdec->buf[2]  = data;
		mdec->phase = 1;
		mdec->btn = mdec->buf[0] & 0x07;
		mdec->x = mdec->buf[1];
		mdec->y = mdec->buf[2];
		if((mdec->buf[0] & 0x10) !=0 ){
			mdec->x |= 0xffffff00;
		}
		if((mdec->buf[0] & 0x20) !=0 ){
			mdec->y |= 0xffffff00;
		}
		mdec->y = - mdec->y;
		
		putfonts8_asc(binfo->vram,binfo->scrnx, 32,48,COL8_000084,s);
		return 1;
	}
	return -1;
}
