/*! \file level.c
 *  \brief Playable Level code.
 */
/*
 * Copyright (c) Sebastian Krzyszkowiak <dos@dosowisko.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include <stdio.h>
#include "moonwalk.h"
#include "config.h"
#include "pause.h"
#include "level.h"
#include "timeline.h"

/* TODO: check if spritesheet speed still depends on FPS */

void SelectDerpySpritesheet(struct Game *game, char* name) {
	struct Spritesheet *tmp = game->level.derpy_sheets;
	if (!tmp) {
		PrintConsole(game, "ERROR: No spritesheets registered for Derpy!");
		return;
	}
	while (tmp) {
		if (!strcmp(tmp->name, name)) {
			PrintConsole(game, "Selecting Derpy spritesheet: %s", name);
			game->level.derpy_sheet = &(tmp->bitmap);
			game->level.sheet_rows = tmp->rows;
			game->level.sheet_cols = tmp->cols;
			game->level.sheet_blanks = tmp->blanks;
			game->level.sheet_speed_modifier = tmp->speed;
			game->level.sheet_pos = 0;
			game->level.sheet_scale = tmp->scale;
			if (game->level.derpy) al_destroy_bitmap(game->level.derpy);
			game->level.derpy = al_create_bitmap(al_get_display_height(game->display)*0.25*tmp->aspect*tmp->scale, al_get_display_height(game->display)*0.25*tmp->scale);
			return;
		}
		tmp = tmp->next;
	}
	PrintConsole(game, "ERROR: No spritesheets registered for Derpy with given name: %s", name);
	return;
}

void RegisterDerpySpritesheet(struct Game *game, char* name) {
	char filename[255] = { };
	sprintf(filename, "data/levels/derpy/%s.ini", name);
	ALLEGRO_CONFIG *config = al_load_config_file(filename);
	struct Spritesheet *s = malloc(sizeof(struct Spritesheet));
	s->name = malloc((strlen(name)+1)*sizeof(char));
	strcpy(s->name, name);
	s->bitmap = NULL;
	s->cols = atoi(al_get_config_value(config, "", "cols"));
	s->rows = atoi(al_get_config_value(config, "", "rows"));
	s->blanks = atoi(al_get_config_value(config, "", "blanks"));
	s->speed = atof(al_get_config_value(config, "", "speed"));
	s->aspect = atof(al_get_config_value(config, "", "aspect"));
	s->scale = atof(al_get_config_value(config, "", "scale"));
	s->next = game->level.derpy_sheets;
	game->level.derpy_sheets = s;
	al_destroy_config(config);
}

void Level_Passed(struct Game *game) {
	if (game->level.current_level<6) {
		int available = atoi(GetConfigOptionDefault("MuffinAttack", "level", "1"));
		available++;
		if ((available<2) || (available>7)) available=1;
		if (available==(game->level.current_level+1)) {
			char* text = malloc(2*sizeof(char));
			sprintf(text, "%d", available);
			SetConfigOption("MuffinAttack", "level", text);
			free(text);
		}
	} else {
		SetConfigOption("MuffinAttack", "completed", "1");
	}
}

bool Accelerate(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	if (state != TM_ACTIONSTATE_RUNNING) return false;
	game->level.speed+=0.000005;
	if (game->level.speed<0.0020) return false;
	return true;
}

bool Walk(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	if (state == TM_ACTIONSTATE_INIT) action->arguments = NULL;
	if (state != TM_ACTIONSTATE_RUNNING) return false;
	if (!(action->arguments)) SelectDerpySpritesheet(game, "walk");
	action->arguments++;
	game->level.derpy_x+=tps(game, 60*0.001);
	if (game->level.derpy_x<0.05) return false;
	return true;
}

bool Move(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	if (state != TM_ACTIONSTATE_RUNNING) return false;
	game->level.speed=0.00035;
	if (game->level.st_pos<0.275) return false;
	return true;
}

bool ShowMeter(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	if (state != TM_ACTIONSTATE_RUNNING) return false;
	game->level.meter_alpha+=tps(game, 60*4);
	if (game->level.meter_alpha>=255) {
		game->level.meter_alpha=255;
		return true;
	}
	return false;
}

