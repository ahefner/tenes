#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include "ag.h"
#include "ff1_vars.h"

#define AG_MAX_BREAKPOINTS 16

void* ag_main (void *ctx);

/*** Shared Data ***/
pthread_t ag_thread;

enum FFGameMode
{
    FF_WUT = 0,
    FF_SHUTDOWN,
    FF_OVERWORLD_MODE,
    FF_MAP_MODE,
    FF_STORY_WAIT,
    FF_BATTLE_MAIN_COMMAND
} volatile game_mode = FF_WUT;

static do_ag_shutdown = false;

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
        //printf("PrepAndGetBattleMainCommand\n");
        game_mode = FF_BATTLE_MAIN_COMMAND;
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
    usleep(40000);
    controller_output = 0;
    usleep(40000);
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


struct tileprop
{
    unsigned walkable : 1;
    unsigned special : 4;
    unsigned teleport_mode : 2;
    unsigned explored : 1;
};

struct tileprop mapdata[64][256][256];

struct tileprop decode_sm_tile (byte tileprop)
{
    struct tileprop ret;
    memset(&ret, 0, sizeof(ret));
    ret.walkable = 1^(tileprop & 1);
    ret.special = (tileprop >> 1) & 0x0F;
    ret.teleport_mode = tileprop >> 6;
    ret.explored = 1;
    return ret;
}

/// Return tile index at x,y coordinate
byte get_sm_tile (unsigned xc, unsigned yc)
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

    return getbyte(var_tileset_prop + a);
    
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
    
    return b;
}

void print_map (byte index)
{
    // While I hope to treat the OW and SM maps in a unified way, SM
    // maps are limited to 64x64 (OW is 256x256) and I'm punting on
    // the extra effort to scan the map and bound its dimensions from
    // its contents.
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
                if (tile.special > 10) c = 'A' + tile.special;

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



void* ag_main (void *ctx)
{
    printf("AlphaGoat running...\n");
    memset(mapdata, 0, sizeof(mapdata));

    while (!do_ag_shutdown)
    {
        //fprintf(stderr, "game_mode = %u\n", (unsigned)game_mode);

        print_stuff();
        printf("\n");

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

            for (int y=-5; y<=6; y++)
            {
                for (int x=-6; x<=7; x++)
                {
                    byte tx = (sx+7+x) & 0x3F;
                    byte ty = (sy+7+y) & 0x3F;

                    //printf("%02X ", get_sm_tile((sx+7+x) & 0x3F, (sy+7+y) & 0x3F));
                    byte prop = get_sm_tile((sx+7+x) & 0x3F, (sy+7+y) & 0x3F);
                    struct tileprop here = decode_sm_tile(prop);
                    mapdata[cur_map][ty][tx] = merge_tileprop(mapdata[cur_map][ty][tx], here);
                }
                //printf("\n");
            }

            print_map(cur_map);
        }

        if (game_mode == FF_BATTLE_MAIN_COMMAND)
        {
            printf("Battle menu, character %u, target %u\n",
                   getbyte(var_btlcmd_curchar),
                   getbyte(var_btlcmd_target));
            //tap_a();
            usleep(1000 * 100);
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
            continue;
                                                         
        }

        game_mode = FF_WUT;
        usleep(1000 * 100);
    }

    
    printf("AlphaGoat put out to pasture\n");
    return 0;
}
