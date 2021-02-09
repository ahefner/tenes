
#define var_story_dropinput  0x07
#define var_inroom           0x0D // bit 7 is the actual inroom flag.  0xx1=entering room, 0xx2=entering locked room (different sprite vis), 0xx5=exiting room, 0xx6=exiting locked room
#define var_doorppuaddr      0x0E // 2 bytes, PPU address of door drawing work

#define var_tmp              0x10 // 16 bytes

#define var_mu_scoreptr      tmp+8  //2 bytes, shared tmp

#define var_dlgbox_row       tmp+0xC  // shared tmp
#define var_palcyc_mode      tmp+0xC  // shared tmp

#define var_joy              0x20
#define var_joy_ignore       0x21
#define var_joy_select       0x22
#define var_joy_start        0x23
#define var_joy_a            0x24
#define var_joy_b            0x25

#define var_sprindex         0x26

#define var_ow_scroll_x      0x27  // X scroll of OW in tiles
#define var_ow_scroll_y      0x28  // Y scroll in tiles

#define var_sm_scroll_x      0x29  // ditto, but for standard maps
#define var_sm_scroll_y      0x2A

#define var_mapdraw_x        0x2B
#define var_mapdraw_y        0x2C
#define var_mapflags         0x2D  // bit 0 set when in standard map.  bit 1 set to indicate column drawing instead of row drawing

#define var_scroll_y         0x2F  // Y scroll in tiles (16x16).  range=0-E

#define var_mapdraw_nty      0x30
#define var_mapdraw_ntx      0x31
#define var_mapdraw_job      0x32  // 0=no job, 1=draw attribs, 2=draw tiles

#define var_mg_slidedir      0x33  // shared
#define var_facing           0x33  // 1=R  2=L  4=D  8=U
#define var_move_speed       0x34  // pixels to move per frame (map)
#define var_move_ctr_x       0x35  // pixels between tiles (map movement -- 00-0F)
#define var_move_ctr_y       0x36  // ditto but for Y axis

#define var_menustall        0x37       // see MenuCondStall in bank F for explanation

#define var_theend_x         0x38
#define var_theend_y         0x39
#define var_theend_src       0x62
#define var_theend_drawbuf   0x6800     // 0x700 bytes!

#define var_box_x            0x38
#define var_box_y            0x39
#define var_box_wd           0x3C // shared
#define var_box_ht           0x3D // shared

#define var_dest_x           0x3A
#define var_dest_y           0x3B
#define var_dest_wd          0x3C
#define var_dest_ht          0x3D


#define var_image_ptr        0x3E // shared
#define var_text_ptr         0x3E // 2 bytes

#define var_spr_x            0x40
#define var_spr_y            0x41

#define var_mm_maprow        0x41 // shared

#define var_vehicle          0x42 //1=walking, 2=canoe, 4=ship, 8=airship
enum vehicle_types
{
    VEHICLE_WALKING = 1,
    VEHICLE_CANOE = 2,
    VEHICLE_SHIP = 4,
    VEHICLE_AIRSHIP = 8
};

#define var_inforest         0x43 // nonzero if in forest

#define var_tileprop         0x44 // 2 bytes

#define var_vehicle_next     0x46 // vehicle we're walking onto

#define var_vehchgpause      0x47 // forced pause when changing vehicles
#define var_cur_map          0x48
#define var_cur_tileset      0x49

#define var_cur_mapobj       0x4A // counter for updating which map object

#define var_music_track      0x4B
#define var_mu_chanprimer    0x4C
#define var_mu_chan          0x4D

#define var_entering_shop    0x50 // nonzero = about to enter shop
#define var_shop_id          0x51

#define var_tileprop_now     0x52 // special tile properties that we're on (tileprop isn't necessarily what we're standing on)

#define var_ow_tile          0x53

#define var_ppu_dest         0x54  // 2 bytes

#define var_dlgflg_reentermap  0x56  // flag to indicate the map needs re-entering due to dialogue (Bahamut/class change)
#define var_cur_bank         0x57
#define var_ret_bank         0x58

#define var_format_buf       0x60  // 7 bytes (5A-60) -- must not cross page bound

#define var_shutter_a        0x61  // shared
#define var_shutter_b        0x62  // shared

#define var_lu_cursor        0x61  // shared
#define var_lu_cursor2       0x62  // shared
#define var_lu_mode          0x63  // shared
#define var_lu_joyprev       0x64  // shared

#define var_mm_bplo          0x61  // shared
#define var_mm_bphi          0x62  // shared

#define var_intro_ataddr     0x62  // shared
#define var_intro_atbyte     0x63  // shared
#define var_intro_color      0x64  // shared

#define var_dlg_itemid       0x61  // shared
#define var_equipmenu_tmp    0x61  // shared
#define var_joy_prevdir      0x61
#define var_cursor           0x62
#define var_cursor_max       0x63
#define var_cursor2          0x63  // shared (secondary cursor)

