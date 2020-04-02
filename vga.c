
#include <vga.h>

/********************************************************************************
 *                           Globals and constants                              *
 * *****************************************************************************/

#define YAASS_QUEEN         1
#define VIDEO_BASE_ADDRESS  0xA0000

#define SCREEN_WIDTH        320       /* width in pixels of mode 0x13 */
#define SCREEN_HEIGHT       200       /* height in pixels of mode 0x13 */

#define	VGA_AC_INDEX		0x3C0
#define	VGA_AC_WRITE		0x3C0
#define	VGA_AC_READ		    0x3C1
#define	VGA_MISC_WRITE		0x3C2
#define VGA_SEQ_INDEX		0x3C4
#define VGA_SEQ_DATA		0x3C5
#define	VGA_DAC_READ_INDEX	0x3C7
#define	VGA_DAC_WRITE_INDEX	0x3C8
#define	VGA_DAC_DATA		0x3C9
#define	VGA_MISC_READ		0x3CC
#define VGA_GC_INDEX 		0x3CE
#define VGA_GC_DATA 		0x3CF
/*			COLOR emulation		MONO emulation */
#define VGA_CRTC_INDEX		0x3D4		/* 0x3B4 */
#define VGA_CRTC_DATA		0x3D5		/* 0x3B5 */
#define	VGA_INSTAT_READ		0x3DA

#define	VGA_NUM_SEQ_REGS	5
#define	VGA_NUM_CRTC_REGS	25
#define	VGA_NUM_GC_REGS		9
#define	VGA_NUM_AC_REGS		21
#define	VGA_NUM_REGS		(1 + VGA_NUM_SEQ_REGS + VGA_NUM_CRTC_REGS + \
				VGA_NUM_GC_REGS + VGA_NUM_AC_REGS)

/* 256 color palette */
#define BLACK 0x00
#define WHITE 0x3F

#define MAX_WINDOWS 10

unsigned int g_window_id = 0;
PORT vga_port;

typedef struct {
	int x;
	int y;
	int width;
	int height;
	unsigned int window_id;
} window;

window windows[MAX_WINDOWS];

/****************************************************************
 *                Helper Function Prototypes                    *
 ***************************************************************/

void write_regs (unsigned char *regs);
void clear_screen ();
void fill_screen (int color);
void fill_rect (int x, int y, int width, int height, int color);
void draw_px (int x, int y, int color);
int clip_check (int x, int y, window * wnd);
int sgn (int x);
int abs (int a);

/*************************************************
 *                  DRAW PIXEL                   *
 * **********************************************/

void draw_pixel (VGA_WINDOW_MSG * msg)
{
	PARAM_VGA_DRAW_PIXEL * params = &msg->u.draw_pixel;

	assert(params->window_id < MAX_WINDOWS);
	window * wnd = &windows[params->window_id];

	/* draw pixel */
	if (clip_check(params->x, params->y, wnd))
		draw_px(wnd->x + params->x, wnd->y + params->y, params->color);
}

/*************************************************
 *                 CREATE WINDOW                 *
 * **********************************************/

void create_window (VGA_WINDOW_MSG * msg)
{
	PARAM_VGA_CREATE_WINDOW * params = &msg->u.create_window;
	int frame_x, frame_y, frame_w, frame_h;

	/* calculate frame size */
	frame_x = params->x-1;
	frame_y = params->y-10;
	frame_w = params->width+2;
	frame_h = params->height+11;

	/* check for clipping */
	if(frame_x < 0)
	{
		if(frame_w + frame_x < 0)
			return;

		frame_w += frame_x;
		frame_x = 0;
	}
	if(frame_x + frame_w >= SCREEN_WIDTH)
	{
		if(frame_x >= frame_w)
			return;

		frame_w = frame_w - frame_x;
	}
	if(frame_y < 0)
	{
		if(frame_h + frame_y < 0)
			return;

		frame_h += frame_y;
		frame_y = 0;
	}
	if(frame_y + frame_h >= SCREEN_HEIGHT)
	{
		if(frame_y >= frame_h)
			return;

		frame_h = frame_h - frame_y;
	}

	assert(g_window_id < MAX_WINDOWS);
	params->window_id = g_window_id++;

	/* render window */
	fill_rect(frame_x, frame_y, frame_w, frame_h, WHITE);
	fill_rect(params->x, params->y, params->width, params->height, BLACK);

	/* write title on frame */

	/* add to window array */
	window * new_window = &windows[params->window_id];
	new_window->x = params->x;
	new_window->y = params->y;
	new_window->width = params->width;
	new_window->height = params->height;
	new_window->window_id = params->window_id;
}

/*************************************************
 *                   DRAW TEXT                   *
 * **********************************************/

void draw_text (VGA_WINDOW_MSG * msg) 
{

}

