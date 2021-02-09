#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "ag.h"
#include "ff1_vars.h"

#define AG_MAX_BREAKPOINTS 16

void* ag_main (void *ctx);

/*** Shared Data ***/
pthread_t ag_thread;

enum FFGameMode
{
    FF_WUT = 0,
    FF_SHUTDOWN = 1,
    FF_OVERWORLD_MODE = 2,
    FF_MAP_MODE = 3,
    FF_STORY_WAIT = 4,
    FF_BATTLE_MAIN_COMMAND = 5,
    FF_POPUP_WAIT = 6
} volatile game_mode = FF_WUT;

static bool do_ag_shutdown = false;

static unsigned controller_output = 0;

/*** Called from emulator thread ***/

void ag_init (void)
{
    if (pthread_create(&ag_thread, 0, ag_main, 0))
    {
        abort();
    }
}

void ag_note_pc (word pc)
{
//    printf("ag_note_pc=%hu\n",pc);
    switch (pc)
    {
    case 0xC23C:
        //printf("Overworld: ready\n");
        game_mode = FF_OVERWORLD_MODE;
        break;

    case 0xC9C4:
        //printf("Standard map: ready\n");
        game_mode = FF_MAP_MODE;
        break;

    case 0xB923:
        //printf("Story wait\n");
        game_mode = FF_STORY_WAIT;
        break;

    case 0x9A2C:
        printf("PrepAndGetBattleMainCommand\n");
        // PrepAndGetBattleMainCommand is the wrong place for this!
        // Is it? Yes. This is hit once while we wait for main menu
        // input. Ideally we want something called continuously while
        // waiting for input.
        //game_mode = FF_BATTLE_MAIN_COMMAND;
        break;

    case 0x94E6:
        printf("BattleSubMenu_Fight\n");
        break;

    case 0x94F5:
        printf("BattleSubMenu_Magic\n");
        break;

        //case 0x95F5:                /* Vanilla address? */
    case 0x95EC:                /* Randomizer address */
        printf("BattleSubMenu_Drink\n");
        break;

    case 0x9679:
        printf("BattleSubMenu_Item\n");
        break;

    case 0x9496:
        printf("InputCharacterBattleCommand\n");
        break;

    case 0xA352:
        printf("Battle_DoTurn  [$A352 :: 0x32362]  entity=%X\n", nes.cpu.A);
        // output: battle_defenderisplayer, battle_defender_index, btl_defender_ailments
        break;

    case  0xA4BA:
        printf("PlayerAttackEnemy_Physical  [$A4BA :: 0x324CA]  attacker=%X defender=%X\n", nes.cpu.A, nes.cpu.X);
        break;

    case 0xA67B:
        printf("DoPhysicalAttack\n");
        break;

    case 0xD641:
    case 0xD64D:
        game_mode = FF_POPUP_WAIT;
        printf("popup wait1 %X\n", pc);
        // Dialog has just opened. Game waits for buttons to be depressed.
        controller_output = 0;
        break;

    case 0xD653:
        game_mode = FF_POPUP_WAIT;
        // Dialog then waits for a button press
        printf("popup wait2 %X\n", pc);
        controller_output = 1;
        break;
        

    case 0xD602:
        printf("entered dialog\n");
//        nes.cpu.Trace = 1;
//        exit(0);
        break;

    default:
        break;
    }
}

unsigned ag_get_gamepad()
{
    return controller_output;
}

void ag_shutdown()
{
    fprintf(stderr, "ag_shutdown called...\n");
    game_mode = FF_SHUTDOWN;
    do_ag_shutdown = true;
    if (pthread_join(ag_thread,0))
    {
        perror("pthread_join");
    }
    fprintf(stderr, "ag_shutdown: joined.\n");
}
/*** AG thread ***/

void sleep_frames (unsigned n)
{
    usleep(1000000 / 60 * n);
}

static byte getbyte(unsigned addr)
{
    if (addr < 0x2000)
    {
        addr &= 0x7FF;
        return nes.ram[addr];
    }

    if ((addr >= 0x6000) && (addr < 0x8000))
    {
        return nes.save[addr-0x6000];
    }

    return 0;                   /* ?? */
}