#define var_mg_slidespr      0x64  // shared, 3 bytes

#define var_namecurs_x       0x64
#define var_shopcurs_x       0x64  // shared
#define var_eq_modecurs      0x64  // shared
#define var_hp_recovery      0x64
#define var_mp_required      0x65
#define var_namecurs_y       0x65
#define var_shopcurs_y       0x65  // shared
#define var_story_credits    0x65  // shared

#define var_minimap_ptr      0x64  // shared, 2 bytes

#define var_submenu_targ     0x66  // shared with shop_type
#define var_shop_type        0x66
#define var_story_page       0x66  // shared
#define var_equipoffset      shop_type  // MUST be shared with shop_type

#define var_story_timer      0x67  // shared
#define var_draweq_stradd    0x67  // shared
#define var_char_index       0x67
#define var_mm_pixrow        0x67  // shared
#define var_talkobj          0x67  // shared -- object you're talking to on SM

#define var_sm_player_x      0x68  // player X/Y position on standard map.  Only used for NPC collision detection
#define var_sm_player_y      0x69

#define var_btlformation     0x6A
#define var_enCHRpage        0x6B

#define var_altareffect      0x6C  // flag to indicate altar effect is to occur (screen shaking, monochrome diagonal window thing)

#define var_dlgmusic_backup  0x7C  // backup music track for restoring music after the dialogue box changes it
#define var_dlgsfx           0x7D  // flag to indicate to play a sound effect after opening dialogue box.  0=no sfx, 1=fanfare, else=treasure

#define var_sq2_sfx          0x7E
#define var_descboxopen      0x7F

#define var_btlptr           0x80  // ??? don't know how big

#define var_lvlup_curexp         0x80       // 2 byte pointer to character's current EXP
#define var_lvlup_exptoadv       0x82       // 2 byte pointer to EXP needed to advance
#define var_lvlup_chmagic        0x84       // 2 byte pointer to character's magic data
#define var_lvlup_chstats        0x86       // 2 byte pointer to character's OB stats


#define var_battlereward         0x88       // 3 bytes.  Note that while this var is 3 bytes, this stop behaving properly
                                //  if rewards ever exceed the 2-byte boundary, since the game assumes you
                                //  will never receive more than 65535 XP/GP in any one battle.

#define var_btl_ib_charstat_ptr  0x80
#define var_btl_ob_charstat_ptr  0x82

#define var_btldraw_blockptrstart  0x8C
#define var_btldraw_blockptrend    0x8E

#define var_btltmp           0x90  // 16 bytes ?

#define var_btl_entityptr_ibram      0x90
#define var_btl_entityptr_obrom      0x92
#define var_btl_magdataptr           0x98

#define var_btldraw_src      0x90   // source data
#define var_btldraw_dst      0x92   // destination pointer
#define var_btldraw_subsrc   0x94   // source pointer of substring
#define var_btldraw_max      0x9F   // maximum characters to draw


#define var_btlsfx_frontseat     0x90   // where battle sfx data is stored in zero page
#define var_btlsfx_backseat      0x6D97 // where it is stored when not in zero page (it swaps between the two)

#define var_btlsfx_framectr      0x94   // The total frame counter for the entire sound effect
#define var_btlsfxsq2_len        0x98
#define var_btlsfxnse_len        0x99
#define var_btlsfxsq2_framectr   0x9A
#define var_btlsfxnse_framectr   0x9B
#define var_btlsfxsq2_ptr        0x9C
#define var_btlsfxnse_ptr        0x9E




#define var_framecounter     0xF0  // 2 bytes!

#define var_npcdir_seed      0xF4  // RNG seed for determining direction for NPCs to walk

#define var_battlestep       0xF5
#define var_battlestep_sign  0xF6
#define var_battlecounter    0xF7
#define var_battlerate       0xF8  // X/256 chance of a random encounter occuring (SM only apparently)

#define var_startintrocheck  0xF9
#define var_respondrate      0xFA

#define var_NTsoft2000       0xFD  // same as soft2000, but used to track coarse NT scroll
#define var_unk_FE           0xFE
#define var_soft2000         0xFF

#define var_unk_0100         0x0100

#define var_tmp_hi           0x0110  // 3? bytes

#define var_oam              0x0200  // 256 bytes -- must be on page bound
#define var_oam_y            var_oam
#define var_oam_t            var_oam+1
#define var_oam_a            var_oam+2
#define var_oam_x            var_oam+3

#define var_puzzle           0x0300  // shared
#define var_str_buf          0x0300  // 0x39 bytes at least -- buffer must not cross page
#define var_item_box         0x0300  // 0x20? bytes -- shares space with str_buf

