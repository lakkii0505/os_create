#include <stdio.h>
#include "bootpack.h"

FIFO32 fifo;
//extern FIFO8 keyfifo, mousefifo;
extern TIMERCTL timerctl;
//FIFO8 timerfifo;
BOOTINFO *binfo= (BOOTINFO *) ADR_BOOTINFO;
// extern MOUSE_DEC mdec; externしてはいけない(未解決)

void HariMain(void)
{
	int fifobuf[128];	
	char s[40], keybuf[32], mousebuf[128], timerbuf[8];
	int mx, my, i, count10;
	unsigned int memtotal, count =0;
	MOUSE_DEC mdec;
	MEMMAN *memman = (MEMMAN *) MEMMAN_ADDR;
	SHTCTL *shtctl;
	SHEET *sht_back, *sht_mouse, *sht_win;
	TIMER *timer, *timer2, *timer3;
	unsigned char *buf_back, buf_mouse[256], *buf_win;

	init_gdtidt();
	init_pic();
	io_sti();

	fifo32_init(&fifo, 128, fifobuf);
	init_pit();
	io_out8(PIC0_IMR, 0xf8); /* PIT, PIC1, キーボードを初期化 */
	io_out8(PIC1_IMR, 0xef);

	set490(&fifo,0);
	timer = timer_alloc();
	timer_init(timer, &fifo, 10);
	timer_settime(timer,1000);
	timer2 = timer_alloc();
	timer_init(timer2, &fifo, 3);
	timer_settime(timer2,300);
	timer3 = timer_alloc();
	timer_init(timer3, &fifo, 1);
	timer_settime(timer3, 50);

	init_keyboard(&fifo, 256);
	enable_mouse(&fifo, 512, &mdec);
	memtotal = memtest(0x00400000,0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff 
	memman_free(memman, 0x00400000, memtotal - 0x00400000);
	
	init_palette();	
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
	sht_back = sheet_alloc(shtctl);
	sht_mouse = sheet_alloc(shtctl);
	sht_win = sheet_alloc(shtctl);
	buf_back = (unsigned char *) memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
	buf_win = (unsigned char *) memman_alloc_4k(memman, 160 * 52);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1);
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);
	sheet_setbuf(sht_win, buf_win, 160, 52, -1);

	init_screen(buf_back, binfo->scrnx, binfo->scrny);

	init_mouse_cursor8(buf_mouse, 99); //COL8_008484
	make_window8(buf_win, 160, 68,"counter");
	sheet_slide(sht_back, 0, 0);
	mx = (binfo->scrnx -16)/2;
	my = (binfo->scrny -28-16)/2;
	sheet_slide(sht_mouse, mx, my);
	sheet_slide(sht_win, 80, 72);
	sheet_updown(sht_back, 0);
	sheet_updown(sht_win, 1);
	sheet_updown(sht_mouse, 2);
	sprintf(s, "(%3d, %3d)", mx, my);
	putfonts8_asc((char*)buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s);

	sprintf(s, "memory %dMB  free : %dKB, size : %dx%d", memtotal/(1024 *1024), memman_total(memman)/1024,binfo->scrnx, binfo->scrny);
	putfonts8_asc(buf_back, binfo->scrnx, 0, 32, COL8_FFFFFF, s);
	sheet_refresh(sht_back, 0, 0, binfo->scrnx, 48);

	for (;;) {
		count +=1;
		sprintf(s, "%010d",  count);
		putfont8_asc_sht(sht_win, 40, 28,COL8_000000, COL8_C6C6C6, s, 10);
		io_cli();
		if(fifo32_status(&fifo) == 0){
			io_sti();
		} else
		{	
			i = fifo32_get(&fifo);
			io_sti();
			if(256 <= i && i <=511){
				// キーボード
				sprintf(s, "%02X", i-256);
				putfont8_asc_sht(sht_back, 0, 16, COL8_FFFFFF, COL8_008484, s, 2);

			}else if(512 <= i && i <= 767){
				// マウス
				if( mouse_decode(binfo, &mdec, i)!=0 ){	
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
					if((mdec.btn & 0x01)!=0){
						s[1] = 'L';
					}
					if((mdec.btn & 0x02)!=0){
						s[3] = 'R';
					}
					if((mdec.btn & 0x04)!=0){
						s[2] = 'C';
					}
					putfont8_asc_sht(sht_back, 32, 16, COL8_FFFFFF, COL8_008484, s, 15);
					/* マウスカーソルの移動 */
					//boxfill8(binfo->vram, binfo->scrnx, COL8_008484, mx, my, mx+15, my+15);/* マウスを消す */
					mx += mdec.x;
					my += mdec.y;
					if(mx < 0){
						mx = 0;
					}
					if(my < 0){
						my = 0;
					}
					if(mx > binfo->scrnx - 1){
						mx = binfo->scrnx -1;
					}
					if(my > binfo->scrny -1){
						my = binfo->scrny - 1;
					}
					sprintf(s, "(%3d, %3d)", mx, my);
					putfont8_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);
					sheet_slide(sht_mouse, mx, my);
				}	
			}
	
			switch (i)
				{
					case 0: // カーソルタイマ
						timer_init(timer3, &fifo, 1); // 次は1 
						boxfill8(buf_back, binfo->scrnx, COL8_008484, 8, 96, 15, 111);
						timer_settime(timer3, 50);
						sheet_refresh(sht_back, 8, 96, 16, 112);
						break;
					case 1: // カーソルタイマ
						timer_init(timer3, &fifo, 0);
						boxfill8(buf_back, binfo->scrnx, COL8_FFFFFF, 8, 96, 15, 111);
						timer_settime(timer3, 50);
						sheet_refresh(sht_back, 8, 96, 16, 112);
						break;
					case 3:
						putfont8_asc_sht(sht_back, 0, 80, COL8_FFFFFF, COL8_008484, "3[sec]", 6);
						timer_init(timer2, &fifo, 0);
						count =0;
						break;					
					case 10:
						putfont8_asc_sht(sht_back, 0, 64, COL8_FFFFFF, COL8_008484, "10[sec]", 7);		
						count10 = count;
						sprintf(s, "%010d", count);
						putfont8_asc_sht(sht_win, 40, 28,COL8_000000, COL8_C6C6C6, s, 10);	
						sprintf(s, "%010d", count10);
						putfont8_asc_sht(sht_back, 60, 130, COL8_000000, COL8_008484, s, 10);	
						
						break;
				}	
		}
	}
}

