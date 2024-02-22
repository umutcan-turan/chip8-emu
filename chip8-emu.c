#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>

#include <SDL2/SDL.h>

#define DEF_W 128
#define DEF_H 64

struct {
	unsigned char mem[4 * 1024];
	unsigned char V[16];
	unsigned short pc;
	unsigned short I;
	unsigned short stack[16];
	unsigned short sindex;
	unsigned char dtimer;
	unsigned char stimer;
	unsigned char input_state[16];
	unsigned char display[32][64];
	int need_draw;
	int need_key;
	unsigned char running;
	SDL_Rect px;
} cpu;

int keytoval(int sym)
{
	switch (sym) {
	case SDLK_x:
		return 0;
	case SDLK_1:
		return 1;
	case SDLK_2:
		return 2;
	case SDLK_3:
		return 3;
	case SDLK_q:
		return 4;
	case SDLK_w:
		return 5;
	case SDLK_e:
		return 6;
	case SDLK_a:
		return 7;
	case SDLK_s:
		return 8;
	case SDLK_d:
		return 9;
	case SDLK_z:
		return 10;
	case SDLK_c:
		return 11;
	case SDLK_4:
		return 12;
	case SDLK_r:
		return 13;
	case SDLK_f:
		return 14;
	case SDLK_v:
		return 15;
	default:
		return -1;
	}
}

int process_event(SDL_Event *e)
{
	int val = -1;

	switch(e->type) {
	case SDL_QUIT:
		cpu.running = 0;
		break;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		val = keytoval(e->key.keysym.sym);
		if (val != -1) {
			cpu.input_state[val] = (e->type == SDL_KEYDOWN);
			if (e->type == SDL_KEYDOWN && cpu.need_key != -1) {
				cpu.V[cpu.need_key] = val;
				cpu.need_key = -1;
			}
			return val;
		}
		break;
	case SDL_WINDOWEVENT:
		cpu.need_draw = 1;
		if (e->window.event == SDL_WINDOWEVENT_RESIZED) {
			cpu.px.x = (e->window.data1 % 64) / 2;
			cpu.px.y = (e->window.data2 % 32) / 2;
			cpu.px.w = e->window.data1 / 64;
			cpu.px.h = e->window.data2 / 32;
		}
		break;
	default:
		break;
	}
	return val;
}

void load_font(void)
{
	static const unsigned char font[80] = {
		0xF0, 0x90, 0x90, 0x90, 0xF0, /* 0 */
		0x20, 0x60, 0x20, 0x20, 0x70, /* 1 */
		0xF0, 0x10, 0xF0, 0x80, 0xF0, /* 2 */
		0xF0, 0x10, 0xF0, 0x10, 0xF0, /* 3 */
		0x90, 0x90, 0xF0, 0x10, 0x10, /* 4 */
		0xF0, 0x80, 0xF0, 0x10, 0xF0, /* 5 */
		0xF0, 0x80, 0xF0, 0x90, 0xF0, /* 6 */
		0xF0, 0x10, 0x20, 0x40, 0x40, /* 7 */
		0xF0, 0x90, 0xF0, 0x90, 0xF0, /* 8 */
		0xF0, 0x90, 0xF0, 0x10, 0xF0, /* 9 */
		0xF0, 0x90, 0xF0, 0x90, 0x90, /* A */
		0xE0, 0x90, 0xE0, 0x90, 0xE0, /* B */
		0xF0, 0x80, 0x80, 0x80, 0xF0, /* C */
		0xE0, 0x90, 0x90, 0x90, 0xE0, /* D */
		0xF0, 0x80, 0xF0, 0x80, 0xF0, /* E */
		0xF0, 0x80, 0xF0, 0x80, 0x80, /* F */
	};
	
	for (int i = 0; i < 80; i++)
		cpu.mem[i] = font[i];
}

void init_cpu(void)
{
	memset(&cpu, 0, sizeof(cpu));
	cpu.pc = 0x200;
	cpu.running = 1;
	cpu.need_key = -1;
	cpu.px.x = DEF_W % 64;
	cpu.px.y = DEF_H % 32;
	cpu.px.w = DEF_W / 64;
	cpu.px.h = DEF_H / 32;
	load_font();
}

#define GETX(op)   (((op)>>8) & 0xf)
#define GETY(op)   (((op)>>4) & 0xf)
#define GETN(op)   ((op) & 0x000f)
#define GETNN(op)  ((op) & 0x00ff)
#define GETNNN(op) ((op) & 0x0fff)