#define var_ptygen           0x0300  // 0x40 bytes, shared
/*                              
 ptygen_class    ptygen
 ptygen_name    = ptygen+2
 ptygen_name_x  = ptygen+6
 ptygen_name_y  = ptygen+7
 ptygen_class_x = ptygen+8
 ptygen_class_y = ptygen+9
 ptygen_spr_x   = ptygen+0xA
 ptygen_spr_y   = ptygen+0xB
 ptygen_box_x   = ptygen+0xC
 ptygen_box_y   = ptygen+0xD
 ptygen_curs_x  = ptygen+0xE
 ptygen_curs_y  = ptygen+0xF
*/

#define var_shop_charindex   0x030A  // shared
#define var_shop_spell       0x030B  // shared
#define var_shop_curitem     0x030C  // shared
#define var_shop_curprice    0x030E  // 2 shared bytes

#define var_cur_pal          0x03C0       // 32 bytes
#define var_inroom_pal       var_cur_pal+0x20 // 16 bytes
#define var_tmp_pal          0x03F0       // 16 bytes

#define var_tileset_data     0x0400  // 0x400 bytes -- must be on page bound

#define var_mm_drawbuf       0x0500  //0x100 bytes, shared, should be on page bound, but don't think it's absolutely required
#define var_mm_mapbuf        0x0600  // same
#define var_mm_mapbuf2       0x0700  // same

#define var_tileset_prop     var_tileset_data  // 256 bytes, 2 bytes per tile
/*                              
tsa_ul          = tileset_data+0x100  // 128 bytes
tsa_ur          = tileset_data+0x180  // 128
tsa_dl          = tileset_data+0x200  // 128
tsa_dr          = tileset_data+0x280  // 128
tsa_attr        = tileset_data+0x300  // 128
load_map_pal    = tileset_data+0x380  // 0x30  (shared with draw_buf -- hence only for loading)
*/

#define var_draw_buf         0x0780  // 128
/*
draw_buf_ul     = draw_buf
draw_buf_ur     = draw_buf + 0x10
draw_buf_dl     = draw_buf + 0x20
draw_buf_dr     = draw_buf + 0x30
draw_buf_attr   = draw_buf + 0x40
draw_buf_at_hi  = draw_buf + 0x50
draw_buf_at_lo  = draw_buf + 0x60
draw_buf_at_msk = draw_buf + 0x70
*/

#define var_unk_07D2         0x07D2



#define UNSRAM           0x6000  // 0x400 bytes
#define var_sram             0x6400  // 0x400 bytes
#define var_sram_checksum    var_sram + 0xFD
#define var_sram_assert_55   var_sram + 0xFE
#define var_sram_assert_AA   var_sram + 0xFF


#define var_ship_vis         UNSRAM + 0x00
#define var_ship_x           UNSRAM + 0x01
#define var_ship_y           UNSRAM + 0x02

#define var_airship_vis      UNSRAM + 0x04
#define var_airship_x        UNSRAM + 0x05
#define var_airship_y        UNSRAM + 0x06

#define var_bridge_vis       UNSRAM + 0x08
#define var_bridge_x         UNSRAM + 0x09
#define var_bridge_y         UNSRAM + 0x0A

#define var_canal_vis        UNSRAM + 0x0C
#define var_canal_x          UNSRAM + 0x0D
#define var_canal_y          UNSRAM + 0x0E

#define unsram_ow_scroll_x    UNSRAM + 0x10
#define unsram_ow_scroll_y    UNSRAM + 0x11

#define var_has_canoe             UNSRAM + 0x12 // (not to be confused with item_canoe)
#define var_unsram_vehicle        UNSRAM + 0x14

#define var_bridgescene      UNSRAM + 0x16  // 00=hasn't happened yet. 01=happens when move is complete, 80=already has happened

#define var_gold             UNSRAM + 0x1C   // 3 bytes
#define var_items            UNSRAM + 0x20

#define var_item_lute        var_items + 0x01
#define var_item_crown       var_items + 0x02
#define var_item_crystal     var_items + 0x03
#define var_item_herb        var_items + 0x04
#define var_item_mystickey   var_items + 0x05
#define var_item_tnt         var_items + 0x06
#define var_item_adamant     var_items + 0x07
#define var_item_slab        var_items + 0x08
#define var_item_ruby        var_items + 0x09
#define var_item_rod         var_items + 0x0A
#define var_item_floater     var_items + 0x0B
#define var_item_chime       var_items + 0x0C
#define var_item_tail        var_items + 0x0D
#define var_item_cube        var_items + 0x0E
#define var_item_bottle      var_items + 0x0F
#define var_item_oxyale      var_items + 0x10
#define var_item_canoe       var_items + 0x11
#define var_item_orb_start   var_items + 0x12
#define var_orb_fire         var_item_orb_start + 0
#define var_orb_water        var_item_orb_start + 1
#define var_orb_air          var_item_orb_start + 2
#define var_orb_earth        var_item_orb_start + 3
#define var_item_qty_start   var_item_orb_start + 4
#define var_item_tent        var_item_qty_start + 0
#define var_item_cabin       var_item_qty_start + 1
#define var_item_house       var_item_qty_start + 2
#define var_item_heal        var_item_qty_start + 3
#define var_item_pure        var_item_qty_start + 4
#define var_item_soft        var_item_qty_start + 5
#define var_item_stop        var_item_qty_start + 6