bool Fly(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	if (state == TM_ACTIONSTATE_INIT) action->arguments = NULL;
	if (state != TM_ACTIONSTATE_RUNNING) return false;
	if (!(action->arguments)) {
		SelectDerpySpritesheet(game, "fly");
		game->level.flying = true;
		TM_AddBackgroundAction(&ShowMeter, NULL, 0, "showmeter");
	}
	action->arguments++;
	game->level.derpy_y-=tps(game, 60*0.004);
	if (game->level.derpy_y>0.2) return false;
	game->level.handle_input=true;
	return true;
}

void Obst_MoveUpDown(struct Game *game, struct Obstracle *obstracle) {
	if (obstracle->data) {
		obstracle->y -= 0.5;
		if (obstracle->y<=0) {
			obstracle->data=NULL;
		}
	} else {
		obstracle->y += 0.5;
		if (obstracle->y>=100) {
			obstracle->data++;
		}
	}
}

bool GenerateObstracles(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	/*float* tmp; bool* in;*/
	int* count;
	if (!action->arguments) {
		action->arguments = TM_AddToArgs(action->arguments, malloc(sizeof(int)));
		/* action->arguments = TM_AddToArgs(action->arguments, malloc(sizeof(bool))); */
	}
	count = (int*)action->arguments->value;
	/*tmp = (float*)action->arguments->value;
	in = (bool*)action->arguments->next->value;*/
	if (state == TM_ACTIONSTATE_INIT) {
		*count = 0;
		/* *tmp = 0;
		*in = true;*/
	}
	else if (state == TM_ACTIONSTATE_RUNNING) {
		if (rand()%(10000/(int)tps(game, 60*100))<=2) {
			PrintConsole(game, "OBSTRACLE %d", *count);
			(*count)++;
			struct Obstracle *obst = malloc(sizeof(struct Obstracle));
			obst->prev = NULL;
			obst->x = 100;
			obst->y = (rand()%91)-1;
			obst->speed = 0;
			obst->bitmap = &(game->level.obst_bmps.pie);
			obst->callback = NULL;
			obst->data = (void*)(rand()%2);
			if (rand()%100<=50) obst->callback=Obst_MoveUpDown;
			if (game->level.obstracles) {
				game->level.obstracles->prev = obst;
				obst->next = game->level.obstracles;
			} else {
				obst->next = NULL;
			}
			game->level.obstracles = obst;
			if (*count > 64) return true;
		}
	} else {
		free(action->arguments->value);
		TM_DestroyArgs(action->arguments);
		action->arguments = NULL;
	}
	return false;
}

bool Stop(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	if (state != TM_ACTIONSTATE_RUNNING) return false;
	game->level.speed=0;
	SelectDerpySpritesheet(game, "stand");
	/*game->level.sheet_speed = 0;*/
	return true;
}

bool Letter(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	if (state != TM_ACTIONSTATE_RUNNING) return false;
	al_draw_text_with_shadow(game->menu.font_title, al_map_rgb(255,255,255), al_get_display_width(game->display)*0.5, al_get_display_height(game->display)*0.45, ALLEGRO_ALIGN_CENTRE, "Letter from Twilight");
	struct ALLEGRO_KEYBOARD_STATE keyboard;
	al_get_keyboard_state(&keyboard);
	if (al_key_down(&keyboard, ALLEGRO_KEY_ENTER)) {
		return true;
	}
	return false;
}