void op_0(unsigned short c)
{
	if (GETNNN(c) == 0x0E0) { /* clr screen */
		cpu.need_draw = 1;
		memset(cpu.display, 0, 64 * 32);
	} else if (GETNNN(c) == 0x0EE && cpu.sindex > 0) { /* ret */
		cpu.pc = cpu.stack[--cpu.sindex];
	}
}

void op_1(unsigned short c) /* jump addr */
{
	cpu.pc = GETNNN(c);
}

void op_2(unsigned short c) /* exec subroutine */
{
	if (cpu.sindex < 16) {
		cpu.stack[cpu.sindex++] = cpu.pc;
		cpu.pc = GETNNN(c);
	}
}

void op_3(unsigned short c) /* skip if eq */
{
	if (cpu.V[GETX(c)] == GETNN(c))
		cpu.pc += 2;
}

void op_4(unsigned short c) /* skip if not eq */
{
	if (cpu.V[GETX(c)] != GETNN(c))
		cpu.pc += 2;
}

void op_5(unsigned short c) /* skip if eq */
{
	if (!GETN(c) && cpu.V[GETX(c)] == cpu.V[GETY(c)])
		cpu.pc += 2;
}

void op_6(unsigned short c) /* load */
{
	cpu.V[GETX(c)] = GETNN(c);
}

void op_7(unsigned short c) /* add */
{
	cpu.V[GETX(c)] += GETNN(c);
}

void op_8(unsigned short c)
{
	switch (GETN(c)) {
	case 0x0: /* load reg */
		cpu.V[GETX(c)] = cpu.V[GETY(c)];
		break;
	case 0x1: /* or */
		cpu.V[GETX(c)] |= cpu.V[GETY(c)];
		break;
	case 0x2: /* and */
		cpu.V[GETX(c)] &= cpu.V[GETY(c)];
		break;
	case 0x3: /* xor */
		cpu.V[GETX(c)] ^= cpu.V[GETY(c)];
		break;
	case 0x4: /* add carry */
		cpu.V[0xF] = cpu.V[GETX(c)] > (UCHAR_MAX - cpu.V[GETY(c)]);
		cpu.V[GETX(c)] += cpu.V[GETY(c)];
		break;
	case 0x5: /* sub borrow */
		cpu.V[0xF] = cpu.V[GETX(c)] >= cpu.V[GETY(c)];
		cpu.V[GETX(c)] -= cpu.V[GETY(c)];
		break;
	case 0x6: /* rshift */
		cpu.V[0xF] = cpu.V[GETY(c)] & 0x1;
		cpu.V[GETX(c)] = cpu.V[GETY(c)] >> 1;
		break;
	case 0x7: /* sub borrow */
		cpu.V[0xF] = cpu.V[GETY(c)] >= cpu.V[GETX(c)];
		cpu.V[GETX(c)] = cpu.V[GETY(c)] - cpu.V[GETX(c)];
		break;
	case 0xE: /* lshift */
		cpu.V[0xF] = (cpu.V[GETY(c)] >> 7) & 0x1;
		cpu.V[GETX(c)] = cpu.V[GETY(c)] << 1;
		break;
	default:
		break;
	}
}

void op_9(unsigned short c) /* skip not eq */
{
	if (!GETN(c) && cpu.V[GETX(c)] != cpu.V[GETY(c)])
		cpu.pc += 2;
}

void op_a(unsigned short c) /* set addr reg */
{
	cpu.I = GETNNN(c);
}

void op_b(unsigned short c) /* jump v0 + nnn */
{
	cpu.pc = cpu.V[0] + GETNNN(c);
}

void op_c(unsigned short c) /* rand mask nn */
{
	cpu.V[GETX(c)] = rand() & GETNN(c);
}

void op_d(unsigned short c) /* draw at x,y n height from I */
{
	cpu.need_draw = 1;
	cpu.V[0xF] = 0;
	for (int i = 0; i < GETN(c); i++) {
		unsigned char src  = cpu.mem[cpu.I+i];
		for (int bit = 0; bit < 7; bit++) {
			unsigned char x = cpu.V[GETX(c)];
			unsigned char y = cpu.V[GETY(c)];
			unsigned char src_bit = (src >> (7-bit)) & 1;
			unsigned char *dest = &cpu.display[(y+i)%32][(x+bit)%64];
			if (!cpu.V[0xF] && (*dest & src_bit))
				cpu.V[0xF] = 1;
			*dest ^= src_bit;
		}
	}
}

void op_e(unsigned short c)
{
	if (GETNN(c) == 0x9E && cpu.input_state[cpu.V[GETX(c)]]) /* skip if keydown */
		cpu.pc += 2;
	else if (GETNN(c) == 0xA1 && !cpu.input_state[cpu.V[GETX(c)]]) /* skip if !keydown */
		cpu.pc += 2;
}