// Get word little endian
static word getword (unsigned addr)
{
    return getbyte(addr) | (getbyte(addr+1)<<8);
}

static unsigned gettriple (unsigned addr)
{
    return getbyte(addr) | (getbyte(addr+1)<<8) | ((unsigned)getbyte(addr+2)<<16);
}

struct point { int x, y; };

struct point get_ow_coord()
{
    return (struct point){ getbyte(var_ow_scroll_x) + 7, getbyte(var_ow_scroll_y) + 7 };
}

static unsigned get_gold()
{
    return gettriple(var_gold);
}

struct point *find_ship()
{
    static struct point p = {0,0};
    if (getbyte(var_ship_vis))
    {
        p.x = getbyte(var_ship_x);
        p.y = getbyte(var_ship_y);
        return &p;
    }
    else
    {
        return 0;
    }
}

struct point *find_airship()
{
    static struct point p = {0,0};
    if (getbyte(var_airship_vis))
    {
        p.x = getbyte(var_airship_x);
        p.y = getbyte(var_airship_y);
        return &p;
    }
    else
    {
        return 0;
    }
}

struct point *find_bridge()
{
    static struct point p = {0,0};
    if (getbyte(var_bridge_vis))
    {
        p.x = getbyte(var_bridge_x);
        p.y = getbyte(var_bridge_y);
        return &p;
    }
    else
    {
        return 0;
    }
}

struct point *find_canal()
{
    static struct point p = {0,0};
    if (getbyte(var_canal_vis))
    {
        p.x = getbyte(var_canal_x);
        p.y = getbyte(var_canal_y);
        return &p;
    }
    else
    {
        return 0;
    }
}

bool has_canoe() { return 0 != getbyte(var_has_canoe); }

static void print_stuff()
{
    struct point ow = get_ow_coord();
    printf("overworld coord %02X,%02X\n", ow.x, ow.y);
    printf("gold: %u\n", get_gold());
    printf("vehicle: %02X\n", getbyte(var_vehicle));
    if (has_canoe()) printf("has canoe\n");

    struct point *ship = find_ship();
    struct point *airship = find_airship();
    struct point *canal = find_canal();
    struct point *bridge = find_bridge();

    if (ship) printf("ship at %02X %02X\n", ship->x, ship->y);
    if (airship) printf("airship at %02X %02X\n", airship->x, airship->y);
    if (bridge) printf("bridge at %02X %02X\n", bridge->x, bridge->y);
    if (canal) printf("canal at %02X %02X\n", canal->x, canal->y);
}

void tap_a()
{
    /* ~2 frames down, ~2 frames up, because some parts of the game
     * (notably the combat menus) seemingly don't register single
     * frame button taps.*/
    printf("Pushed A\n");
    controller_output = 1;
    sleep_frames(2);
    controller_output = 0;
    sleep_frames(2);
}

void print_party()
{
    for (int i=0; i<4; i++)
    {
        unsigned off = 0x40 * i;
        printf("Player %i. class %i. Level %u, HP %u/%u. %u XP.\n",
               i,
               getbyte(var_ch_class+off),
               getbyte(var_ch_level+off),
               getword(var_ch_curhp+off),
               getword(var_ch_maxhp+off),
               gettriple(var_ch_exp+off));
    }
}

struct game_smtile_prop
{
    int valid : 1;
};

static byte ror8nc (byte x)       /* rotate inside byte, not through carry! */
{
    return (x >> 1) | ((x&1) << 7);
}

uint64_t ai_timestamp = 0;

enum SpecialTypes
{
    SPEC_NONE = 0,
    SPEC_DOOR = 1,
    SPEC_LOCKED = 2,
    SPEC_CLOSEROOM = 3,
    SPEC_TREASURE = 4,
    SPEC_BATTLE = 5,
    SPEC_DAMAGE = 6,
    SPEC_CROWN = 7,
    SPEC_CUBE = 8,
    SPEC_4ORBS = 9,
    SPEC_USEROD = 10,
    SPEC_USELUTE = 11,
    SPEC_EARTHORB = 12,
    SPEC_FIREORB = 13,
    SPEC_WATERORB = 14,
    SPEC_AIRORB = 15
};