void Level_Draw(struct Game *game) {
	if (!al_get_sample_instance_playing(game->level.music) && (game->loadstate==GAMESTATE_LEVEL)) {
		al_set_sample_instance_playing(game->level.music, true);
		al_set_sample_instance_position(game->level.music, game->level.music_pos);
	}
	if (game->level.current_level!=1) Moonwalk_Draw(game);
	else {

		struct ALLEGRO_KEYBOARD_STATE keyboard;
		al_get_keyboard_state(&keyboard);
		if (game->level.handle_input) {
			if (al_key_down(&keyboard, ALLEGRO_KEY_UP)) {
				game->level.derpy_y -= tps(game, 60*0.005);
				/*PrintConsole(game, "Derpy Y position: %f", game->level.derpy_y);*/
			}
			if (al_key_down(&keyboard, ALLEGRO_KEY_DOWN)) {
				game->level.derpy_y += tps(game, 60*0.005);
				/*PrintConsole(game, "Derpy Y position: %f", game->level.derpy_y);*/
			}
			if ((game->level.derpy_y > 0.6) && (game->level.flying)) {
				SelectDerpySpritesheet(game, "run");
				game->level.flying = false;
				game->level.sheet_speed = tps(game, 60*0.0020/game->level.speed);
			}
			else if ((game->level.derpy_y <= 0.6) && (!game->level.flying)) {
				SelectDerpySpritesheet(game, "fly");
				game->level.flying = true;
				game->level.sheet_speed = tps(game, 60*2.4);
			}
			if (!game->level.flying) game->level.sheet_speed = tps(game, 60*0.0020/game->level.speed);
			if (game->level.derpy_y < 0) game->level.derpy_y=0;
			else if (game->level.derpy_y > 0.8) game->level.derpy_y=0.8;
		}
		al_hold_bitmap_drawing(true);

		al_draw_bitmap(game->level.clouds, (-game->level.cl_pos)*al_get_bitmap_width(game->level.clouds), 0, 0);
		al_draw_bitmap(game->level.clouds, (1+(-game->level.cl_pos))*al_get_bitmap_width(game->level.clouds), 0, 0);
		al_draw_bitmap(game->level.background, (-game->level.bg_pos)*al_get_bitmap_width(game->level.background), 0, 0);
		al_draw_bitmap(game->level.background, (1+(-game->level.bg_pos))*al_get_bitmap_width(game->level.background), 0, 0);
		al_draw_bitmap(game->level.stage, (-game->level.st_pos)*al_get_bitmap_width(game->level.stage), 0 ,0);
		al_draw_bitmap(game->level.stage, (1+(-game->level.st_pos))*al_get_bitmap_width(game->level.stage), 0 ,0);

		int derpyx = game->level.derpy_x*al_get_display_width(game->display);
		int derpyy = game->level.derpy_y*al_get_display_height(game->display);
		int derpyw = al_get_bitmap_width(game->level.derpy);
		int derpyh = al_get_bitmap_height(game->level.derpy);
		bool colision = false;
		struct Obstracle *tmp = game->level.obstracles;
		while (tmp) {
			/*PrintConsole(game, "DRAWING %f %f", tmp->x, tmp->y);*/
			if (tmp->x > -10) {
				bool col = false;
				int x = (tmp->x/100.0)*al_get_display_width(game->display);
				int y = (tmp->y/100.0)*al_get_display_height(game->display);
				int w = al_get_bitmap_width(*(tmp->bitmap));
				int h = al_get_bitmap_height(*(tmp->bitmap));
				if ((((x>=derpyx) && (x<=derpyx+derpyw)) || ((x+w>=derpyx) && (x+w<=derpyx+derpyw))) &&
					(((y>=derpyy) && (y<=derpyy+derpyh)) || ((y+h>=derpyy) && (y+h<=derpyy+derpyh))))
					  col = true;
				al_draw_tinted_bitmap(*(tmp->bitmap), al_map_rgba(255,255-col*255,255-col*255,255), x, y, 0);
				if (col) colision = true;
				tmp->x -= tps(game, game->level.speed*60)*310;
				if (tmp->callback) tmp->callback(game, tmp);
				tmp = tmp->next;
			} else {
				if (tmp->next)
					tmp->next->prev = tmp->prev;
				if (tmp->prev)
					tmp->prev->next = tmp->next;
				else
					game->level.obstracles = tmp->next;
				struct Obstracle *t = tmp;
				tmp = tmp->next;
				free(t);
			}
		}
		if (colision) game->level.hp-=0.0005;
		al_hold_bitmap_drawing(false);

		al_set_target_bitmap(game->level.derpy);
		al_clear_to_color(al_map_rgba(0,0,0,0));
		al_draw_bitmap_region(*(game->level.derpy_sheet),al_get_bitmap_width(game->level.derpy)*(game->level.sheet_pos%game->level.sheet_cols),al_get_bitmap_height(game->level.derpy)*(game->level.sheet_pos/game->level.sheet_cols),al_get_bitmap_width(game->level.derpy), al_get_bitmap_height(game->level.derpy),0,0,0);
		if ((game->level.sheet_speed) && (game->level.sheet_speed_modifier)) {
			game->level.sheet_tmp+=tps(game, 60);
			if (game->level.sheet_tmp >= game->level.sheet_speed/game->level.sheet_speed_modifier) {
				game->level.sheet_pos++;
				game->level.sheet_tmp = 0;
			}
			if (game->level.sheet_pos>=game->level.sheet_cols*game->level.sheet_rows-game->level.sheet_blanks) game->level.sheet_pos=0;
		}
		al_set_target_bitmap(al_get_backbuffer(game->display));
		al_hold_bitmap_drawing(true);
		
		al_draw_tinted_bitmap(game->level.derpy, al_map_rgba(255,255-colision*255,255-colision*255,255), derpyx+al_get_display_width(game->display)*0.1953125-al_get_bitmap_width(game->level.derpy), derpyy, 0);

		al_draw_bitmap(game->level.foreground, (-game->level.fg_pos)*al_get_bitmap_width(game->level.foreground), 0 ,0);
		al_draw_bitmap(game->level.foreground, (1+(-game->level.fg_pos))*al_get_bitmap_width(game->level.foreground), 0 ,0);

		if (game->level.speed > 0) {
			game->level.cl_pos += tps(game, 60*game->level.speed) * 0.2;
			game->level.bg_pos += tps(game, 60*game->level.speed) * 0.6;
			game->level.st_pos += tps(game, 60*game->level.speed) * 1;
			game->level.fg_pos += tps(game, 60*game->level.speed) * 1.75;
			if (game->level.bg_pos >= 1) game->level.bg_pos=game->level.bg_pos-1;
			if (game->level.st_pos >= 1) game->level.st_pos=game->level.st_pos-1;
			if (game->level.fg_pos >= 1) game->level.fg_pos=game->level.fg_pos-1;
		}
		game->level.cl_pos += tps(game, 60*0.00005);
		if (game->level.cl_pos >= 1) game->level.cl_pos=game->level.cl_pos-1;
		al_hold_bitmap_drawing(false);

		al_set_target_bitmap(game->level.meter_bmp);
		al_clear_to_color(al_map_rgba(0,0,0,0));
		al_draw_filled_rounded_rectangle(al_get_bitmap_width(game->level.meter_bmp)*0.1, al_get_bitmap_height(game->level.meter_bmp)*0.3, al_get_bitmap_width(game->level.meter_bmp), al_get_bitmap_height(game->level.meter_bmp)*0.7,
																		 7.5,7.5, al_map_rgb(232,234,239));
		al_draw_horizontal_gradient_rect(al_get_bitmap_width(game->level.meter_bmp)-al_get_display_width(game->display)*0.215, (al_get_bitmap_height(game->level.meter_bmp)-al_get_display_height(game->display)*0.025)/2, al_get_display_width(game->display)*0.215*0.975, al_get_display_height(game->display)*0.025, al_map_rgb(150,159,182), al_map_rgb(130,139,162));
		al_draw_filled_rectangle(al_get_bitmap_width(game->level.meter_bmp)-al_get_display_width(game->display)*0.215, (al_get_bitmap_height(game->level.meter_bmp)-al_get_display_height(game->display)*0.025)/2, al_get_bitmap_width(game->level.meter_bmp)-al_get_display_width(game->display)*0.215+(al_get_display_width(game->display)*0.215*0.975)*game->level.hp, (al_get_bitmap_height(game->level.meter_bmp)-al_get_display_height(game->display)*0.025)/2+al_get_display_height(game->display)*0.025, al_map_rgb(214,172,55));
		al_draw_bitmap(game->level.meter_image, 0, 0, 0);
		al_set_target_bitmap(al_get_backbuffer(game->display));

		al_draw_tinted_bitmap(game->level.meter_bmp, al_map_rgba(game->level.meter_alpha,game->level.meter_alpha,game->level.meter_alpha,game->level.meter_alpha), al_get_display_width(game->display)*0.95-al_get_bitmap_width(game->level.meter_bmp), al_get_display_height(game->display)*0.975-al_get_bitmap_height(game->level.meter_bmp), 0);

		TM_Process();
	}
}