void op_f(unsigned short c)
{

	switch (GETNN(c)) {
	case 0x07: /* get time */
		cpu.V[GETX(c)] = (unsigned char)(cpu.dtimer);
		break;
	case 0x0A: /* get input */
		cpu.need_key = GETX(c);
		break;
	case 0x15: /* set time */
		cpu.dtimer = cpu.V[GETX(c)];
		break;
	case 0x18: /* set stime */
		cpu.stimer = cpu.V[GETX(c)];
		break;
	case 0x1E: /* add I */
		cpu.I += cpu.V[GETX(c)];
		break;
	case 0x29: /* get font sprite */
		cpu.I = cpu.V[GETX(c)] * 5;
		break;
	case 0x33: /* BCD */
		cpu.mem[cpu.I]   = cpu.V[GETX(c)] / 100;
		cpu.mem[cpu.I+1] = (cpu.V[GETX(c)] % 100) / 10;
		cpu.mem[cpu.I+2] = cpu.V[GETX(c)] % 10;
		break;
	case 0x55: /* dump reg */
		for (int i = 0; i <= GETX(c); i++)
			cpu.mem[cpu.I++] = cpu.V[i];
		break;
	case 0x65: /* set reg */
		for (int i = 0; i <= GETX(c); i++)
			cpu.V[i] = cpu.mem[cpu.I++];
		break;
	default:
		break;
	}
}

typedef void (*opfuncptr)(unsigned short);
opfuncptr opfuncs[] = {
	op_0, op_1, op_2, op_3, op_4, op_5, op_6, op_7,
	op_8, op_9, op_a, op_b, op_c, op_d, op_e, op_f,
};

void run_cpu(void)
{
	static int tick_time = 0;
	struct timespec waittime;

	waittime.tv_sec = 0;
	waittime.tv_nsec = 10000000L;
	nanosleep(&waittime, NULL);
	for (int i = 0; i < 5; i++) {
		unsigned short opcode = (cpu.mem[(cpu.pc)&0x0fff] << 8) | cpu.mem[(cpu.pc+1)&0x0fff];
		cpu.pc += 2;
		opfuncs[(opcode >> 12) & 0xf](opcode);
		tick_time++;
		if (tick_time > 8) {
			tick_time = 0;
			if (cpu.dtimer)
				cpu.dtimer--;
			if (cpu.stimer)
				cpu.stimer--;
		}
	}
}


void draw_term(void) 
{
	for (int y = 0; y < 32; y++) {
		for (int x = 0; x < 64; x++) {
			unsigned char px = cpu.display[y][x];
			if (px)
				putchar('#');
			else
				putchar(' ');
		}
		putchar('\n');
	}
}


void sdl_draw(SDL_Renderer *r)
{
	SDL_Rect rect;
	
	draw_term();
	SDL_SetRenderDrawColor(r, 0x0, 0x0, 0x0, 255);
	SDL_RenderClear(r);
	SDL_SetRenderDrawColor(r, 0, 0xff, 0, 255);
	rect.w = cpu.px.w;
	rect.h = cpu.px.h;
	for (int y = 0; y < 32; y++) {
		for (int x = 0; x < 64; x++) {
			if (cpu.display[y][x]) {
				rect.x = cpu.px.x + x * cpu.px.w;
				rect.y = cpu.px.y + y * cpu.px.h;
				SDL_RenderFillRect(r, &rect);
			}
		}
	}
	SDL_RenderPresent(r);
	cpu.need_draw = 0;

}
int main(int argc, char *argv[])
{
	SDL_Window *w;
	SDL_Renderer *r;
	FILE *fp;

	if (argc != 2) {
		printf("Usage: %s filename\n", argc ? argv[0] : "");
		exit(1);
	}

	init_cpu();
	fp = fopen(argv[1], "rb");
	if (!fp) {
		printf("File not found: '%s'\n", argv[1]);
		exit(1);
	}
	if (!fread(cpu.mem+0x200, 1, (4 * 1024) - 0x200, fp)) {
		printf("Failed to read file: '%s'\n", argv[1]);
		exit(1);
	}
	fclose(fp);
	SDL_Init(SDL_INIT_VIDEO);
	w = SDL_CreateWindow("Chip-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                     DEF_W, DEF_H, SDL_WINDOW_RESIZABLE);
	r = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED);

	while (cpu.running) {
		SDL_Event e;
		while (cpu.running && SDL_PollEvent(&e))
			process_event(&e);
		if (cpu.need_key == -1)
			run_cpu();
		if (cpu.need_draw)
			sdl_draw(r);
	}

	SDL_DestroyRenderer(r);
	SDL_DestroyWindow(w);
	SDL_Quit();
	return 0;
}
