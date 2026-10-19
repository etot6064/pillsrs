/* pillsrs (2d) v0.1 by etot
a unique puzzle game inspired by an unusual way i played setris once,
originally poorly made in turbowarp in a handful of hours, now remade
from scratch for the ti84+ce. i misspelled "pillars" when saving the
turbowarp version for the first time and just rolled with it.
*/

//todo
//clean up menu
//clean up failure
//ingame sidebar
//menu sidebar
//input buffering during flicker?
//save file?
//scores?
//double c_stored?
//double next?
//color menu??

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <sys/util.h>
#include <sys/rtc.h>


//flag!- defines

//functions
/* correctly converts rgb values into ti's weird 1555 format.
seriously, why is the most significant bit of the whole color
the least significant bit of the green channel??
@param r red, 0-31
@param g green, 0-63
@param b blue, 0-31
*/
#define c1555(r, g, b) \
((uint16_t)( (((g) & 0b1) << 15) | ((r) << 10) | (((g) & 0b111110) << 4) | (b) ))
/* increase/decrease a variable based on two conditions, with limits.
also optionally runs a function if the variable is changed.
@param var variable to use
@param top upper limit
@param bottom lower limit
@param inc increase if true
@param dec decrease if true
@param success function(s) run if variable changed. include semicolon
*/
#define updown(var, top, bottom, inc, dec, success) \
if ((inc) && ((var) < (top))) {(var)++; success} \
else if ((dec) && ((var) > (bottom))) {(var)--; success}
/* increase/decrease a variable based on two conditions, with overflow.
also optionally runs a function if the variable is changed.
@param var variable to use
@param top max value before loopover
@param bottom min value before loopunder
@param inc increase if true
@param dec decrease if true
@param success function(s) run if variable changed. include semicolon
*/
#define updown_loop(var, top, bottom, inc, dec, success) \
if (inc) {if ((var) < (top)) {(var)++;} else {(var) = (bottom);} success} \
else if (dec) {if ((var) > (bottom)) {(var)--;} else {(var) = (top);} success}

//shortcuts
#define g gridpos
#define c(i) (firstcolor + (i))
#define c_rand (firstcolor + randInt(0, colors - 1))
#define l 0
#define r 1

//keys
#define k2nd (kpress[1] & kb_2nd)
#define kclear (kpress[6] & kb_Clear)
#define kenter (kpress[6] & kb_Enter)
#define kup (kpress[7] & kb_Up)
#define kdown (kpress[7] & kb_Down)
#define kleft (kpress[7] & kb_Left)
#define kright (kpress[7] & kb_Right)
#define dup (kdir & kb_Up)
#define ddown (kdir & kb_Down)
#define dleft (kdir & kb_Left)
#define dright (kdir & kb_Right)

//palette
#define white 0 //palette index
#define black 1 //palette index
#define transparent 2 //palette index
#define flicker 3 //palette index
#define firstcolor 4 //palette index

//settings
#define rpt_init 11469 //32768hz * 350ms
#define rpt_delay 2458 //32768hz * 75ms
#define anim_speed 2621 //32768hz * 80ms
#define anim_count 8
#define menu_board 12
//#define debug

//limits
#define board_lower 6
#define board_upper 24
#define colors_lower 3
#define colors_upper 12

//strings
#define str_title "pillsrs"
#define str_play "Play"
#define str_board "Grid size: "
#define str_colors "Colors: "
#define str_quit "Quit"
#define str_select "\x10 "
#define str_arrow_left "< "
#define str_arrow_right " >"
#define str_limit_left "   "
#define str_limit_right ""


//flag!- variables

//keys
uint8_t kprev[8] = {0}; //previous kb_data
uint8_t kpress[8] = {0}; //rising edge of kb_data
uint8_t lastdir = 0b0; //kpress[7] at time of last rising edge
uint8_t kdir = 0b0; //kpress[7] with repeat
bool quit = false;

//control
uint8_t gridpos[board_upper+1] = {0}; //board position to screen position
uint8_t updboard = 2;
uint8_t updsdbar = 2; //sidebar
bool draw = false;
bool in_menu = true;
bool lose = false;

//menu
static uint8_t menu_len[2][menu_board] = {
	{0, 0, 4, 6},
	{0, 2, 2, 2, 3, 3, 4}
};
static uint8_t menu_color[2][menu_board] = {
	{c(0), c(1), c(2), c(3)},
	{c(4), c(5), c(6), c(7), c(8), c(9), c(1)}
};
static uint8_t menu_top[2] = {4, 7};
static uint8_t title_x[] = {3, 6, 6, 6, 3, 3, 3};
uint8_t menu_selection = 0;

