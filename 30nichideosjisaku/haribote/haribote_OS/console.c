#include "bootpack.h"
#include <stdio.h>
#include <string.h>

void console_task(SHEET *sheet, unsigned int memtotal){
	TIMER * timer;
	TASK *task = task_now();
	MEMMAN *memman = (MEMMAN *) MEMMAN_ADDR;
	int *fat = (int *)memman_alloc_4k(memman, 4*2880); 
	int i, fifobuf[128];
	CONSOLE cons;
	char cmdline[30];
	cons.sht = sheet;
	cons.cur_x = 8;
	cons.cur_y = 28;
	cons.cur_c = -1;
	*((int*)0x0fec) = (int) &cons;

	fifo32_init((FIFO32 *)task->fifo, 128, fifobuf, task);
	timer = timer_alloc();
	timer_init(timer, (FIFO32 *)task->fifo, 1);
	timer_settime(timer, 50);
	file_readfat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));

	// プロンプト表示
	cons_putchar(&cons, '>', 1);

	for(;;){
		io_cli();
		if(fifo32_status((FIFO32 *)task->fifo) == 0){
			task_sleep(task);
			io_sti();
		} else { 
			i = fifo32_get((FIFO32 *)task->fifo);
			io_sti();
			// カーソル用タイマ
			if(i <= 1){
				if(i != 0){
					timer_init(timer, (FIFO32 *)task->fifo, 0); //次は0
					if(cons.cur_c >= 0){
						cons.cur_c = COL8_FFFFFF;
					}
				} else {
					timer_init(timer, (FIFO32 *)task->fifo, 1); // 次は1
					if(cons.cur_c >= 0){
						cons.cur_c = COL8_000000;
					}
				}
				timer_settime(timer, 50);
			}
			switch (i)
			{
				case 2:
					cons.cur_c = COL8_FFFFFF;
					break;
			
				case 3:
					boxfill8(sheet->buf, sheet->bxsize, COL8_000000, cons.cur_x, cons.cur_y, cons.cur_x + 7 ,cons.cur_y + 15);
					cons.cur_c = -1;
					break;
			}
			if(256 <= i && i<=511){ // キーボードデータ (タスクA経由)

				if(i == 8 +256){ // バックスペース
					if(cons.cur_x > 16){ 
						// カーソルをスペースで消してからカーソルを一つ戻す
						cons_putchar(&cons, ' ', 0);
						cons.cur_x -= 8;
					} /*else if(cons.cur_x == 16){
						// ">"だけにする
						cons_putchar(&cons, ' ', 0);
					}*/
				} else if(i == 10 + 256){ //enter
				
					// カーソルをスペースで消す
					cons_putchar(&cons, ' ', 0);
					cmdline[cons.cur_x/8 -2] = 0;
					cons_newline(&cons);
					cons_runcmd(cmdline, &cons, fat, memtotal);
					cons_putchar(&cons, '>', 1);

				} else {
					// 一般文字
					if(cons.cur_x < 240){
						// 一文字表示してからカーソルを一つ進める
						cmdline[cons.cur_x/8 -2] = i-256;
						cons_putchar(&cons, i-256, 1);
						}
				}
			}
			// カーソル再表示
			if(cons.cur_c >= 0){
					boxfill8(sheet->buf, sheet->bxsize, cons.cur_c, cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
			}
			sheet_refresh(sheet, cons.cur_x, cons.cur_y, cons.cur_x + 8, cons.cur_y+16);
		}
	}
}

void cons_newline(CONSOLE *cons){
	int x, y;
	SHEET *sheet =cons->sht;
	if(cons->cur_y < 28 + 112){
		 cons->cur_y += 16; // 次の行
	} else {
		for(y = 28; y < 28 + 112; y++){
			for(x = 8; x < 8 + 240;x++){
				sheet->buf[x + y*sheet->bxsize] = sheet->buf[x + (y+16)*sheet->bxsize];
			}
		}
		for(y = 28 + 112; y<28 +128; y++){
			for(x = 8; x< 8 +240;x++){
				sheet->buf[x + y*sheet->bxsize] = COL8_000000;
			}
		}
		sheet_refresh(sheet, 8, 28, 8+240, 28+128);
	}
	cons->cur_x = 8;
	return;
}