#define var_ch_stats         UNSRAM + 0x0100  // MUST be on page bound.  Each character allowed 0x40 bytes, so use 00,40,80,C0 to index ch_stats

#define var_ch_class         var_ch_stats + 0x00
#define var_ch_ailments      var_ch_stats + 0x01
#define var_ch_name          var_ch_stats + 0x02  // 4 bytes

#define var_ch_exp           var_ch_stats + 0x07  // 3 bytes
#define var_ch_curhp         var_ch_stats + 0x0A  // 2 bytes
#define var_ch_maxhp         var_ch_stats + 0x0C  // 2 bytes

#define var_ch_str           var_ch_stats + 0x10
#define var_ch_agil          var_ch_stats + 0x11
#define var_ch_int           var_ch_stats + 0x12
#define var_ch_vit           var_ch_stats + 0x13
#define var_ch_luck          var_ch_stats + 0x14

#define var_ch_exptonext     var_ch_stats + 0x16  // 2 bytes -- only for user display, not actually used.
#define var_ch_weapons       var_ch_stats + 0x18  // 4
#define var_ch_armor         var_ch_weapons + 4  // 4

#define var_ch_substats      var_ch_stats + 0x20
#define var_ch_dmg           var_ch_substats + 0x00
#define var_ch_hitrate       var_ch_substats + 0x01
#define var_ch_absorb        var_ch_substats + 0x02
#define var_ch_evade         var_ch_substats + 0x03
#define var_ch_resist        var_ch_substats + 0x04
#define var_ch_magdef        var_ch_substats + 0x05

#define var_ch_level         var_ch_stats + 0x26        // OB this is 0 based, IB this is 1 based

#define var_game_flags       UNSRAM + 0x0200  // must be on page bound

#define GMFLG_OBJVISIBLE 1
#define GMFLG_EVENT 2
#define GMFLG_TCOPEN 4

// Out of battle, spell data is stored stupidly so valid values are
// only 00-08, where 01 to 08 are actual spells and 00 is 'empty'.
// Each spell is conceptually in a "slot" that belongs to each spell
// level.  Therefore, both CURE and LAMP are stored as '01' because
// they're both the first spell in their level, but because they're in
// a different level slot, the game distinguishes them.  In battle,
// fortunately, that is thrown out the window (why does it do it at
// all?) and the spells are stored in a logical 1-based index where
// the level simply doesn't matter.

#define var_ch_magicdata     UNSRAM + 0x0300  // must be on page bound
  /* ch_spells       = ch_magicdata */
  /* ch_mp           = ch_magicdata + 0x20 */
  /* ch_curmp        = ch_mp + 0x00 */
  /* ch_maxmp        = ch_mp + 0x08 */

#define var_btl_chstats          0x6800  // 0x12 bytes per character
#if 0
  btlch_slotindex   = 0x00
  btlch_class       = 0x01
  btlch_ailments    = 0x02       // appears not to be used?  OB always seems to be used
  btlch_hp          = 0x03       // appears not to be used?  OB always seems to be used
  btlch_hitrate     = 0x05
  btlch_magdef      = 0x06
  btlch_evade       = 0x07
  btlch_absorb      = 0x08
  btlch_dmg         = 0x09
  btlch_elemresist  = 0x0A
  btlch_numhitsmult = 0x0B
  btlch_numhits     = 0x0C
  btlch_category    = 0x0D           // always 0 since players have no category assigned
  btlch_elemweak    = 0x0E           // always 0 (players can't have weaknesses)
  btlch_critrate    = 0x0F
  btlch_wepgfx      = 0x10
  btlch_wepplt      = 0x11
#endif
  
#define var_btl_turnorder        0x6848         // 0xD entries (9 enemies + 4 characters)
  
// Battle stuff
#if 0
    MATHBUF_HITCHANCE           = 0
    MATHBUF_BASEDAMAGE          = 1
    MATHBUF_NUMHITS             = 2
    MATHBUF_MAGRANDHIT          = 2
    MATHBUF_CATEGORY            = 3
    MATHBUF_ELEMENT             = 4
    MATHBUF_RANDHIT             = 4
    MATHBUF_DMGCALC             = 5
    MATHBUF_CRITCHANCE          = 6
    MATHBUF_AILMENTCHANCE       = 7
    MATHBUF_MAGDEFENDERHP       = 0x12
    MATHBUF_DEFENDERHP          = 0x13
    MATHBUF_MAGDEFENDERMAXHP    = 0x15
    MATHBUF_TOTALDAMAGE         = 0x16
