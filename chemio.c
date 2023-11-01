#include <time.h>
#include <math.h>
#include <stdio.h>

#if PLATFORM_WEB
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#define VWIDTH 800
#define VHEIGHT 600
#define REMAINING 10.0f

#define assert(c)  while (!(c)) __builtin_unreachable()
#define countof(array) (sizeof ((array)) / sizeof *(array))

enum {
	TAG_NONE,
	TAG_C,
	TAG_O,
	TAG_2O,
	TAG_N,

	COUNTOF_TAGS,
};
static Texture2D g_tag_texture[COUNTOF_TAGS];
static float g_tag_origin[COUNTOF_TAGS];

typedef struct gamestate gamestate;
typedef struct level level;
typedef struct droppoint droppoint;
typedef struct drag drag;

struct level {
	char const *name;
	float scale;
	Texture2D base;
	droppoint *dps;
	int n_dps, expect;
	drag *drags;
	int n_drags;
};

struct droppoint {
	float x, y;
	float rotation;
	int tag;
};

struct drag {
	int tag;
	float x, y;
};

static droppoint *nearest_droppoint(level const *l, float x, float y, float *o_dist) {
	droppoint *res = 0;
	float nearest = 0.0f;
	for (int i = 0; i < l->n_dps; i++) {
		droppoint *it = l->dps + i;
		float dist = sqrtf(powf(x-it->x, 2) + powf(y-it->y, 2));
		if (dist < nearest || !res) {
			res = it;
			nearest = dist;
		}
	}
	if (o_dist) *o_dist = nearest;
	return res;
}

static void draw_level(level const *l, float time) {
	assert(l != 0);
	DrawTexture(l->base, 0, 0, WHITE);
	enum { FONTSIZE = 32+16 };
	int measure_name = MeasureText(l->name, FONTSIZE);
	DrawText(l->name, (VWIDTH-measure_name)/2, 32, FONTSIZE, BLACK);
	char timestr[16];
	if (isnan(time)) {
		snprintf(timestr, sizeof(timestr), "ZEN");
	} else {
		snprintf(timestr, sizeof(timestr), "%.2fs", time);
	}
	int measure_time = MeasureText(timestr, FONTSIZE-16);
	DrawText(timestr, (VWIDTH-measure_time)/2, 32+FONTSIZE, FONTSIZE-16, (time < 1.0f) ? RED : WHITE);
	for (int i = 0; i < l->n_dps; i++) {
		int len = 12;
		DrawCircle(l->dps[i].x, l->dps[i].y, len/2, (Color){0,0,0,200});
	}
	for (int i = 0; i < l->n_drags; i++) {
		drag d = l->drags[i];
		float dist = 0.0f;
		droppoint *dp = nearest_droppoint(l, d.x, d.y, &dist);
		float rotation = dist == 0 ? dp->rotation : 0.0f;
		float scale = dist == 0 ? l->scale : 1.0f;
		Texture2D tex = g_tag_texture[d.tag];
		float origin = g_tag_origin[d.tag];
		DrawTexturePro(tex, (Rectangle){0.0f, 0.0f, tex.width, tex.height},
			(Rectangle){d.x, d.y, tex.width*scale, tex.height*scale},
			(Vector2){0.0f, origin}, rotation, WHITE);
	}
}

static _Bool verify_level(level const *l) {
	int n_ok = 0;
	for (int i = 0; i < l->n_dps; i++) {
		droppoint *dp = l->dps + i;
		_Bool found = false;
		for (int j = 0; j < l->n_drags; j++) {
			drag *d = l->drags + j;
			_Bool sametag = dp->tag == TAG_NONE || dp->tag == d->tag;
			if (dp->x == d->x && dp->y == d->y && sametag) {
				found = true;
				break;
			}
		}
		n_ok += dp->tag != TAG_NONE ? found : !found;
	}
	return n_ok == l->expect;
}

static void position_drags(level const *l, float y) {
	float w = 0.0f;
	for (int i = 0; i < l->n_drags; i++) {
		w += g_tag_texture[l->drags[i].tag].width * 1.5f;
	}
	float xstart = (VWIDTH - w) / 2.0f;
	for (int i = 0; i < l->n_drags; i++) {
		drag *d = l->drags + i;
		float xdiff = g_tag_texture[d->tag].width * 1.5f;
		d->x = xstart;
		d->y = y;
		xstart += xdiff;
	 }
}