bool FadeIn(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	if (!action->arguments) {
		action->arguments = TM_AddToArgs(action->arguments, malloc(sizeof(float)));
		action->arguments = TM_AddToArgs(action->arguments, (void*)al_create_bitmap(al_get_display_width(game->display), al_get_display_height(game->display)));
	}
	float* fadeloop;
	ALLEGRO_BITMAP* fade_bitmap;
	fadeloop = (float*)action->arguments->value;
	fade_bitmap = (ALLEGRO_BITMAP*)action->arguments->next->value;
	if (state == TM_ACTIONSTATE_INIT) {
		*fadeloop = 255;
		al_set_target_bitmap(fade_bitmap);
		al_clear_to_color(al_map_rgb(0,0,0));
		al_set_target_bitmap(al_get_backbuffer(game->display));
	} else if (state == TM_ACTIONSTATE_RUNNING) {
		al_draw_tinted_bitmap(fade_bitmap,al_map_rgba_f(1,1,1,*fadeloop/255.0),0,0,0);
		*fadeloop-=tps(game, 600);
		if (*fadeloop<=0) return true;
	} else {
		al_destroy_bitmap(fade_bitmap);
		free(fadeloop);
		TM_DestroyArgs(action->arguments);
		action->arguments = NULL;
		al_play_sample_instance(game->level.music);
	}
	return false;
}