/*************************************************
 *                   DRAW LINE                   *
 * **********************************************/

void draw_line (VGA_WINDOW_MSG * msg) 
{

	PARAM_VGA_DRAW_LINE * params = &msg->u.draw_line;

	assert(params->window_id < MAX_WINDOWS);
	window * wnd = &windows[params->window_id];
	
	int dx,dy,sdx,sdy,px,py,dxabs,dyabs,i;
	float slope;

	dx=params->x1-params->x0;
	dy=params->y1-params->y0;     
	dxabs=abs(dx);
	dyabs=abs(dy);
	sdx=sgn(dx);
	sdy=sgn(dy);
	if (dxabs>=dyabs)
	{
		slope=(float)dy / (float)dx;
		for(i=0;i!=dx;i+=sdx)
		{
			px=i+params->x1;
			py=slope*i+params->y0;
			if (clip_check(px, py, wnd))
				draw_px(wnd->x+px,wnd->y+py,params->color);
		}
	}
	else 
	{
		slope=(float)dx / (float)dy;
		for(i=0;i!=dy;i+=sdy)
		{
			px=slope*i+params->x1;
			py=i+params->y0;
			if (clip_check(px, py, wnd))
				draw_px(wnd->x+px,wnd->y+py,params->color);
		}
	}	
}

/********************************************************************************
 *                              VGA DEVICE DRIVER                               *
 * *****************************************************************************/

void vga_process (PROCESS proc, PARAM param)
{

	VGA_WINDOW_MSG * msg;
	PROCESS sender;

	/* wait for messages */
	while (1)
	{
		
		/* get message from sending process */
		msg = (VGA_WINDOW_MSG *) receive(&sender);
		/* process request from message */
		switch (msg->cmd) 
		{

			case VGA_CREATE_WINDOW:
				create_window(msg);
				break;

			case VGA_DRAW_TEXT:
				draw_text(msg);
				break;

			case VGA_DRAW_PIXEL:
				draw_pixel(msg);
				break;

			case VGA_DRAW_LINE:
				draw_line(msg);
				break;
		}

		/* reply to sender */
		reply(sender);
	}
}

/***************************************************************
 *                          INIT VGA                           *
 ***************************************************************/

int init_vga()
{
    int implemented = YAASS_QUEEN;

	unsigned char g_320x200x256[] =
	{
		/* MISC */
		0x63,
		/* SEQ */
		0x03, 0x01, 0x0F, 0x00, 0x0E,
		/* CRTC */
		0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
		0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
		0xFF,
		/* GC */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,
		0xFF,
		/* AC */
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x41, 0x00, 0x0F, 0x00, 0x00
	};

	/* set to vga 256 color mode */
    write_regs(g_320x200x256);

	/* create vga driver process */
    vga_port = create_process(vga_process, 7, 0, "VGA");

	/* get rid of junk in memory */
	clear_screen();

	/* ret */
    return implemented;
}

/**************************************************************
 *               WRITE TO THE VGA REGISTERS                   *
 *************************************************************/

void write_regs (unsigned char *regs)
{
	unsigned int i;

	/* write MISCELLANEOUS reg */
	outportb (VGA_MISC_WRITE, *(regs++));
	
	/* write SEQUENCER regs */
	for (i = 0; i < VGA_NUM_SEQ_REGS; i++)
	{
		outportb (VGA_SEQ_INDEX, i);
		outportb (VGA_SEQ_DATA, *(regs++));
	}

	/* unlock CRTC registers */
	outportb (VGA_CRTC_INDEX, 0x03);
	outportb (VGA_CRTC_DATA, inportb (VGA_CRTC_DATA) | 0x80);
	outportb (VGA_CRTC_INDEX, 0x11);
	outportb (VGA_CRTC_DATA, inportb (VGA_CRTC_DATA) & ~0x80);

	/* make sure they remain unlocked */
	regs[0x03] |= 0x80;
	regs[0x11] &= ~0x80;

	/* write CRTC regs */
	for (i = 0; i < VGA_NUM_CRTC_REGS; i++)
	{
		outportb (VGA_CRTC_INDEX, i);
		outportb (VGA_CRTC_DATA, *(regs++));
	}

	/* write GRAPHICS CONTROLLER regs */
	for (i = 0; i < VGA_NUM_GC_REGS; i++)
	{
		outportb (VGA_GC_INDEX, i);
		outportb (VGA_GC_DATA, *(regs++));
	}

	/* write ATTRIBUTE CONTROLLER regs */
	for (i = 0; i < VGA_NUM_AC_REGS; i++)
	{
		(void) inportb (VGA_INSTAT_READ);
		outportb (VGA_AC_INDEX, i);
		outportb (VGA_AC_WRITE, *(regs++));
	}

	/* lock 16-color palette and unblank display */
	(void) inportb (VGA_INSTAT_READ);
	outportb (VGA_AC_INDEX, 0x20);
}