#endif
    
#define var_btl_mathbuf  0x6856     // 0x14 bytes?, 2 byte pairs, used as buffers for mathematical routines
/*
    math_hitchance      = btl_mathbuf + (MATHBUF_HITCHANCE*2)   // 0x6856
    math_basedamage     = btl_mathbuf + (MATHBUF_BASEDAMAGE*2)  // 0x6858
    math_numhits        = btl_mathbuf + (MATHBUF_NUMHITS*2)     // 0x685A
    math_magrandhit     = btl_mathbuf + (MATHBUF_MAGRANDHIT*2)  // 0x685A
    math_category       = btl_mathbuf + (MATHBUF_CATEGORY*2)    // 0x685C not really math... but whatever
    math_element        = btl_mathbuf + (MATHBUF_ELEMENT*2)     // 0x685E not really math... but whatever
    math_randhit        = btl_mathbuf + (MATHBUF_RANDHIT*2)     // 0x685E
    math_dmgcalc        = btl_mathbuf + (MATHBUF_DMGCALC*2)     // 0x6860
    math_critchance     = btl_mathbuf + (MATHBUF_CRITCHANCE*2)  // 0x6862
    math_ailmentchance  = btl_mathbuf + (MATHBUF_AILMENTCHANCE*2)//0x6864
*/    
#define var_eob_gp_reward                0x6876
#define var_eob_exp_reward               0x6878
    

#define var_battle_ailmentrandchance     0x6866

#define var_battle_hitsconnected         0x686A     // number of hits actually connected
#define var_battle_critsconnected        0x686B
#define var_btl_attacker_strength        0x686C
#define var_btl_attacker_category        0x686D
#define var_btl_attacker_element         0x686E
#define var_btl_attacker_hitrate         0x686F
#define var_btl_attacker_numhitsmult     0x6870
#define var_btl_attacker_numhits         0x6871
#define var_btl_attacker_critrate        0x6872
#define var_btl_attacker_attackailment   0x6873

#define var_btl_defender_category        0x6876
#define var_btl_defender_elemweakness    0x6877
#define var_btl_defender_evade           0x6878
#define var_btl_defender_absorb          0x6879
#define var_btl_defender_magdef          0x687A
#define var_btl_defender_elemresist      0x687B
#define var_btl_defender_hp              var_btl_mathbuf + (MATHBUF_DEFENDERHP*2)  // 0x687C -- treated as part of math buffer by some code

#define var_btl_attacker_graphic         0x6880     // the graphic used for an attack
#define var_btl_attacker_varplt          0x6881     // The variable palette color used for an attack

#define var_battle_totaldamage           btl_mathbuf + (MATHBUF_TOTALDAMAGE*2)  // 0x6882 -- treated as part of math buffer by some code

// Battle stuff for magical attacks
/*
btlmag_spellconnected       = 0x685C
btlmag_defender_ailments    = 0x686D
btlmag_effect               = 0x686E
btlmag_hitrate              = 0x6870
btlmag_defender_magdef               = 0x6872
btlmag_defender_unknownRawInit0      = 0x6873
btlmag_effectivity          = 0x6874
btlmag_defender_elemweakness         = 0x6876
btlmag_defender_elemresist           = 0x6877
btlmag_element              = 0x6878

btlmag_attacker_unk686C     = 0x686C // Enemy:  ailments              Player:  ailments
btlmag_attacker_unk6875     = 0x6875 // Enemy:  ai                    Player:  damage
btlmag_attacker_unk6879     = 0x6879 // Enemy:  high byte of GP       Player:  class
btlmag_attacker_unk6883     = 0x6883 // Enemy:  0                     Player:  level
btlmag_attacker_unk6884     = 0x6884 // Enemy:  damage                Player:  hit rate

btlmag_defender_hp          = btl_mathbuf + (MATHBUF_MAGDEFENDERHP*2)  // 0x687A
btlmag_defender_numhitsmult = 0x687D
btlmag_defender_morale      = 0x687E
btlmag_defender_absorb      = 0x687F
btlmag_defender_hpmax       = btl_mathbuf + (MATHBUF_MAGDEFENDERMAXHP*2)  // 0x6880
btlmag_defender_strength    = 0x6882

btlmag_defender_evade       = 0x6885  // shared with battle_defender_index?  Is that not used for btlmag ?
btlmag_defender_category    = 0x6886

btlmag_fakeout_defindex     = 0x6D95 // See Battle_DoTurn in bank C for a description of
btlmag_fakeout_ailments     = 0x6D94 //   what these 'fakeout' vars are and how/why they're used
btlmag_fakeout_defplayer    = 0x6D96
                            
                                    // These two are kind of redundant
battle_attacker_index       = 0x6884 // ?? redundant??  why not just use btl_attacker?
battle_defender_index       = 0x6885 // same... but this is necessary for output!  See Battle_DoTurn in bank C!!

battle_defenderisplayer     = 0x6887 // nonzero if player is defending, zero if enemy is defending 
                                    //   important for output!  See Battle_DoTurn in bank C

btl_attacker_ailments       = 0x6888
btl_defender_ailments       = 0x6889 // important for output!

btl_rngstate = 0x688A    // State of RNG used for in-battle

btltmp_divLo = 0x688B
btltmp_divHi = 0x688C
btltmp_divV  = 0x688D

btl_curturn         = 0x688E         // current turn (index for btl_turnorder)
*/