bool FadeOut(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	if (!action->arguments) {
		action->arguments = TM_AddToArgs(action->arguments, malloc(sizeof(float)));
		action->arguments = TM_AddToArgs(action->arguments, (void*)al_create_bitmap(al_get_display_width(game->display), al_get_display_height(game->display)));
	}
	float* fadeloop;
	ALLEGRO_BITMAP* fade_bitmap;
	fadeloop = (float*)action->arguments->value;
	fade_bitmap = (ALLEGRO_BITMAP*)action->arguments->next->value;
	if (state == TM_ACTIONSTATE_INIT) {
		*fadeloop = 0;
		al_set_target_bitmap(fade_bitmap);
		al_clear_to_color(al_map_rgb(0,0,0));
		al_set_target_bitmap(al_get_backbuffer(game->display));
	} else if (state == TM_ACTIONSTATE_RUNNING) {
		al_draw_tinted_bitmap(fade_bitmap,al_map_rgba_f(1,1,1,*fadeloop/255.0),0,0,0);
		*fadeloop+=tps(game, 600);
		if (*fadeloop>=256) return true;
	} else {
		al_destroy_bitmap(fade_bitmap);
		free(fadeloop);
		Level_Unload(game);
		game->gamestate = GAMESTATE_LOADING;
		game->loadstate = GAMESTATE_MAP;
		TM_DestroyArgs(action->arguments);
		action->arguments = NULL;
	}
	return false;
}

bool Welcome(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	float* tmp; bool* in;
	if (!action->arguments) {
		action->arguments = TM_AddToArgs(action->arguments, malloc(sizeof(float)));
		action->arguments = TM_AddToArgs(action->arguments, malloc(sizeof(bool)));
	}
	tmp = (float*)action->arguments->value;
	in = (bool*)action->arguments->next->value;
	if (state == TM_ACTIONSTATE_INIT) {
		*tmp = 0;
		*in = true;
		/*PrintConsole(game, "WELCOME INIT");*/
	}
	else if (state == TM_ACTIONSTATE_RUNNING) {
		/*PrintConsole(game, "WELCOME RUNNING FADE=%f, IN=%d", *in); */
		float fade = *tmp;
		if (fade>255) fade=255;
		if (*tmp > 2048) { *tmp=255; *in=false; }
		al_draw_tinted_bitmap(game->level.welcome, al_map_rgba_f(fade/255.0,fade/255.0,fade/255.0,fade/255.0), 0, 0, 0);
		if (*in) {
			*tmp+=tps(game, 600);
		} else {
			*tmp-=tps(game, 600);
			if (*tmp<=0) { return true; }
		}
	} else {
		free(action->arguments->value);
		free(action->arguments->next->value);
		TM_DestroyArgs(action->arguments);
		action->arguments = NULL;
	}
	return false;
}

bool PassLevel(struct Game *game, struct TM_Action *action, enum TM_ActionState state) {
	if (state == TM_ACTIONSTATE_DESTROY) {
		Level_Passed(game);
		TM_AddBackgroundAction(&FadeOut, NULL, 0, "fadeout");
	}
	return true;
}