void cmd_mem(CONSOLE *cons, unsigned int memtotal){
	MEMMAN *memman = (MEMMAN *) MEMMAN_ADDR;
	char s[60];
	sprintf(s, "total	%dMB\nfree %dKB\n\n",memtotal/(1024*1024), memman_total(memman)/1024);
	cons_putstr0(cons, s);
	return;
}

void cmd_cls(CONSOLE *cons){
	int x, y;
	SHEET *sheet = cons->sht;
		for(y = 28; y<28 +128;y++){
			for(x = 8; x < 8+240; x++){
				sheet->buf[x + y*sheet->bxsize] = COL8_000000;
			}
		}
		sheet_refresh(sheet, 8, 28, 8+240, 28 +128);
		cons->cur_y = 28;
}

void cmd_ls(CONSOLE *cons){
	int x, y;
	FILEINFO *finfo = (FILEINFO *) (ADR_DISKIMG + 0x002600);
	char s[30];
	for(x = 0; x<224; x++){
		if(finfo[x].name[0] == 0x00){
			break;
		}
		if(finfo[x].name[0] != 0xe5){
			if((finfo[x].type & 0x18) == 0){
				sprintf(s, "filename.ext	%7d\n", finfo[x].size);
				for(y = 0 ;y < 8; y++){
					s[y] = finfo[x].name[y];
				}
				s[ 9] = finfo[x].ext[0];
				s[10] = finfo[x].ext[1];
				s[11] = finfo[x].ext[2];
				cons_putstr0(cons, s);
			}
		}
	}
	cons_newline(cons);
	return;
}

void cmd_cat(CONSOLE *cons, int *fat, char *cmdline){
	MEMMAN *memman = (MEMMAN *) MEMMAN_ADDR;
	FILEINFO *finfo = file_search(cmdline + 4, (FILEINFO *)(ADR_DISKIMG + 0x002600),224);
	char *p;
	int i;
	if(finfo != 0){
		// ファイルが見つかった
		p = (char *)memman_alloc_4k(memman, finfo->size);
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *)(ADR_DISKIMG + 0x003e00));
		cons_putstr1(cons, p, finfo->size);
		memman_free_4k(memman, (int)p, finfo->size);
	} else {
		// ファイルが見つからない
		cons_putstr0(cons,"File not Found.\n");
	}
	cons_newline(cons);
	return;
}

void cmd_hlt(CONSOLE *cons, int *fat){
	MEMMAN *memman = (MEMMAN *) MEMMAN_ADDR;
	FILEINFO *finfo = file_search("HLT.HRB", (FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	SEGMENT_DISCRIPTOR *gdt = (SEGMENT_DISCRIPTOR *) ADR_GDT;
	char *p;
	if(finfo != 0){
		// ファイルが見つかった
		p = (char *)memman_alloc_4k(memman, finfo->size);
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *)(ADR_DISKIMG + 0x003e00));
		set_segmdesc(gdt + 1003, finfo->size -1, (int)p, AR_CODE32_ER);
		farcall(0, 1003*8);
		memman_free_4k(memman, (int)p, finfo->size);
	} else {
		// ファイルが見つからなかった
		putfonts8_asc_sht(cons->sht, 8,  cons->cur_y, COL8_FFFFFF, COL8_000000, "File not found.", 15);
		cons_newline(cons);
	}
	cons_newline(cons);
	return;
}