//  Buffers to hold character commands for battle.  These must be contiguious in memory
//  due to the way memory is cleared.  These buffers also contain a bit of redundant data.
//
//  btl_charcmdbuf contains 3 bytes (padded to 4) per character:
//    byte 0 = command
//    byte 1 = spell effect ID  (used for DRINK/MAGIC/ITEM).  FF if no effect
//    byte 2 = target.  8x are player targets 0x are enemy targets.  FF=target all enemies, FE=target all players.
//
//  Commands can be the following:
//    00 = no command -- if surprised/asleep/stunned
//    01 = no command -- if dead
//    02 = no command -- if stone
//    04 = attack
//    08 = drink potion
//    10 = use item
//    20 = run   ('target' would be the actual character running)
//    40 = magic
//    
//
//  btl_charcmditem contains 1 byte per character:  the ID of the item they're using.
//    This is only used when the command is '10'
//
//  btl_charcmdconsumetype contains 1 byte per character.  It will be 01 for magic and 02 for DRINK.
//       unused for other commands.
//
//  btl_charcmdconsumeid contains 1 byte per character.  If will be the potion index
//       for drink, or the spell level for magic

#define var_btl_charcmdbuf           0x688F
// btl_charcmditem         = btl_charcmdbuf+0x10        // 0x689F
// btl_charcmdconsumetype  = btl_charcmditem+4         // 0x68A3
// btl_charcmdconsumeid    = btl_charcmdconsumetype+4  // 0x68A7

#define var_char_order_buf           0x689F

#if 0
// These next 5 vars are all in temp memory, and are mostly just used for 
//  passing into BattleDraw8x8Sprite
btl8x8spr_x = 0x68AF // X coord
                    //  +1 used in drawing code as original position
btl8x8spr_y = 0x68B1 // Y coord
                    //  +1 used in drawing code as original position
btl8x8spr_a = 0x68B3 // attribute
btl8x8spr_t = 0x68B4 // tile ID
btl8x8spr_i = 0x68B5 // slot to draw to (00-3F)
  
btl_tmpindex = 0x68B3    // temporary holder for a current index
btl_tmpchar =  0x68B4    // temporary holder for a 0-based character index

btltmp_multA = 0x68B3    // shared
btltmp_multB = 0x68B4    // shared
btltmp_multC = 0x68B5


btltmp_boxleft   = 0x68B3
btltmp_boxcenter = 0x68B4
btltmp_boxright  = 0x68B5

btl_input        = 0x68B3

btl_soft2000 = 0x68B7    // soft copy of 0x2000 used in battles
btl_soft2001 = 0x68B8    // soft copy of 0x2001 used in battles

btlbox_blockdata    = 0x68BA
                    // 0x68CF   ???

btl_msgbuffer = 0x691E   // 0x180 bytes  (0x0C rows * 0x20 bytes per row)
                        // this buffer contains on-screen tiles to be drawn to ppu0x2240
                        // (note only 0x19 bytes are actually drawn, the remaining 7 bytes are padding)

btl_msgdraw_hdr     = 0x6A9E
btl_msgdraw_x       = 0x6A9F
btl_msgdraw_y       = 0x6AA0
btl_msgdraw_width   = 0x6AA1
btl_msgdraw_height  = 0x6AA2
btl_msgdraw_srcptr  = 0x6AA1  // shared
                    // 0x6AA2    above +1

btl_msgdraw_blockcount = 0x6AA3      // the number of blocks drawn

eobbox_slotid       = 0x6AA6
eobbox_textid       = 0x6AA7

btlinput_prevstate  = 0x6AA6     // prev state for input
inputdelaycounter   = 0x6AA7     // counter to delay multiple-input processing when holding a direction

btl_animatingchar   = 0x6AA9     // the character currently being animated (0-3)
#endif

#define var_btlcurs_x           0x6AAA     // battle cursor X position (menu position, not pixel position)
#define var_btlcurs_y           0x6AAB     // battle cursor Y position (menu position, not pixel position)

#if 0
btlcurs             = 0x6AAA
btlcurs_max         = 0x6AAB     // highest value for the cursor
btlcurs_positions   = 0x6AAC     // ?? bytes, 2 bytes per entry, each entry is the pixel coord of where the
                                //   cursor should be drawn when its item is selected.