//game
uint8_t len[2][board_upper] = {{0}};
uint8_t color[2][board_upper] = {{0}};
uint8_t top[2] = {0};
uint8_t board = 12;
uint8_t colors = 8;
uint8_t c_next = 0;
uint8_t c_current = 0;
uint8_t c_stored = 0;

//clear logic
uint8_t c_match;
uint8_t top_piece = 0;
uint8_t animstate = 0;
bool clearcheck = false;
bool clearside = false;
bool can_match = false;
bool animflip = false;


//flag!- functions
void sclgrid(uint24_t n) {
	for (uint24_t i = 0; i < n+1; i++) {
		g[i] = 240 - ((i * 240) / n);
	}
}
void drawboard() {
	for (uint24_t li = 0; li < top[l]; li++) {
		gfx_SetColor(color[l][li]);
		gfx_FillRectangle_NoClip(0, g[li+1], g[len[l][li]], g[li]-g[li+1]);
	}
	for (uint24_t ri = 0; ri < top[r]; ri++) {
		gfx_SetColor(color[r][ri]);
		gfx_FillRectangle_NoClip(g[ri+1], 0, g[ri]-g[ri+1], g[len[r][ri]]);
	} 
	gfx_SetColor((top[l] == 0) && (top[r] == 0) ? flicker : white);
	gfx_FillRectangle_NoClip(0, 0, g[top[r]], g[top[l]]);
}

void setboard_menu() {
	memcpy(len[l], menu_len[l], sizeof(menu_len[l]));
	memcpy(len[r], menu_len[r], sizeof(menu_len[r]));
	memcpy(color[l], menu_color[l], sizeof(menu_color[l]));
	memcpy(color[r], menu_color[r], sizeof(menu_color[r]));
	memcpy(top, menu_top, sizeof(menu_top));
}
void setboard_clear() {
	memset(len, 0, sizeof(len));
	memset(color, 0, sizeof(color));
	memset(top, 0, sizeof(top));
}

void switchto_menu() {
	timer_Disable(2);
	timer_SetReload(2, rpt_delay);
	timer_AckInterrupt(2, TIMER_RELOADED);
	in_menu = true;
	menu_selection = 0;
	animstate = 0;
	animflip = false;
	gfx_palette[flicker]  = gfx_palette[white];
	sclgrid(menu_board);
	setboard_menu();
	updboard = 2;
	updsdbar = 2;
}
void switchto_game() {
	timer_Disable(2);
	timer_SetReload(2, anim_speed);
	timer_AckInterrupt(2, TIMER_RELOADED);
	in_menu = false;
	kdir = 0;
	lose = false;
	c_next = c_rand;
	c_current = c_rand;
	c_stored = c_rand;
	sclgrid(board);
	setboard_clear();
	updboard = 2;
	updsdbar = 2;
}

void anim() {
	animstate = anim_count;
	timer_Disable(2);
	timer_Set(2, anim_speed);
	timer_Enable(2, TIMER_32K, TIMER_0INT, TIMER_DOWN);
}
void placed(bool a) {
	bool b = !a;
	len[a][top[a]] = top[b];
	color[a][top[a]] = c_current;
	top[a]++;
	c_current = c_next;
	c_next = c_rand;
	if ((top[b] > 0) && (color[a][top[a] - 1] == color[b][top[b] - 1])) {
		c_match = color[a][top[a]-1];
		color[a][top[a]-1] = flicker;
		top_piece = top[b];
		while (top_piece > 0 && color[b][top_piece-1] == c_match) {
			color[b][top_piece-1] = flicker; top_piece--;
		}
		anim();
		clearside = a;
	}
	if(top[a] == board && color[a][board-1] != flicker) {lose = true;}
	updboard = 2;
	updsdbar = 2;
}