void make_window8(unsigned char *buf, int xsize, int ysize, char *title){
	static char closebtn[14][16] ={
		"OOOOOOOOOOOOOOO@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQQQ@@QQQQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"O$$$$$$$$$$$$$$@",
		"@@@@@@@@@@@@@@@@",

	};
	int x, y;
	char c;
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 		 0, 		xsize -1, 	0		 );
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 		 1, 		xsize -2, 	1		 );
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 		 0, 		0, 			ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 		 1, 		1, 			ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1, 		xsize -2, 	ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0, 		xsize -1, 	ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2, 		 2, 		xsize -3, 	ysize - 3);
	boxfill8(buf, xsize, COL8_000084, 3, 		 3, 		xsize -4, 	20		 );
	boxfill8(buf, xsize, COL8_848484, 1, 		 ysize-2, 	xsize -2, 	ysize-2  );
	boxfill8(buf, xsize, COL8_000000, 0, 		 ysize-1, 	xsize -1, 	ysize-1  );
	putfonts8_asc(buf, xsize, 24, 4, COL8_FFFFFF, title);
	for(y=0; y <14; y++){
		for(x =0 ; x<16; x++){
			c = closebtn[y][x];
			if(c == '@'){
				c = COL8_000000;
			} else if(c == '$'){
				c = COL8_848484;
			} else if(c == 'Q'){
				c = COL8_C6C6C6;
			}else{
				c = COL8_FFFFFF;
			}
			buf[(5+y)*xsize + (xsize -21 + x)] = c;
		}
	}
	return;		
}

void putfont8_asc_sht(SHEET *sht, int x, int y, int c, int b, char *s, int l){
	boxfill8(sht->buf, sht->bxsize, b, x, y, x+l*8-1, y+15);
	putfonts8_asc((char *)sht->buf, sht->bxsize, x, y, c, s);
	sheet_refresh(sht, x, y, x + l*8, y+16);
}

void set490(FIFO32 *fifo, int mode){
	int i;
	TIMER *timer;
	if(mode != 0){
		for(i=0;i<490;i++){
			timer =timer_alloc();
			timer_init(timer, fifo, 1024 +i);
			timer_settime(timer, 100*60*60*24*50 + i*100);
		}
	}
	return;
}