enum TeleportMode
{
    TELE_EXIT = 3,
    TELE_NORM = 2,
    TELE_WARP = 1,
    TELE_NONE = 0
};

struct tileprop
{
    unsigned walkable : 1;
    unsigned special : 4;
    unsigned teleport_mode : 2;
    unsigned explored : 1;
    unsigned arg : 8;           /* Argument depending on special type */
};

/// SM data
struct tileprop mapdata[64][64][64]; /* map index, y, x */

unsigned sm_unexplored_dist[64][64][64];
uint64_t sm_updated_timestamp[64];
uint64_t sm_need_timestamp[64];

struct tileprop decode_sm_tile (byte tileprop)
{
    struct tileprop ret;
    memset(&ret, 0, sizeof(ret));
    ret.walkable = 1^(tileprop & 1);
    ret.special = (tileprop >> 1) & 0x0F;
    ret.teleport_mode = tileprop >> 6;
    ret.explored = 1;
    ret.arg = 0;
    return ret;
}

/// Return tile property at x,y coordinate
byte get_sm_tile (unsigned xc, unsigned yc, unsigned propindex)
{
    byte a,x,y;
    
    /* LDA sm_scroll_y       ; get Y scroll */
    a = yc;
    /* CLC */
    /* ADC #$07              ; add 7 to get player's Y position */
    /* AND #$3F              ; wrap around edges of map */

    //a = (a+7)&0x3F;
    a &= 0x3F;
    
    /* TAX                   ; put the y coord in X */
    x = a;

    /* LSR A                 ; right shift y coord by 2 (the high byte of *64) */
    /* LSR A */
    a = a >> 2;
    
    /* ORA #>mapdata         ; OR to get the high byte of the tile entry in the map */
    a |= (var_mapdata>>8);
    /* STA tmp+1             ; store to source pointer */
    byte tmp_high = a;

    /* TXA                   ; restore y coord */
    a = x;
    /* ROR A                 ; rotate right by 3 and mask out the high 2 bits. */
    /* ROR A                 ;  same as a left-shift-by-6 (*64) */
    /* ROR A */
    /* AND #$C0 */
    a = a << 6;
    /* STA tmp               ; store as low byte of source pointer (points to start of row) */
    byte tmp_low = a;

    /* LDA sm_scroll_x       ; get X scroll */
    /* CLC */
    /* ADC #$07              ; add 7 for player's X position */
    /* AND #$3F              ; wrap around map boundaries */
    a = xc;
    a &= 0x3F;
    
    /* TAY                   ; put in Y for indexing this row of map data */
    y = a;

    /* LDA (tmp), Y          ; get the tile from the map */
    word tmpw = tmp_low | (tmp_high<<8);
    //printf("%i,%i tmpw=%X [%X + %i] = %02X\n", xc, yc, tmpw, tmpw, y, getbyte(tmpw+y));
           
    a = getbyte(tmpw+y);

    /* ASL A                 ; *2  (2 bytes of properties per tile) */
    a = a << 1;
    /* TAX                   ; put index in X */
    x = a;

    /* LDA tileset_prop, X   ; get the first property byte */

    return getbyte(var_tileset_prop + a + propindex);
    
    /* AND #TP_SPEC_MASK     ; isolate the 'special' bits */
    /* STA tileprop_now      ; and record them!     */
}

struct tileprop merge_tileprop(struct tileprop a, struct tileprop b)
{
    if (!a.explored) return b;
    if (!b.explored) return a;

    if (memcmp(&a, &b, sizeof(a))) {
        printf("merge_tileprop: mismatch?\n");
    }

    // Is there anything I'll ever want to do here, or am I just being overly fancy?
    // (the latter)
    return b;
}

void print_map (byte index)
{
    for (int y=0; y<40 /*64*/; y++)
    {
        for (int x=0; x<64; x++)
        {
            char c = ' ';
            struct tileprop tile = mapdata[index][y][x];
            if (tile.explored)
            {
                if (tile.walkable) c = '_';
                else c = 'X';

                if (tile.special && tile.special < 10) c = '0' + tile.special;
                if (tile.special > 10) c = 'A' + tile.special - 10;

                if (tile.special == 5) c = '.';

                if (tile.teleport_mode == 1) c = '^';
                if (tile.teleport_mode == 2) c = 'T';
                if (tile.teleport_mode == 3) c = '#';

            }
            
            putchar(c);
        }
        putchar('\n');
    }
}