btl_drawflagsA = 0x6AD1  // bits 0-3 = set to indicate character should be drawn as dead
                        // bit    4 = set to draw battle cursor
                        // bit    5 = set to draw weapon attack graphic
                        // bit    6 = set to draw magic graphic & flash BG.

btl_drawflagsB = 0x6AD2  // bits 0-4 = set to indicate character should be drawn as stone

btl_chardrawinfo = 0x6AD3        //0x10 bytes, 4 bytes for each character
    btl_chardraw_x      = btl_chardrawinfo+0
    btl_chardraw_y      = btl_chardrawinfo+1
    btl_chardraw_gfxset = btl_chardrawinfo+2
    btl_chardraw_pose   = btl_chardrawinfo+3
    
btlcursspr_x        = 0x6AE3
btlcursspr_y        = 0x6AE4
btlattackspr_x      = 0x6AE5
btlattackspr_y      = 0x6AE6

btlattackspr_t      = 0x6AE7     // indicate which tile to draw for the weapon graphic
btlattackspr_pose   = 0x6AE8     // for weapons, 0 or 8 to indicate whether or not to flip it
                                // for magic, 0 or ?4? to indicate which frame to draw
btlattackspr_gfx    = 0x6AE9     // copied to 't' prior to drawing.  Indicates which graphic to use
btlattackspr_wepmag = 0x6AEA     // 0 for drawing the weapon, 1 for drawing the magic

btlattackspr_hidell = 0x6AED     // nonzero to hide the lower-left tile of the attack graphic
                                //   This is done for the "behind the back" frame of weapon swing animation.
btlattackspr_nodraw = 0x6AEE     // nonzero to hide the weapon/magic sprite entirely.  This is
                                //   Used when a non-BB player attacks without any weapon equipped
                                //   Also used when using ITEMs to supress the magic flashing effect.

btltmp_targetlist   = 0x6AEF     // temporary buffer (9 entries) containing possible targets

btl_combatboxcount  = 0x6AF8     // the number of combat boxes that have been drawn

btl_unfmtcbtbox_buffer = 0x6AFA  // 0x80 bytes total, 0x10 bytes for each combat box.
                                // houses the unformatted text for each combat box.
                                // Additional bytes are used for other areas
#endif

#define var_btlcmd_curchar      0x6B7A     // the current character inputting battle commands (0-3)
#define var_btlcmd_target       0x6B7B     // the current enemy slot that is being targetted


#if 0
btlcmd_magicgfx     = 0x6B7E     // 2 bytes per character.  [0] = graphic to draw, [1] = palette to use

btl_result          = 0x6B86 //   0 = keep battling
                            //   1 = party defeated
                            //   2 = all enemies defeated
                            //   3 = party ran
                            // 0xFF = wait for 2 seconds after fadeout before exiting (chaos defeated?)

btl_usepalette      = 0x6B87 // 0x20 bytes - the palette that is actually displayed (after fade effects)

btl_followupmusic   = 0x6BA7 // song to play in battle after current song finishes.  Moved to music_track
                            //    once music_track has its high bit set  (does this ever happen?)

btl_charattrib      = 0x6BA8 // attributes to use when drawing charcters in battle  (4 bytes, 1 for each)

btl_responddelay    = 0x6BAC

btl_strikingfirst   = 0x6BAE // nonzero if players are striking first.  Zero otherwise

btl_potion_heal     = 0x6BAF // battle containers for Heal/Pure potions.  Stored separately because
btl_potion_pure     = 0x6BB0 //  it can fall out of sync with the ACTUAL items (if a character trying
                            //  to use one dies, for example)

battle_bank     = 0x6BB1  // The bank to jump back to for setting up battles
btl_smallslots  = 0x6BB2             // Number of small enemy slots available
btl_largeslots  = btl_smallslots+1  // Number of large slots available.  Must immediately follow smallslots

btl_enemyeffect = 0x6BB6     // 0 to draw expolosion graphics as the effect
                            //   nonzero to erase the enemy as the effect

btl_enemyIDs =    0x6BB7  // 9 entries of enemy IDs
btl_enemygfxplt = 0x6BC0  // 9 entries of enemy graphic and palette assignment (graphic in high 2 bits, plt in low bit)

btl_enemyroster = 0x6BC9  // 4 bytes of enemy IDs printed in the main battle menu, showing enemies in the fight

btl_attacker_alt    = 0x6BCD // An EXTREMELY redundant and stupid copy of btl_attacker

btl_randomplayer    = 0x6BCF // set by GetRandomPlayerTarget  (0-3)