void cons_putchar(CONSOLE *cons, int chr, char move){
	char s[2];
	s[0] = chr;
	s[1] = 0;
	if(s[0] == 0x09){ // タブ
		for(;;){
			putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
			cons->cur_x +=8;
			if(cons->cur_x == 8 +240){
				cons_newline(cons);
			}
			if(((cons->cur_x -8 )& 0x1f) == 0){
				break;	 // 32で割り切れたらbreak
			}
		}
	} else if(s[0] == 0x0a){ // 改行
		cons_newline(cons);
	} else if(s[0] == 0x0d){ // 復帰
		// とりあえず何もしない
	} else {
		putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
		if(move != 0){ //move が0のときはカーソルを進めない
			cons->cur_x += 8;
			if(cons->cur_x == 8 +240){
				cons_newline(cons);
			}
		}
	}
	return;
 }

 void cons_runcmd(char* cmdline, CONSOLE *cons, int *fat, unsigned int memtotal){
		 if(strcmp(cmdline, "mem") == 0){	cmd_mem(cons, memtotal); }
	else if(strcmp(cmdline, "cls") == 0){	cmd_cls(cons);}
	else if(strcmp(cmdline, "ls") == 0){	cmd_ls(cons);}
	else if(strncmp(cmdline, "cat ", 4) == 0){	cmd_cat(cons, fat, cmdline);}
	else if(strcmp(cmdline, "hlt") == 0){	cmd_hlt(cons, fat);}
	else if(cmdline[0] != 0){
		if(cmd_app(cons,fat,cmdline) == 0){
		// コマンドではなくアプリでなく空行でない
		//putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, "Bad Command", 12);
		cons_putstr0(cons, "Bad Command.\n\n");
		}
	}
	return;
}

int cmd_app(CONSOLE *cons,  int *fat, char *cmdline){
	MEMMAN *memman = (MEMMAN *) MEMMAN_ADDR;
	FILEINFO *finfo;
	SEGMENT_DISCRIPTOR *gdt = (SEGMENT_DISCRIPTOR *) ADR_GDT;
	char name[18], *p;
	int i;

	// コマンドラインからファイル名を生成
	for(i=0; i<13; i++){
		if(cmdline[i] <= ' '){
			break;
		}
		name[i] = cmdline[i];
	}
	name[i] = 0; // ファイル名の後ろを0にする

	// ファイルを探す
	finfo = file_search(name, (FILEINFO *)(ADR_DISKIMG + 0x002600), 224);
	if(finfo == 0 && name[i-1] != '.'){
		// 見つからなかったので後ろに".HRB"をつけてもう一度探す
		name[i] = '.';
		name[i+1] = 'H';
		name[i+2] = 'R';
		name[i+3] = 'B';
		name[i+4] = 0;
		finfo = file_search(name,(FILEINFO *)(ADR_DISKIMG + 0x002600), 224);
	}

	if(finfo != 0){
		// ファイルが見つかった場合
		p = (char *)memman_alloc_4k(memman, finfo->size);
		*((int *)0xfe8) = (int) p;
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
		set_segmdesc(gdt +1003, finfo->size - 1, (int)p, AR_CODE32_ER);
		if(finfo->size >= 8 && strncmp(p+4, "Hari", 4) == 0){
			p[0] = 0xe8;
			p[1] = 0x16;
			p[2] = 0x00;
			p[3] = 0x00;
			p[4] = 0x00;
			p[5] = 0xcb;
			
		}
		farcall(0,1003*8);
		memman_free_4k(memman, (int)p, finfo->size);
		cons_newline(cons);
		return 1;
	}
	return 0;
}

void cons_putstr0(CONSOLE *cons, char *s){
	for(; *s != 0;s++){
		cons_putchar(cons, *s, 1);
	}
	return;
}

void cons_putstr1(CONSOLE *cons, char *s, int l){
	int i;
	for(i=0;i<l;i++){
		cons_putchar(cons, s[i], 1);
	}
	return;
}

void hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax){
	int cs_base = *((int *) 0xfe8);
	CONSOLE *cons = (CONSOLE *) *((int *) 0x0fec);
	if(edx ==1){
		cons_putchar(cons, eax&0xff, 1);
	} else if(edx == 2){
		cons_putstr0(cons, (char *)ebx + cs_base);
	} else if(edx == 3){
		cons_putstr1(cons, (char *)ebx + cs_base, ecx);
	}
	return;
}