enum { BT_INIT, BT_MENU, BT_ZEN, BT_HOWTO };
static _Bool button(Vector2 pos, Texture2D map, int id, Vector2 mouse) {
	_Bool clicked = 0, down = 0;
	Rectangle bounds = (Rectangle) { pos.x, pos.y, 200.0f, 50.0f };
	if (CheckCollisionPointRec(mouse, bounds)) {
		down = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
		clicked = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
	}
	Rectangle source = { down*200.0f, 50.0f*id, 200.0f, 50.0f };
	DrawTextureRec(map, source, pos, WHITE);
	return clicked;
}

int main() {
	srand(time(0));

	InitWindow(VWIDTH, VHEIGHT, "ChemIO");
	SetWindowMinSize(VWIDTH, VHEIGHT);
	SetWindowState(FLAG_WINDOW_RESIZABLE);
	// SetExitKey(KEY_NULL);

	Texture2D background_tex = LoadTexture("./res/bg.png");
	Texture2D buttons_tex = LoadTexture("./res/buttons.png");
	Texture2D title_tex = LoadTexture("./res/titulo.png");

	g_tag_texture[TAG_C] = LoadTexture("./res/tag_c.png");
	g_tag_origin[TAG_C] = 32.0f;
	g_tag_texture[TAG_O] = LoadTexture("./res/tag_o.png");
	g_tag_origin[TAG_O] = 32.0f;
	g_tag_texture[TAG_2O] = LoadTexture("./res/tag_2o.png");
	g_tag_origin[TAG_2O] = 32.0f;
	g_tag_texture[TAG_N] = LoadTexture("./res/tag_n.png");
	g_tag_origin[TAG_N] = 32.0f;

	static level levels[16];
	level *lvl = levels;
	// hidrocarbonetos
	*lvl++ = (level){
		.name = "metil-propano",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_metil_propano.png"),
		.dps = (droppoint[]) {
			{304.0f, 240.0f, 270.0f, TAG_NONE},
			{400.0f, 240.0f, 270.0f, TAG_C},
			{496.0f, 240.0f, 270.0f, TAG_NONE},
		},
		.n_dps = 3,
		.expect = 3,
		.drags = (drag[]) {{TAG_C}, {TAG_C}},
		.n_drags = 2,
	};
	*lvl++ = (level){
		.name = "but-2-eno",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_but_2_eno.png"),
		.dps = (droppoint[]) {
			{223.0f, 303.0f, 180.0f, TAG_NONE},
			{352.0f, 337.0f,  90.0f, TAG_NONE},
			{480.0f, 303.0f,   0.0f, TAG_C},
		},
		.n_dps = 3,
		.expect = 3,
		.drags = (drag[]) {{TAG_C}, {TAG_C}},
		.n_drags = 2,
	};
	*lvl++ = (level){
		.name = "etil-cicloexano",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_etil_cicloexano.png"),
		.dps = (droppoint[]) {
			{320.0f, 204.0f, 270.0f, TAG_NONE},
			{220.0f, 255.0f, 225.0f, TAG_NONE},
			{220.0f, 370.0f, 135.0f, TAG_NONE},
			{320.0f, 425.0f,  90.0f, TAG_NONE},
			{424.0f, 370.0f,  45.0f, TAG_NONE},
			{500.0f, 226.0f,  45.0f, TAG_C},
		},
		.n_dps = 6,
		.expect = 6,
		.drags = (drag[]) {{TAG_C}, {TAG_2O}},
		.n_drags = 2,
	};
	// cetonas
	*lvl++ = (level){
		.name = "4-metil-pentan-2-ona",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_4_metil_pentan_2_ona.png"),
		.dps = (droppoint[]) {
			{208.0f, 271.0f, 270.0f, TAG_NONE},
			{304.0f, 175.0f, 270.0f, TAG_NONE},
			{400.0f, 271.0f, 270.0f, TAG_NONE},
			{496.0f, 271.0f, 270.0f, TAG_2O},
			{592.0f, 271.0f, 270.0f, TAG_NONE},
		},
		.n_dps = 5,
		.expect = 5,
		.drags = (drag[]) {{TAG_C}, {TAG_2O}, {TAG_O}},
		.n_drags = 3,
	};
	*lvl++ = (level){
		.name = "propanona",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_propanona.png"),
		.dps = (droppoint[]) {
			{304.0f, 240.0f, 270.0f, TAG_NONE},
			{400.0f, 240.0f, 270.0f, TAG_2O},
			{496.0f, 240.0f, 270.0f, TAG_NONE},
		},
		.n_dps = 3,
		.expect = 3,
		.drags = (drag[]) {{TAG_2O}, {TAG_C}},
		.n_drags = 2,
	};
	*lvl++ = (level){
		.name = "3,4-dimetil-heptan-2-ona",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_3_4_dimetil_heptan_2_ona.png"),
		.dps = (droppoint[]) {
			{304.0f, 143.0f, 270.0f, TAG_NONE},
			{336.0f, 176.0f,   0.0f, TAG_NONE},
			{400.0f, 239.0f, 270.0f, TAG_NONE},
			{496.0f, 239.0f, 270.0f, TAG_NONE},
			{592.0f, 239.0f, 270.0f, TAG_NONE},
			{624.0f, 272.0f,   0.0f, TAG_NONE},
			{592.0f, 304.0f,  90.0f, TAG_NONE},
			{496.0f, 304.0f,  90.0f, TAG_NONE},
			{400.0f, 304.0f,  90.0f, TAG_NONE},
			{304.0f, 304.0f,  90.0f, TAG_NONE},
			{240.0f, 368.0f,   0.0f, TAG_2O},
			{240.0f, 464.0f,   0.0f, TAG_NONE},
			{208.0f, 496.0f,  90.0f, TAG_NONE},
			{175.0f, 464.0f, 180.0f, TAG_NONE},
			{175.0f, 368.0f, 180.0f, TAG_2O},
			{112.0f, 304.0f,  90.0f, TAG_NONE},
			{ 79.0f, 272.0f, 180.0f, TAG_NONE},
			{112.0f, 239.0f, 270.0f, TAG_NONE},
			{208.0f, 239.0f, 270.0f, TAG_NONE},
			{271.0f, 176.0f, 180.0f, TAG_NONE},
		},
		.n_dps = 20,
		.expect = 19,
		.drags = (drag[]) {{TAG_2O}, {TAG_2O}},
		.n_drags = 2,
	};
	// aldeídos
	*lvl++ = (level){
		.name = "propanal",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_propanal.png"),
		.dps = (droppoint[]) {
			{304.0f, 240.0f, 270.0f, TAG_2O},
			{400.0f, 240.0f, 270.0f, TAG_NONE},
			{496.0f, 240.0f, 270.0f, TAG_2O},
		},
		.n_dps = 3,
		.expect = 2,
		.drags = (drag[]) {{TAG_2O}, {TAG_2O}},
		.n_drags = 2,
	};
	*lvl++ = (level){
		.name = "2-metil-but-3-enal",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_2_metil_but_3_enal.png"),
		.dps = (droppoint[]) {
			{208.0f, 270.0f, 270.0f, TAG_2O},
			{304.0f, 270.0f, 270.0f, TAG_C},
			{400.0f, 270.0f, 270.0f, TAG_NONE},
			{496.0f, 270.0f, 270.0f, TAG_NONE},
		},
		.n_dps = 4,
		.expect = 4,
		.drags = (drag[]) {{TAG_2O}, {TAG_2O}, {TAG_C}},
		.n_drags = 3,
	};
	// fenol
	*lvl++ = (level){
		.name = "meta-metil-fenol",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_meta_metil_fenol.png"),
		.dps = (droppoint[]) {
			{286.0f, 144.0f, 180.0f, TAG_NONE},
			{220.0f, 255.0f, 225.0f, TAG_NONE},
			{220.0f, 370.0f, 135.0f, TAG_NONE},
			{320.0f, 425.0f,  90.0f, TAG_NONE},
			{424.0f, 370.0f,  45.0f, TAG_C},
			{422.0f, 264.0f, 315.0f, TAG_NONE},
		},
		.n_dps = 6,
		.expect = 6,
		.drags = (drag[]) {{TAG_C}, {TAG_2O}, {TAG_C}},
		.n_drags = 3,
	};
	// alcool
	*lvl++ = (level){
		.name = "etanol",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_etanol.png"),
		.dps = (droppoint[]) {
			{315.0f, 287.0f, 180.0f, TAG_C},
			{476.0f, 287.0f,   0.0f, TAG_NONE},
		},
		.n_dps = 2,
		.expect = 2,
		.drags = (drag[]) {{TAG_C}, {TAG_2O}, {TAG_C}, {TAG_2O}},
		.n_drags = 4,
	};
	*lvl++ = (level){
		.name = "4-metil-pentan-2-ol",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_4_metil_pentan_2_ol.png"),
		.dps = (droppoint[]) {
			{208.0f, 271.0f, 270.0f, TAG_NONE},
			{304.0f, 175.0f, 270.0f, TAG_NONE},
			{400.0f, 271.0f, 270.0f, TAG_NONE},
			{496.0f, 271.0f, 270.0f, TAG_O},
			{592.0f, 271.0f, 270.0f, TAG_NONE},
		},
		.n_dps = 5,
		.expect = 5,
		.drags = (drag[]) {{TAG_C}, {TAG_2O}, {TAG_O}},
		.n_drags = 3,
	};
	// éster
	*lvl++ = (level){
		.name = "propanoato de propila",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_propanoato_de_propila.png"),
		.dps = (droppoint[]) {
			{159.0f, 352.0f, 180.0f, TAG_C},
			{288.0f, 222.0f, 270.0f, TAG_NONE},
			{384.0f, 318.0f, 270.0f, TAG_NONE},
			{480.0f, 318.0f, 270.0f, TAG_NONE},
			{608.0f, 352.0f,   0.0f, TAG_C},
		},
		.n_dps = 5,
		.expect = 5,
		.drags = (drag[]) {{TAG_C}, {TAG_2O}, {TAG_O}, {TAG_C}},
		.n_drags = 4,
	};
	// amina
	*lvl++ = (level){
		.name = "butan-2-amina",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_butan_2_amina.png"),
		.dps = (droppoint[]) {
			{240.0f, 287.0f, 270.0f, TAG_NONE},
			{336.0f, 287.0f, 270.0f, TAG_N},
			{432.0f, 287.0f, 270.0f, TAG_N},
			{528.0f, 287.0f, 270.0f, TAG_NONE},
		},
		.n_dps = 4,
		.expect = 3,
		.drags = (drag[]) {{TAG_C}, {TAG_2O}, {TAG_N}},
		.n_drags = 3,
	};
	*lvl++ = (level){
		.name = "N-metil-etanamina",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_n_metil_etanamina.png"),
		.dps = (droppoint[]) {
			{303.0f, 320.0f, 180.0f, TAG_C},
			{464.0f, 320.0f,   0.0f, TAG_C},
		},
		.n_dps = 2,
		.expect = 2,
		.drags = (drag[]) {{TAG_C}, {TAG_C}, {TAG_N}},
		.n_drags = 3,
	};
	// amida
	*lvl++ = (level){
		.name = "propanamida",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_propanamida.png"),
		.dps = (droppoint[]) {
			{255.0f, 336.0f, 180.0f, TAG_NONE},
			{384.0f, 369.0f,  90.0f, TAG_NONE},
			{512.0f, 336.0f,   0.0f, TAG_N},
			{480.0f, 207.0f, 270.0f, TAG_NONE},
		},
		.n_dps = 4,
		.expect = 4,
		.drags = (drag[]) {{TAG_O}, {TAG_C}, {TAG_N}},
		.n_drags = 3,
	};
	*lvl++ = (level){
		.name = "2-metil-propanamida",
		.scale = 2.0f,
		.base = LoadTexture("./res/tst_2_metil_propanamida.png"),
		.dps = (droppoint[]) {
			{255.0f, 336.0f, 180.0f, TAG_NONE},
			{384.0f, 369.0f,  90.0f, TAG_C},
			{512.0f, 336.0f,   0.0f, TAG_N},
			{480.0f, 207.0f, 270.0f, TAG_NONE},
		},
		.n_dps = 4,
		.expect = 4,
		.drags = (drag[]) {{TAG_O}, {TAG_C}, {TAG_N}},
		.n_drags = 3,
	};
	if (lvl != levels + countof(levels)) {
		printf("level length mismatch: %d\n", (int)(lvl - levels + 1));
		return 0;
	}
	lvl = levels + (rand() % (countof(levels)-1));
	position_drags(lvl, 500.0f);

	RenderTexture2D framebuf = LoadRenderTexture(VWIDTH, VHEIGHT);

	enum {
		ST_MENU,
		ST_PLAY,
		ST_TRUE,
		ST_HOWTO,
		ST_END,
	} state = ST_MENU;
	float remaining = REMAINING;
	float time = NAN;
	drag *hold = 0;
	int pts = 0;
	SetTargetFPS(60);
	while (!WindowShouldClose()) {
		Vector2 mouse = GetMousePosition();
		mouse.x = mouse.x * VWIDTH / GetScreenWidth();
		mouse.y = mouse.y * VHEIGHT / GetScreenHeight();

		switch (state) {
		case ST_MENU: {
			BeginTextureMode(framebuf);
			DrawTexture(background_tex, 0, 0, WHITE);
			DrawTexture(title_tex, 0, 0, WHITE);

			pts = 0;
			if (button((Vector2) { 67.0f, 500.0f }, buttons_tex, BT_INIT, mouse)) {
				state = ST_PLAY;
				remaining = REMAINING;
			}
			if (button((Vector2) { 284.0f, 500.0f }, buttons_tex, BT_ZEN, mouse)) {
				state = ST_PLAY;
				remaining = NAN;
			}
			if (button((Vector2) { 501.0f, 500.0f }, buttons_tex, BT_HOWTO, mouse)) {
				state = ST_HOWTO;
			}
			
			EndTextureMode();
		} break;
		case ST_PLAY: {
			if (verify_level(lvl) && !hold) {
				state = ST_TRUE;
			}

			remaining -= GetFrameTime();
			if (!isnan(remaining) && remaining <= 0.0f) {
				state = ST_END;
			}
			
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
				float nearest = 0.0f;
				for (int i = 0; i < lvl->n_drags; i++) {
					drag *it = lvl->drags + i;
					float dist = powf(mouse.x-it->x, 2) + powf(mouse.y-it->y, 2);
					if (dist < nearest || !hold) {
						hold = it;
						nearest = dist;
					}
				}
			}
			if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
				hold = 0;
			}
			if (hold) {
				float dist = 0.0f;
				droppoint *dp = nearest_droppoint(lvl, mouse.x, mouse.y, &dist);
				hold->x = (dist < 30.0f) ? dp->x : mouse.x;
				hold->y = (dist < 30.0f) ? dp->y : mouse.y;
			}

			BeginTextureMode(framebuf);
			DrawTexture(background_tex, 0, 0, WHITE);
			draw_level(lvl, remaining);
			EndTextureMode();
		} break;
		case ST_TRUE: {
			if (isnan(time)) {
				pts++;
				hold = 0;
				if (!isnan(remaining)) {
					remaining = fminf(remaining + REMAINING/2, REMAINING*3);
				}
				position_drags(lvl, 500.0f);
				time = 1.0f;
			}
			time -= GetFrameTime();
			if (time <= 0.0f) {
				time = NAN;
				level *last = lvl;
				do {
					lvl = levels + (rand() % (countof(levels)-1));
				} while(lvl == last);
				state = ST_PLAY;
				position_drags(lvl, 520.0f);
			}
		
			BeginTextureMode(framebuf);
			DrawRectangle(0, 0, VWIDTH, VHEIGHT, (Color){ 0, 228, 48, 255/10 });
			EndTextureMode();
		} break;
		case ST_HOWTO: {
			BeginTextureMode(framebuf);
			DrawTexture(background_tex, 0, 0, WHITE);
			char const *text = "O jogo possui dois modos: Modo normal e Modo Zen.\n\nNo modo normal, o jogador deve montar as moléculas\naparecem na parte de cima do quadro, antes que o\ntempo acabe, caso o jogador acerte a molécula, ele\nganha um pouco mais de tempo para a próxima rodada,\ncaso ele erre, o jogo acaba e exibe a quantidade de\npontos que ele fez baseado na quantidade de moléculas\nque ele conseguiu montar antes do tempo acabar.\n\nNo modo Zen as moléculas são exibidas aleatoriamente\nda mesma forma, porém sem tempo, para que o jogador\npossa aprender sem a pressão do contador.";
			enum { FONTSIZE = 20 };
			int measure = MeasureText(text, FONTSIZE);
			DrawText(text, (VWIDTH-measure)/2, 50, FONTSIZE, BLACK);
			if (button((Vector2) {300.0f, 500.0f}, buttons_tex, BT_MENU, mouse)) {
				state = ST_MENU;
			}
			EndTextureMode();
		} break;
		case ST_END: {
			BeginTextureMode(framebuf);
			ClearBackground(RED);
			static char text[0xff];
			snprintf(text, sizeof(text), "Tempo esgotado!!\n%d pontos", pts);
			enum { FONTSIZE = 64 };
			int measure = MeasureText(text, FONTSIZE);
			DrawText(text, (VWIDTH-measure)/2, 200, FONTSIZE, BLACK);
			if (button((Vector2) {300.0f, 500.0f}, buttons_tex, BT_MENU, mouse)) {
				state = ST_MENU;
				lvl = levels + (rand() % (countof(levels)-1));
				position_drags(lvl, 500.0f);
			}
			EndTextureMode();
		} break;
		}

		BeginDrawing();
		ClearBackground(BLACK);
		DrawTexturePro(framebuf.texture, (Rectangle){0.0f, 0.0f, VWIDTH, -VHEIGHT},
			(Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()}, (Vector2){0, 0}, 0.0f, WHITE);
		EndDrawing();
	}
	CloseWindow();
	return 0;
}