btl_enemystats =  0x6BD3  // 0x14 bytes per enemy - data does NOT match how it is stored in ROM
    en_romptr       = 0x00  // 2 bytes - pointer to enemy stats in ROM
    en_hp           = 0x02  // 2 bytes
    en_defense      = 0x04
    en_numhitsmult  = 0x05
    en_ailments     = 0x06
    en_aimagpos     = 0x07
    en_aiatkpos     = 0x08
    en_morale       = 0x09
    en_evade        = 0x0A
    en_strength     = 0x0B
    en_ai           = 0x0C
    en_exp          = 0x0D  // 2 bytes
    en_gp           = 0x0F  // 2 bytes
    en_enemyid      = 0x11
    en_unknown12    = 0x12  // low byte of HP max
    en_unknown13    = 0x13  // not initialized?  probably suppoed to be high byte of HP max

btl_tmppltassign = 0x6C88    // temporary value to assign palette to enemies in a formation

btl_attacker            = 0x6C89
btl_defender            = 0x6C8A
btl_combatboxcount_alt  = 0x6C8B // ANOTHER combatbox counter... this is totally redundant
btl_attackid            = 0x6C8C // >= 0x42 for enemy attacks

btlmag_magicsource      = 0x6C8F // 0=magic, 1=drink, 2=item
btlmag_ailment_orig     = 0x6C90 // A backup of 
btl_battletype  = 0x6C92     // 0=9 small, 1=4 large, 2=mix, 3=fiend, 4=chaos
btl_enemycount  = 0x6C93     // count of the number of enemies being generated for a battle
btltmp_attr     = 0x6C94     // 0x40 bytes of attribute data for the battle setup

// temporary space used by the lineup menu
lutmp_ch_stats  = 0x6C00
lutmp_ch_magic  = 0x6D00


bigstr_buf      = 0x6C00   // 0x81 bytes?

btl_stringoutputbuf = 0x6CD4 // output buffer where decoded strings are printed

explode_min_x   = 0x6D14
explode_min_y   = 0x6D15
explode_max_x   = 0x6D16
explode_max_y   = 0x6D17
explode_count   = 0x6D18

btltmp_altmsgbuffer     = 0x6D19
btltmp_attackerbuffer   = 0x6D2C

// ????          = 0x6D14   // action buffer?  0x20 bytes?  contents for combat boxes are placed here?

btl_palettes    = 0x6D34   // 0x20 bytes

btl_stringbuf   = 0x6D54   // 0x20 byte buffer to contain string data for printing

btltmp_backseat = 0x6D74   // 0x10 byte buffer -- backup of btltmp

// btlsfx_backseat   = 0x6D97

btlmag_playerhitsfx = 0x6DA7     // sound effect to play when magic hits player
btltmp_smallslotpos = 0x6DB0
#endif

#define var_btl_formdata    0x6D84   // 0x10 bytes (formation data as appears in ROM)
#define    var_btlform_type     ( var_btl_formdata+0x0 )   // battle type (high 4 bits) -- low 4 bits are pattern table
#define    var_btlform_engfx    ( var_btl_formdata+0x1 )   // graphic assignment (2 bits per enemy)
#define    var_btlform_enids    ( var_btl_formdata+0x2 )   // enemy IDs (4 bytes)
#define    var_btlform_enqty    ( var_btl_formdata+0x6 )   // enemy quantities (4 bytes)
#define    var_btlform_plts     ( var_btl_formdata+0xA )   // palettes for this battle (2 bytes)
#define    var_btlform_surprise ( var_btl_formdata+0xC )   // surprise rate
#define    var_btlform_enplt    ( var_btl_formdata+0xD )   // enemy palette assign (in high 4 bits)
#define    var_btlform_norun    ( var_btlform_enplt )     // no run flag (in low bit)
#define    var_btlform_enqtyB   ( var_btl_formdata+0xE )   // enemy quantities for B formation (2 bytes)




#define var_mapobj           0x6F00   // 0x100 bytes -- page
#define var_mapobj_id       mapobj + 0x00  // rearranging these is ill advised
#define mapobj_flgs     mapobj + 0x01  //  because the loader is pretty rigid
#define mapobj_physX    mapobj + 0x02  //  flags:  0x80=inroom 0x40=don't move
#define mapobj_physY    mapobj + 0x03
#define mapobj_gfxX     mapobj + 0x04
#define mapobj_gfxY     mapobj + 0x05
#define mapobj_ctrX     mapobj + 0x06
#define mapobj_ctrY     mapobj + 0x07
#define mapobj_spdX     mapobj + 0x08
#define mapobj_spdY     mapobj + 0x09
#define mapobj_rawid    mapobj + 0x0A
#define mapobj_movectr  mapobj + 0x0B
#define mapobj_face     mapobj + 0x0C
#define mapobj_pl       mapobj + 0x0D   // bit 7 = talking to player (changes facing), other bits = being shoved by player
#define mapobj_tsaptr   mapobj + 0x0E

#define var_mapdata          0x7000   // must be on 0x1000 byte bound (ie:  pretty much unmovable)

#define var_mm_decorchr      0x7000   // 0x300 bytes -- should be on page bound, shared
#define var_mm_titlechr      0x7300   // 0x280 bytes -- should be on page bound, shared