void Level_Load(struct Game *game) {
	game->level.hp=1;
	game->level.cl_pos=0;
	game->level.bg_pos=0;
	game->level.fg_pos=0.2;
	game->level.st_pos=0.1;
	game->level.speed = 0;
	game->level.derpy_x = -0.2;
	game->level.derpy_y = 0.6;
	game->level.sheet_speed = tps(game, 60*2.4);
	game->level.sheet_tmp = 0;
	game->level.handle_input = false;
	game->level.obstracles = NULL;
	game->level.flying = false;
	game->level.meter_alpha=0;
	al_clear_to_color(al_map_rgb(0,0,0));
	if (game->level.current_level!=1) Moonwalk_Load(game);
	else {
		TM_Init(game);
		TM_AddBackgroundAction(&FadeIn, NULL, 0, "fadein");
		TM_AddDelay(1000);
		TM_AddQueuedBackgroundAction(&Welcome, NULL, 0, "welcome");
		TM_AddDelay(1000);
		TM_AddAction(&Walk, NULL, "walk");
		TM_AddAction(&Move, NULL, "move");
		TM_AddAction(&Stop, NULL, "stop");
		TM_AddDelay(1000);
		TM_AddAction(&Letter, NULL, "letter");
		TM_AddDelay(500);
		TM_AddQueuedBackgroundAction(&Accelerate, NULL, 0, "accelerate");
		TM_AddAction(&Fly, NULL, "fly");
		TM_AddDelay(5*1000);
		/* first part gameplay goes here */

		/* actions for generating obstracles should go here
		* probably as regular actions. When one ends, harder one
		* begins. After last one part with muffins starts.
		* Should obstracles themselves be handled as objects
		* on timeline? (probably not). Hmm... */
		TM_AddAction(&GenerateObstracles, NULL, "obstracles");
		TM_AddDelay(5*1000);

		/*
        // wings disappear, deccelerate, fall down
		// run
		// show Fluttershy's house

		// second part gameplay goes here
		//

		// cutscene goes here
		//
		*/

		TM_AddAction(&PassLevel, NULL, "passlevel");
	}
}

int Level_Keydown(struct Game *game, ALLEGRO_EVENT *ev) {
	if (ev->keyboard.keycode==ALLEGRO_KEY_ESCAPE) {
		game->level.music_pos = al_get_sample_instance_position(game->level.music);
		al_set_sample_instance_playing(game->level.music, false);
	}
	if (game->level.current_level!=1) Moonwalk_Keydown(game, ev);
	if (ev->keyboard.keycode==ALLEGRO_KEY_ESCAPE) {
		game->gamestate = GAMESTATE_PAUSE;
		game->loadstate = GAMESTATE_LEVEL;
		Pause_Load(game);
	}
	return 0;
}

void Level_ProcessLogic(struct Game *game, ALLEGRO_EVENT *ev) {
	if (game->level.current_level==1) TM_HandleEvent(ev);

}

void Level_Preload(struct Game *game) {
	game->level.derpy_sheets = NULL;
	game->level.derpy = NULL;
	Pause_Preload(game);
	RegisterDerpySpritesheet(game, "walk");
	RegisterDerpySpritesheet(game, "fly");
	RegisterDerpySpritesheet(game, "run");
	RegisterDerpySpritesheet(game, "stand");
	if (game->level.current_level!=1) Moonwalk_Preload(game);
	else {
		game->level.sample = al_load_sample( "data/levels/1/music.flac" );
		game->level.music = al_create_sample_instance(game->level.sample);
		al_attach_sample_instance_to_mixer(game->level.music, game->audio.music);
		al_set_sample_instance_playmode(game->level.music, ALLEGRO_PLAYMODE_LOOP);

		if (!game->level.sample){
			fprintf(stderr, "Audio clip sample not loaded!\n" );
			exit(-1);
		}
	}
	Level_PreloadBitmaps(game);
}