void print_dist_map (byte index)
{
    const double SCALE = 1.0/30.0;

    assert(index < 64);
    for (int y=0; y<40 /*64*/; y++) {
        for (int x=0; x<64; x++) {
            unsigned raw_dist = sm_unexplored_dist[index][y][x];
            unsigned dist = lround(ceil(raw_dist*SCALE));
            unsigned c = ' ';
            if (mapdata[index][y][x].explored)
            {
                if (dist >= 0) c = '0' + dist;
                if (dist > 9) c = 'A' + dist-10;
                if (c > 'Z') c = c + 'a' - 'A';
                if (c > 'z') c = '!';

                if (!mapdata[index][y][x].walkable)
                {
                    c = '*';
                }
            }
            putchar(c);
        }
        putchar('\n');
    }

}

unsigned min_u (unsigned a, unsigned b) { return a<b? a : b; }
unsigned max_u (unsigned a, unsigned b) { return a<b? b : a; }

// If not a goal, returns NOT_A_GOAL. Othewise, lower return values
// indicate higher priority.
#define NOT_A_GOAL 0xFEFEFEFE
//#define NOT_A_GOAL 0xFFFFFFFF

struct goalstuff
{
    unsigned some_number;
    bool walking_only;
    bool action_goal;
};

struct goalstuff is_goal_tile (unsigned map_index, unsigned y, unsigned x)
{
    assert(map_index < 0x40);

    struct tileprop prop = mapdata[map_index&0x3F][y&0x3F][x&0x3F];

    // Walk toward unexplored areas
    if (!prop.explored) {
        struct goalstuff tmp = { .some_number = 0, .walking_only = true, .action_goal = false };
        return tmp;
    }

    // Walk toward unopened chests
    if (prop.special == SPEC_TREASURE) {
        byte flags = getbyte(var_game_flags + prop.arg);
        //printf("is goal? treasure %X flags=%X\n", prop.arg, flags);
        if (!(flags & GMFLG_TCOPEN)) {
            // Walk to unopened chests
            struct goalstuff tmp = { .some_number = 0, .walking_only = false, .action_goal = true };
            return tmp;
        }
    }

    struct goalstuff tmp = { .some_number = NOT_A_GOAL, .walking_only = true, .action_goal = false };
    return tmp;
}

/// Returns cost for stepping on to this tile
unsigned get_tile_cost (struct tileprop prop)
{
    const unsigned COST_WALK_SAFE = 1; /* Walk one tile with no encounters */
    const unsigned COST_WALK_UNSAFE = 30; /* Walk one tile with chance of encounter - probably should be higher*/
    const unsigned COST_WALK_SPIKED = COST_WALK_UNSAFE * 15;

    unsigned cost = COST_WALK_SAFE;

    if (prop.special == SPEC_BATTLE) {
        cost = (prop.arg & 128)? COST_WALK_SPIKED : COST_WALK_UNSAFE;
    }

    return cost;
}

bool is_passable (struct tileprop prop)
{
    return (prop.walkable && !prop.teleport_mode)
        || prop.special == SPEC_DOOR
//        || prop.special == SPEC_CLOSEROOM
//        || (prop.special == SPEC_LOCKED && getbyte(var_item_mystickey)) 
        ;
}