//flag!- main
int main(void) {
	//setup
	{
		switchto_menu();
		srandom(rtc_Time() + *TIMER_COUNT_ADDR(3));
		kb_SetMode(MODE_0_IDLE);
		kb_Scan();
		for (uint24_t i = 1; i < 8; i++) {
			kprev[i] = kb_Data[i];
		}
		gfx_Begin();
		gfx_SetDrawBuffer();
		gfx_SetTextConfig(gfx_text_noclip);
		gfx_SetTextTransparentColor(transparent);
		gfx_SetTextBGColor(transparent);
		gfx_SetTextFGColor(black);
		{ //palette
			gfx_palette[0]  = c1555(31, 63, 31); //always white
			gfx_palette[1]  = c1555(0,  0,  0 ); //always black
			gfx_palette[2]  = 0;                 //transparent
			gfx_palette[3]  = gfx_palette[white]; //flicker
			gfx_palette[4]  = c1555(31, 0,  0 ); //c0  (red)
			gfx_palette[5]  = c1555(0,  0,  31); //c1  (blue)
			gfx_palette[6]  = c1555(0,  63, 0 ); //c2  (lime)
			gfx_palette[7]  = c1555(31, 63, 0 ); //c3  (yellow)
			gfx_palette[8]  = c1555(0,  63, 31); //c4  (cyan)
			gfx_palette[9]  = c1555(31, 0,  31); //c5  (magenta)
			gfx_palette[10] = c1555(31, 31, 0 ); //c6  (orange)
			gfx_palette[11] = c1555(23, 0,  31); //c7  (purple)
			gfx_palette[12] = c1555(15, 31, 15); //c8  (grey)
			gfx_palette[13] = c1555(31, 0,  15); //c9  (amaranth) (my favorite :3)
			gfx_palette[14] = c1555(15, 0,  0 ); //c10 (maroon)
			gfx_palette[15] = c1555(0,  31, 0 ); //c11 (dark green)
		}
	}

	//loop
	while (true) {
		kb_Scan();
		if (kb_Data[1] & kb_Del) {quit = true;}
		for (uint24_t i = 1; i < 8; i++) {
			kpress[i] = ~kprev[i] & kb_Data[i];
			kprev[i] = kb_Data[i];
		}

		//flag!- buttons
		if (in_menu) {
			{ //repeat logic
				if (timer_ChkInterrupt(2, TIMER_RELOADED)) {
					timer_AckInterrupt(2, TIMER_RELOADED);
					kdir = lastdir;
				} else {kdir = 0;}
				if (kpress[7] > 0) {
					/* yes i know if two directions are pressed at once this
					repeats all of them, but i don't care. i can't think of a
					better way and it's a pretty unlikely edge case. */
					lastdir = kpress[7];
					timer_Disable(2);
					timer_Set(2, rpt_init);
					timer_Enable(2, TIMER_32K, TIMER_0INT, TIMER_DOWN);
					kdir = lastdir & kb_Data[7];
				}
				else if ((lastdir & kb_Data[7]) == 0) {
					timer_Disable(2);
					kdir = 0;
				}
			}
			
			if (kclear) {quit = true;}
			else updown_loop(menu_selection, 3, 0, ddown, dup, updboard = 2;)
			else switch (menu_selection) {
				case 0: { //play
					if (k2nd || kenter || kright) {switchto_game();}
				break;}
				case 1: { //board
					updown(board, board_upper, board_lower, dright, dleft, updboard = 2;)
				break;}
				case 2: { //colors
					updown(colors, colors_upper, colors_lower, dright, dleft, updboard = 2;)
				break;}
				case 3: { //quit
					if (k2nd || kenter || kright) {quit = true;}
				break;}
			}
		}
		else if (lose) {
			if (kclear || k2nd || kenter || kpress[7] > 0) {switchto_menu();}
		} 
		else { //in game
			if (kclear) {switchto_menu();}
			else if (kup || kdown) {
				uint24_t c_temp = c_stored;
				c_stored = c_current;
				c_current = c_temp;
				updsdbar = 2;
			}
			else if (kright && !animstate) {placed(r);}
			else if (kleft && !animstate) {placed(l);}
		}
		if (quit) {break;}

		//flag!- logic
		if (animstate > 0 && timer_ChkInterrupt(2, TIMER_RELOADED)) {
			timer_AckInterrupt(2, TIMER_RELOADED);
			animstate--;
			if (animstate == 0) {
				timer_Disable(2);
				animflip = false;
				if(top[l] > 0 && top[r] > 0) {clearcheck = true;}
			} else {animflip = !animflip;}
			gfx_palette[flicker] = gfx_palette[animflip ? black : white];
		}
		if (clearcheck) {
			clearcheck = false;
			bool a, b;
			if (clearside) {a = r, b = l;} else {a = l, b = r;} 
			top[b] = top_piece;
			if (top_piece > 0) {
				can_match = true;
				c_match = color[b][top_piece-1]; 
			} else {can_match = false;}
			uint24_t write = len[b][top_piece];
			for (uint24_t read = write; read < top[a]; read++) {
				if(color[a][read] != flicker) {
					if(can_match && color[a][read] == c_match) {
						color[a][write] = flicker;
						clearcheck = true;
					} else {
						color[a][write] = color[a][read];
					}
					len[a][write] = top_piece;
					write++;
				}
			}
			top[a] = write;
			if (clearcheck == true) {
				clearcheck = false;
				while (top_piece > 0 && color[b][top_piece-1] == c_match) {
					color[b][top_piece-1] = flicker; top_piece--;
				}
				anim();
			}
			if(top[a] == 0 && top[b] == 0) {anim();}
			updboard = 2;
		}

		//flag!- display
		if (updboard > 0) {
			updboard--;
			draw = true;
			drawboard();

			if (in_menu) {
				//title
				gfx_SetTextScale(2, 2);
				for (uint24_t i = 0; i < sizeof(str_title)-1; i++) {
					gfx_SetTextXY((100 + (20 * i) + (title_x[i])), 3);
					gfx_PrintChar(str_title[i]);
				}
				gfx_SetTextScale(1, 1);

				//make better when layout is finalized===================================
				{//play
					gfx_SetTextXY(6, 166);
					if (menu_selection == 0)
						{gfx_PrintString(str_select);}
					gfx_PrintString(str_play);
				}
				{//board
					gfx_SetTextXY(6, 186);
					if (menu_selection == 1) {
						gfx_PrintString(str_select str_board);
						if (board == board_lower)
						{gfx_PrintString(str_limit_left);}
						else {gfx_PrintString(str_arrow_left);}
						gfx_PrintUInt(board, 1);
						if (board == board_upper)
						{gfx_PrintString(str_limit_right);}
						else {gfx_PrintString(str_arrow_right);}
					} else {
						gfx_PrintString(str_board);
						gfx_PrintUInt(board, 1);
					}
				}
				{//colors
					gfx_SetTextXY(6, 206);
					if (menu_selection == 2) {
						gfx_PrintString(str_select str_colors);
						if (colors == colors_lower)
						{gfx_PrintString(str_limit_left);}
						else {gfx_PrintString(str_arrow_left);}
						gfx_PrintUInt(colors, 1);
						if (colors == colors_upper)
						{gfx_PrintString(str_limit_right);}
						else {gfx_PrintString(str_arrow_right);}
					} else {
						gfx_PrintString(str_colors);
						gfx_PrintUInt(colors, 1);
					}
				}
				{//quit
					gfx_SetTextXY(6, 226);
					if (menu_selection == 3)
						{gfx_PrintString(str_select);}
					gfx_PrintString(str_quit);
				}
			}
			else { //in game
				#ifdef debug
					gfx_SetTextConfig(gfx_text_clip);
					for (uint24_t i = 0; i < board; i++) {
						gfx_SetTextXY(0, g[i+1]+3);
						gfx_PrintUInt(color[l][i], 0);
						gfx_SetTextXY(g[i+1]+3, 0);
						gfx_PrintUInt(color[r][i], 0);
						gfx_SetTextXY(18, g[i+1]+3);
						gfx_PrintUInt(len[l][i], 0);
						gfx_SetTextXY(g[i+1]+3, 10);
						gfx_PrintUInt(len[r][i], 0);
					}
					gfx_SetColor(black);
					gfx_FillRectangle(0, g[top[l]]-2, 80, 4);
					gfx_SetTextXY(84, g[top[l]]-4);
					gfx_PrintUInt(top[l], 1);
					gfx_FillRectangle(g[top[r]]-2, 0, 4, 80);
					gfx_SetTextXY(g[top[r]]-4, 84);
					gfx_PrintUInt(top[r], 1);
					gfx_SetColor(firstcolor+9);
					if (clearside) {
						gfx_FillRectangle(0, g[top_piece]-2, 80, 4);
						gfx_SetTextXY(84, g[top_piece]-4);
						gfx_PrintUInt(top_piece, 1);
					} else {
						gfx_FillRectangle(g[top_piece]-2, 0, 4, 80);
						gfx_SetTextXY(g[top_piece]-4, 84);
						gfx_PrintUInt(top_piece, 1);
					}
					gfx_SetTextConfig(gfx_text_noclip);
				#endif

				if (lose) {gfx_FillRectangle_NoClip(40, 40, 160, 160);}
			}
		}
		if (updsdbar > 0) {
			updsdbar--;
			draw = true;
			gfx_SetColor(black);
			gfx_FillRectangle_NoClip(240, 0, 4, 240);

			if(!in_menu) { //in game
				gfx_SetColor(c_next);
				gfx_FillRectangle_NoClip(244, 0, 76, 40);
				gfx_SetColor(c_current);
				gfx_FillRectangle_NoClip(244, 40, 76, 120);
				gfx_SetColor(c_stored);
				gfx_FillRectangle_NoClip(244, 160, 76, 80);
			}
		}
		if (draw) {
			draw = false;
			gfx_SwapDraw();
		}
	}

	//wrapup
	gfx_End();
	kb_Reset();
	timer_Disable(2);
	return 0;
}