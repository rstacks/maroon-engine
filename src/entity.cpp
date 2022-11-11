#include <math.h>

#include "entity.hpp"
#include "game-map.hpp"
#include "render.hpp"

float g_dt;
DL_HEAD(g_entities);
int g_key_down[256];

static const float g_gravity = 2.0F;
static int8_t cam_seek = 0;

const entity_meta g_entity_metas[COUNTOF_EM] = {
	[EM_CAPTAIN] = {
		.mask = {
			{
				20.0F / TILE_LEN, 
				0.0F
			},
			{
				52.0F / TILE_LEN,
				32.0F / TILE_LEN
			}
		},
		.def_anim = ANIM_CAPTAIN_IDLE
	},
	[EM_CRABBY] = {
		.mask = {
			{
				17.0F / TILE_LEN,
				6.0F / TILE_LEN,
			},
			{
				58.0F / TILE_LEN,
				28.0F / TILE_LEN
			}
		},
		.def_anim = ANIM_CRABBY_IDLE
	}
};

static entity *g_captain = NULL;

/**
 * set_animation - Set current animation for entity
 * @e: Entity to change
 * @anim: Animation to set
 *
 * NOTE: Use change_animation to avoid animation reset in the case the 
 * new animation is the same as the old.
 */
static void set_animation(entity *e, const anim *new_anim)
{
	e->cur_anim = new_anim;
	e->anim_time = ANIM_DT;
	e->sprite = new_anim->start;
}

entity *create_entity(int tx, int ty, uint8_t em)
{
	entity *e;
	const entity_meta *meta;

	e = (entity *) xmalloc(sizeof(*e));

	dl_push_back(&g_entities, &e->node);
	e->em= em;
	e->spawn.x = tx;
	e->spawn.y = ty;

	e->pos.x = tx;
	e->pos.y = ty;
	e->vel.x = 0.0F;
	e->vel.y = 0.0F;

	e->flipped = 0;

	meta = g_entity_metas + em;
	set_animation(e, g_anims + meta->def_anim);

	switch (em) {
	case EM_CRABBY:
		e->vel.x = 2.0F;
		break;
	}
	return e;
}

void destroy_entity(entity *e)
{
	dl_del(&e->node);
	free(e);
}

void start_entities(void)
{
	uint8_t **r;
	int y;

	r = g_gm->rows;
	for (y = 0; y < g_gm->h; y++) {
		uint8_t *c;
		int x;

		c = *r; 
		for (x = 0; x < g_gm->w; x++) {
			int em;

			em = g_tile_to_em[*c];
			if (em != EM_INVALID) {
				entity *e = create_entity(x, y, em); 

				if (em == EM_CAPTAIN) {
					g_captain = e;
				}

				*c = 0;
			}
			c++;
		}
		r++;
	}

	if (g_captain == NULL) {
		printf("No player on this level!\n");
	}
}

/**
 * change_animation() - Change animation
 * @e: Enity to change the animation of
 * @new_anim: New animation to change to
 *
 * NOTE: Will not reset animation timer if new
 * animation is the same as the old.
 */
static void change_animation(entity *e, const anim *new_anim)
{
	if (e->cur_anim != new_anim) {
		set_animation(e, new_anim);
	}
}

/**
 * update_animation() - Update animation for entity
 * @e: Entity to update animation of
 */
static void update_animation(entity *e)
{
	e->anim_time -= g_dt;
	
	if (e->anim_time <= 0.0F) {
		if (e->sprite < e->cur_anim->end) {
			e->sprite++;
		} else {
			e->sprite = e->cur_anim->start;
		}

		e->anim_time = ANIM_DT;
	}
}

/**
 * update_physics() - Update physics of entity
 * @e: Entity to update physics of
 */
static void update_physics(entity *e)
{
	e->pos.x += e->vel.x * g_dt;
	e->pos.y += e->vel.y * g_dt;
}

/**
 * update_captain() - Update captain specific behavoir
 * @e: Captain to update
 */