/****************************************************************
 *                       HELPER FUNCTIONS                       *
 ***************************************************************/

void clear_screen () { fill_screen(0x00); }

void fill_screen (int color) 
{
	int x, y;
	for (x = 0; x < SCREEN_WIDTH; x++)
    	for (y = 0; y < SCREEN_HEIGHT; y++)
			draw_px(x, y, color);
}

void fill_rect (int x, int y, int width, int height, int color)
{
	int x2, y2;
	for(x2 = x; x2 < x + width; x2++)
			for(y2 = y; y2 < y + height; y2++)
				draw_px(x2, y2, color);
}

void draw_px (int x, int y, int color) 
{
	poke_b( (VIDEO_BASE_ADDRESS + y * SCREEN_WIDTH + x), color);
}

int clip_check (int x, int y, window * wnd)
{
		if (x < 0)
		return 0;

	if (x >= wnd->width)
		return 0;

	if (y < 0)
		return 0;

	if (y >= wnd->width)
		return 0;

	return 1;
}

int sgn (int x) {
	return (0 < x) - (x < 0);
}

int abs (int a) {
	if (a < 0) {
		return a*(-1);
	} else {
		return a;
	}
}

/*********************************************************
 *                      TEST PROCESS                     *
 *********************************************************/
/* I'm declaring this in vga.h and calling it in kernel_main.
This function is not necessary, I just wanted it in a place 
thats easy to find for when I'm debugging/testing */

void vga_test (PROCESS proc, PARAM param)
{
  	VGA_WINDOW_MSG msg;

	// Create Window 1
	msg.cmd = VGA_CREATE_WINDOW;
	msg.u.create_window.title = "Window 1";
	msg.u.create_window.x = 50;
	msg.u.create_window.y = 50;
	msg.u.create_window.width = 100;
	msg.u.create_window.height = 50;
	send(vga_port, &msg);
	unsigned int window1_id = msg.u.create_window.window_id;

	// Create Window 2
	msg.cmd = VGA_CREATE_WINDOW;
	msg.u.create_window.title = "Window 2";
	msg.u.create_window.x = 10;
	msg.u.create_window.y = 120;
	msg.u.create_window.width = 150;
	msg.u.create_window.height = 60;
	send(vga_port, &msg);
	unsigned int window2_id = msg.u.create_window.window_id;

	// Create Window 3
	msg.cmd = VGA_CREATE_WINDOW;
	msg.u.create_window.title = "Window 3";
	msg.u.create_window.x = 180;
	msg.u.create_window.y = 30;
	msg.u.create_window.width = 100;
	msg.u.create_window.height = 100;
	send(vga_port, &msg);
	unsigned int window3_id = msg.u.create_window.window_id;

	// Draw some lines in Window 1
	char current_color = 0;
	for (int x = 0; x < 100; x += 2) {
		msg.cmd = VGA_DRAW_LINE;
		msg.u.draw_line.window_id = window1_id;
		msg.u.draw_line.x0 = x;
		msg.u.draw_line.y0 = 0;
		msg.u.draw_line.x1 = 100 - x;
		msg.u.draw_line.y1 = 49;
		msg.u.draw_line.color = current_color++;
		send(vga_port, &msg);
	}

	// Write some text in Window 2
	msg.cmd = VGA_DRAW_TEXT;
	msg.u.draw_text.window_id = window2_id;
	msg.u.draw_text.text = "Hello CSC720!";
	msg.u.draw_text.x = 1;
	msg.u.draw_text.y = 1;
	msg.u.draw_text.fg_color = 0x3f; // White
	msg.u.draw_text.bg_color = 0;
	send(vga_port, &msg);

	// Write some text in Window 2 that will be clipped
	msg.cmd = VGA_DRAW_TEXT;
	msg.u.draw_text.window_id = window2_id;
	msg.u.draw_text.text = "Text that is too long will be clipped";
	msg.u.draw_text.x = 20;
	msg.u.draw_text.y = 20;
	msg.u.draw_text.fg_color = 0x3f; // White
	msg.u.draw_text.bg_color = 0;
	send(vga_port, &msg);

	// Draw some random pixels in Window 3
	msg.cmd = VGA_DRAW_PIXEL;
	msg.u.draw_pixel.window_id = window3_id;
	current_color = 0;
	for (int x = 3; x < 100; x += 5) {
		for (int y = 3; y < 100; y += 5) {
			msg.u.draw_pixel.x = x;
			msg.u.draw_pixel.y = y;
			msg.u.draw_pixel.color = current_color;
			current_color = (current_color + 1) % 64;
			send(vga_port, &msg);
		}
	}
}