void hack_iterate_pathing (byte map_index)
{
    //fprintf(stderr, "map iteration for %u\n", map_index);
    
    if (sm_need_timestamp[map_index] != sm_updated_timestamp[map_index])
    {
        memset(&sm_unexplored_dist[map_index], 254, sizeof(sm_unexplored_dist[map_index]));
        sm_need_timestamp[map_index] = sm_updated_timestamp[map_index];
        /* WRONG-ISH: Update timestamp when recompute is complete? Or
         not, fuck it.  But the actual point was to know when we were
         done and could go back to sleep.*/
    }
    
    for (unsigned y=0; y<64; y++) {
        for (unsigned x=0; x<64; x++) {
            unsigned *outptr = &sm_unexplored_dist[map_index][y][x];
            struct tileprop prop = mapdata[map_index][y][x];

            unsigned tmp = *outptr;
            unsigned cost = get_tile_cost(prop);
            
            if (is_passable(prop)) {
                    
                // Is a goal adjacent?
                tmp = min_u(tmp, is_goal_tile(map_index,y,x+1).some_number);
                tmp = min_u(tmp, is_goal_tile(map_index,y,x-1).some_number);
                tmp = min_u(tmp, is_goal_tile(map_index,y+1,x).some_number);
                tmp = min_u(tmp, is_goal_tile(map_index,y-1,x).some_number);
                                    
                // Add cost and propagate..
                tmp = min_u(tmp, cost + sm_unexplored_dist[map_index][y][(x+1)&0x3F]);
                tmp = min_u(tmp, cost + sm_unexplored_dist[map_index][y][(x-1)&0x3F]);
                tmp = min_u(tmp, cost + sm_unexplored_dist[map_index][(y+1)&0x3F][x]);
                tmp = min_u(tmp, cost + sm_unexplored_dist[map_index][(y-1)&0x3F][x]);
            }

            *outptr = tmp;
        }
    }
}

void follow_gradient (byte map_index, byte y, byte x)
{
    assert(map_index < 64);
    assert(x < 64);
    assert(y < 64);

    unsigned here = sm_unexplored_dist[map_index][y][x];

    controller_output = 0;
    if (sm_unexplored_dist[map_index][(y-1)&0x3F][x] < here) controller_output = 0x10;
    if (sm_unexplored_dist[map_index][(y+1)&0x3F][x] < here) controller_output = 0x20;
    if (sm_unexplored_dist[map_index][y][(x-1)&0x3F] < here) controller_output = 0x40;
    if (sm_unexplored_dist[map_index][y][(x+1)&0x3F] < here) controller_output = 0x80;

    if (controller_output) printf("Seeking... ctrl=%X\n", controller_output);
}

// facing          = $33  ; 1=R  2=L  4=D  8=U
void face_and_act__up()
{
    controller_output = 0x10;
    if (getbyte(var_facing) == 8) controller_output |= 1;
}

void face_and_act__down()
{
    controller_output = 0x20;
    if (getbyte(var_facing) == 4) controller_output |= 1;
}

void face_and_act__left()
{
    controller_output = 0x40;
    if (getbyte(var_facing) == 2) controller_output |= 1;
}

void face_and_act__right()
{
    controller_output = 0x80;
    if (getbyte(var_facing) == 1) controller_output |= 1;
}

void follow_gradient_and_do_shit (byte map_index, byte y, byte x)
{
    controller_output = 0;
    printf("fgados...\n");

    if (is_goal_tile(map_index,y-1,x).action_goal) {        
        face_and_act__up();
        return;
    }

    if (is_goal_tile(map_index,y+1,x).action_goal) {
        face_and_act__down();
        return;
    }

    if (is_goal_tile(map_index,y,x-1).action_goal) {
        face_and_act__left();
        return;
    }

    if (is_goal_tile(map_index,y,x+1).action_goal) {
        face_and_act__right();
        return;
    }

    // Otherwise..
    follow_gradient(map_index, y, x);
}

#define zeromem(var) memset(&var, 0, sizeof(var))