static void update_captain(entity *e)
{
	/*simple left and right captain movement*/
	const entity_meta *meta;
	v2 captain_vel;
	v2 offset;
	float captain_speed;
	bool touch_below, touch_above;
	bool touch_left, touch_right;

	meta = g_entity_metas + e->em;

	/*check for tiles colliding with captain on all four sides*/
	offset = e->pos + meta->mask.tl;
	touch_below = get_tile(offset.x, 
			offset.y + meta->mask.br.y) ||
			get_tile(offset.x + meta->mask.tl.x,
			offset.y + meta->mask.br.y);
	touch_above = get_tile(offset.x, 
			offset.y + meta->mask.tl.y) ||
			get_tile(offset.x + meta->mask.tl.x,
			offset.y + meta->mask.tl.y);
	touch_left = get_tile(offset.x - 
			meta->mask.tl.x / 5, offset.y + 
			meta->mask.tl.y) || get_tile(offset.x - 
			meta->mask.tl.x / 5, offset.y + 
			meta->mask.br.y * 0.75);
	touch_right = get_tile(offset.x + 
			meta->mask.tl.x * 1.25, offset.y + 
			meta->mask.tl.y) || get_tile(offset.x + 
			meta->mask.tl.x * 1.25, offset.y + 
			meta->mask.br.y * 0.75);
	
	captain_vel.x = 0.0F;
	captain_vel.y = 0.0F;
	captain_speed = 4.0F;

	if (g_key_down['W'] && !touch_above) {
		captain_vel.y = -1.0F;
	}

	if (g_key_down['S'] && !touch_below) {
		captain_vel.y = 1.0F;
	}

	if (g_key_down['A'] && !touch_left) {
		captain_vel.x = -1.0F;
	}
	
	if (g_key_down['D'] && !touch_right) {
		captain_vel.x = 1.0F;
	}

	e->vel = captain_vel * captain_speed;
	e->vel.y += g_gravity;

	/**
	 * Note(Lenny) - collision detection and 
	 * resolution code should go here
	 * the current method is not ideal
	 */
	if (touch_below) {
		e->vel.y -= g_gravity;
	}

	/* figuring out which aniemation to use*/
	if (fabsf(e->vel.x) > 0.05F) {
		change_animation(e, &g_anims[ANIM_CAPTAIN_RUN]);
	} else {
		change_animation(e, &g_anims[ANIM_CAPTAIN_IDLE]);
	}

	if(!g_key_down[VK_SPACE]){
		e->flipped = true;
	}else{
		e->flipped = false;
	}

	// if (e->vel.x < 0) {
	// 	e->flipped = true;
	// }

	// if (e->vel.x > 0) {
	// 	e->flipped = false;
	// }

	/* camera follow */
	box b = g_entity_metas[e->em].mask;
	v2 cap_pos = e->pos + b.tl;
	float bound = 2.0F;

	float dist_to_end = (g_cam.w - cap_pos.x) + g_cam.x;
	float dist_to_start = (g_cam.w - dist_to_end);

	/*TODO(Lenny): Make the camera slow down as it gets closer to cap*/
	float catch_up_speed = 4.0F;
	if (cam_seek) {

		g_cam.x += catch_up_speed * cam_seek * g_dt;

		if (dist_to_end >= g_cam.w / 2 && cam_seek == 1) {
			cam_seek = 0;
			catch_up_speed = catch_up_speed;
		} else if (dist_to_start >= g_cam.w / 2 && cam_seek == -1) {
			cam_seek = 0;
			catch_up_speed = catch_up_speed;
		}
	}

	if (dist_to_end < bound && cam_seek == false) {
		cam_seek = 1;
	} else if (dist_to_start < bound && cam_seek == false) {
		cam_seek = -1;
	}

	if (g_cam.x < 0.0F) {
		g_cam.x = 0.0F;
		cam_seek = 0;
	}

	bound_cam();
}

static void update_crabby(entity *e)
{
	/* crabby movement */
	const entity_meta *meta;
	v2 offset;
	uint8_t tile_id_left; 
	uint8_t tile_id_right;

	/* for player detection/charging */
	int wall_collide;
	int player_detected;

	meta = g_entity_metas + e->em;
	offset = e->pos + meta->mask.tl;

	float crabby_speed = 1.0f;

	tile_id_left = get_tile(offset.x, 
			offset.y + meta->mask.br.y + 0.1F);
	tile_id_right = get_tile(offset.x + 
			(meta->mask.br.x - meta->mask.tl.x), 
			offset.y + meta->mask.br.y + 0.1F);

	if (tile_id_left == TILE_SOLID || tile_id_left != TILE_GRASS) {
		e->vel.x = crabby_speed;
	} else if (tile_id_right == TILE_SOLID || 
			tile_id_right != TILE_GRASS) {
		e->vel.x = -crabby_speed;
	} 

	/**
	 * detecting the player and charging at them
	 * 
	 * variables will be set appropriately once tile and
	 * player detection is implemented for crabby
	 * 
	 * -1 indicates left, 1 indicates right, and 0 indicates
	 * no detection
	 */
	wall_collide = 0;
	player_detected = 0;

	if (player_detected != 0) {
		/* crabby moves left if player is left and no wall is left*/
		if (player_detected == -1 && wall_collide != -1) {
			e->vel.x = -3.0F;
		}
		/* crabby moves right if player is right and no wall is right */
		if (player_detected == 1 && wall_collide != 1) {
			e->vel.x = 3.0F;
		}
	}


	/* selecting the animation */
	if (fabsf(e->vel.x) > 0.05F) {
		change_animation(e, &g_anims[ANIM_CRABBY_RUN]);
	} else {
		change_animation(e, &g_anims[ANIM_CRABBY_IDLE]);
	}


	if (e->vel.x < 0) {
		e->flipped = false;
	}

	if (e->vel.x > 0) {
		e->flipped = true;
	}
}

static void update_specific(entity *e)
{
	switch (e->em) {
	case EM_CAPTAIN:
		update_captain(e);
		break;
	case EM_CRABBY:
		update_crabby(e);
		break;
	}
}

void update_entities(void)
{
	entity *e, *n;

	dl_for_each_entry_s (e, n, &g_entities, node) {
		update_specific(e);
		update_animation(e);
		update_physics(e);
	}
}

void end_entities(void)
{
	clear_entities();
}

void clear_entities(void)
{
	entity *e, *n;

	dl_for_each_entry_s (e, n, &g_entities, node) {
		uint8_t *tile;

		tile = g_gm->rows[e->spawn.y] + e->spawn.x;
		*tile = g_em_to_tile[e->em];
		destroy_entity(e);
	}
}