void Level_Unload(struct Game *game) {
	Pause_Unload_Real(game);
	al_destroy_sample_instance(game->level.music);
	al_destroy_sample(game->level.sample);
	if (game->level.current_level!=1) Moonwalk_Unload(game);
	else {
		TM_Destroy();
	}
	struct Obstracle *t = game->level.obstracles;
	if (t) {
		while (t->next) {
			if (t->prev) free(t->prev);
			t = t->next;
		}
		free(t);
	}
	Level_UnloadBitmaps(game);
	struct Spritesheet *tmp, *s = game->level.derpy_sheets;
	tmp = s;
	while (s) {
		tmp = s;
		s = s->next;
		free(tmp);
	}
}

void Level_UnloadBitmaps(struct Game *game) {
	al_destroy_bitmap(game->level.derpy);
	struct Spritesheet *tmp = game->level.derpy_sheets;
	while (tmp) {
		al_destroy_bitmap(tmp->bitmap);
		tmp = tmp->next;
	}
	if (game->level.current_level!=1) Moonwalk_UnloadBitmaps(game);
	else {
		al_destroy_bitmap(game->level.foreground);
		al_destroy_bitmap(game->level.background);
		al_destroy_bitmap(game->level.stage);
		al_destroy_bitmap(game->level.clouds);
		al_destroy_bitmap(game->level.welcome);
		al_destroy_bitmap(game->level.obst_bmps.pie);
		al_destroy_bitmap(game->level.meter_bmp);
		al_destroy_bitmap(game->level.meter_image);
	}
}

void Level_PreloadBitmaps(struct Game *game) {
	struct Spritesheet *tmp = game->level.derpy_sheets;
	while (tmp) {
		char filename[255] = { };
		sprintf(filename, "levels/derpy/%s.png", tmp->name);
		tmp->bitmap = LoadScaledBitmap(filename, al_get_display_height(game->display)*0.25*tmp->aspect*tmp->cols*tmp->scale, al_get_display_height(game->display)*0.25*tmp->rows*tmp->scale);
		tmp = tmp->next;
	}

	if (!game->level.derpy) SelectDerpySpritesheet(game, "stand");

	game->level.derpy = al_create_bitmap(al_get_bitmap_width(*(game->level.derpy_sheet))/game->level.sheet_cols, al_get_bitmap_height(*(game->level.derpy_sheet))/game->level.sheet_rows);
	
	if (game->level.current_level!=1) Moonwalk_PreloadBitmaps(game);
	else {
		/* TODO: handle strange display aspects */
		game->level.clouds = LoadScaledBitmap("levels/1/clouds.png", al_get_display_height(game->display)*4.73307291666666666667, al_get_display_height(game->display));
		game->level.foreground = LoadScaledBitmap("levels/1/foreground.png", al_get_display_height(game->display)*4.73307291666666666667, al_get_display_height(game->display));
		game->level.background = LoadScaledBitmap("levels/1/background.png", al_get_display_height(game->display)*4.73307291666666666667, al_get_display_height(game->display));
		game->level.stage = LoadScaledBitmap("levels/1/stage.png", al_get_display_height(game->display)*4.73307291666666666667, al_get_display_height(game->display));
		game->level.obst_bmps.pie = LoadScaledBitmap("menu/pie.png", al_get_display_width(game->display)*0.1, al_get_display_height(game->display)*0.1);
		game->level.welcome = al_create_bitmap(al_get_display_width(game->display), al_get_display_height(game->display)/2);
		al_set_target_bitmap(game->level.welcome);
		al_clear_to_color(al_map_rgba(0,0,0,0));
		al_draw_text_with_shadow(game->menu.font_title, al_map_rgb(255,255,255), al_get_display_width(game->display)*0.5, al_get_display_height(game->display)*0.1, ALLEGRO_ALIGN_CENTRE, "Level 1");
		al_draw_text_with_shadow(game->menu.font_subtitle, al_map_rgb(255,255,255), al_get_display_width(game->display)*0.5, al_get_display_height(game->display)*0.275, ALLEGRO_ALIGN_CENTRE, "Fluttershy");

		game->level.meter_image = LoadScaledBitmap("levels/meter.png", al_get_display_width(game->display)*0.075, al_get_display_width(game->display)*0.075*0.96470588235294117647);
		game->level.meter_bmp = al_create_bitmap(al_get_display_width(game->display)*0.2+al_get_bitmap_width(game->level.meter_image), al_get_bitmap_height(game->level.meter_image));

		al_set_target_bitmap(al_get_backbuffer(game->display));
	}
}