void* ag_main (void *ctx)
{
    printf("AlphaGoat running...\n");

    zeromem(mapdata);
    zeromem(sm_unexplored_dist);
    zeromem(sm_updated_timestamp);
    zeromem(sm_need_timestamp);

    while (!do_ag_shutdown)
    {
        controller_output = 0;
        ai_timestamp++;

        // fprintf(stderr, "game_mode = %u\n", (unsigned)game_mode);

        //print_stuff();
        //printf("\n");

        if (game_mode == FF_OVERWORLD_MODE)
        {
            struct point p = get_ow_coord();
            printf("On overworld at %02X,%02X\n", p.x, p.y);
        }

        if (game_mode == FF_STORY_WAIT)
        {
            printf("Make it go away!\n");
            tap_a();
        }

        if (game_mode == FF_MAP_MODE)
        {
            unsigned sx = getbyte(var_sm_scroll_x);
            unsigned sy = getbyte(var_sm_scroll_y);

            byte cur_map = getbyte(var_cur_map);
            if (cur_map > 63) continue;
            printf("Map mode cur_map=%u at %02X,%02X\n", cur_map, sx, sy);

            bool need_pathing_update = false;
            for (int y=-5; y<=6; y++)
            {
                for (int x=-6; x<=7; x++)
                {
                    byte tx = (sx+7+x) & 0x3F;
                    byte ty = (sy+7+y) & 0x3F;

                    //printf("%02X ", get_sm_tile((sx+7+x) & 0x3F, (sy+7+y) & 0x3F));
                    byte prop = get_sm_tile((sx+7+x) & 0x3F, (sy+7+y) & 0x3F, 0);
                    byte arg = get_sm_tile((sx+7+x) & 0x3F, (sy+7+y) & 0x3F, 1);
                    struct tileprop here = decode_sm_tile(prop);
                    here.arg = arg;

                    // HMM: Should I fetch relevant gameflags and store them in the tileprop?
                    if (here.special == SPEC_TREASURE)
                    {
                        printf("Treasure %02X,%02X: %u flags=%02X\n", tx, ty, arg, getbyte(var_game_flags + arg));
                    }
                    
                    struct tileprop merged = merge_tileprop(mapdata[cur_map][ty][tx], here);
                    if (memcmp(&mapdata[cur_map][ty][tx], &merged, sizeof(struct tileprop)))
                    {
                        mapdata[cur_map][ty][tx] = merged;
                        need_pathing_update = true;
                    }
                }
                //printf("\n");
            }

            static unsigned wait_counter = 0;
            if (!(++wait_counter & 0x01F)) {
                // Periodically reupdate pathing just in case we missed a trigger
                need_pathing_update = true;
            }

            if (need_pathing_update)
            {
                sm_need_timestamp[cur_map] = ai_timestamp;
                printf("need pathing update!\n");
            }

            for (int i=0; i<1024; i++) hack_iterate_pathing(cur_map);

            //print_map(cur_map);
            print_dist_map(cur_map);

            follow_gradient_and_do_shit(cur_map, (sy+7)&0x3F, (sx+7)&0x3F);
        }

        if (game_mode == FF_BATTLE_MAIN_COMMAND)
        {
            printf("Battle menu, character %u, target %u\n",
                   getbyte(var_btlcmd_curchar),
                   getbyte(var_btlcmd_target));
            //tap_a();

//            game_mode = 0;

            // Note that there's a separate copy of character stats in
            // battle. Although HP is updated in the main ones, battle
            // AI should probably look at the battle stats (var_btlch_stats.*)
            // vars.
            print_party();

            for (int i=0; i<9; i++)
            {
                const unsigned addr = 0x6BD3 + i*0x14;
                printf("enemy %2i: %i HP id=%u\n",
                       i, getword(addr+2), getbyte(addr+0x11));
            }

            if (getbyte(var_btlform_norun) & 1) printf("UNRUNNABLE!\n");

            printf("battle cursor %i,%i\n", getbyte(var_btlcurs_x), getbyte(var_btlcurs_y));
            usleep(1000 * 100);
            continue;
        }

        if (game_mode == FF_POPUP_WAIT)
        {
            printf("Popup wait\n");

            if (controller_output) {
                controller_output = 0;
                sleep_frames(1);
            }
            
            controller_output = 1;
            sleep_frames(1);
            controller_output = 0;

            unsigned sleep_interval = 2;
            while (game_mode == FF_POPUP_WAIT)
            {
                printf("still waiting... gm=%u\n", game_mode);
                sleep_frames(sleep_interval);
/*
                //controller_output = 1;
                sleep_frames(1);
                controller_output = 0;
                sleep_interval *= 2;
                if (sleep_interval > 60) sleep_interval = 60;
*/
            }

            printf("finally closed!\n");
        }

        game_mode = FF_WUT;
        usleep(1000 * 100);
    }

    
    printf("AlphaGoat put out to pasture\n");
    return 0;
}
